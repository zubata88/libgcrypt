/* cipher-selftest.c - Helper functions for bulk encryption selftests.
 *	Copyright © 2013 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser general Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#ifdef HAVE_SYSLOG
# include <syslog.h>
#endif /*HAVE_SYSLOG*/

#include "types.h"
#include "g10lib.h"
#include "cipher.h"
#include "bufhelp.h"
#include "cipher-selftest.h"

#ifdef HAVE_STDINT_H
# include <stdint.h> /* uintptr_t */
#elif defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#else
/* In this case, uintptr_t is provided by config.h. */
#endif

/* Helper macro to force alignment to 16 bytes.  */
#ifdef HAVE_GCC_ATTRIBUTE_ALIGNED
# define ATTR_ALIGNED_16  __attribute__ ((aligned (16)))
#else
# define ATTR_ALIGNED_16
#endif


/* Run the self-tests for <block cipher>-CBC-128, tests bulk CBC
   decryption.  Returns NULL on success. */
const char *
_gcry_selftest_helper_cbc_128 (const char *cipher,
                               gcry_cipher_setkey_t setkey_func,
			       gcry_cipher_encrypt_t encrypt_one,
			       gcry_cipher_bulk_cbc_dec_t bulk_cbc_dec,
			       const int nblocks, const int blocksize,
			       const int context_size)
{
  int i, offs;
  unsigned char *ctx, *plaintext, *plaintext2, *ciphertext, *iv, *iv2, *mem;
  unsigned int ctx_aligned_size, memsize;

  static const unsigned char key[16] ATTR_ALIGNED_16 = {
      0x66,0x9A,0x00,0x7F,0xC7,0x6A,0x45,0x9F,
      0x98,0xBA,0xF9,0x17,0xFE,0xDF,0x95,0x22
    };

  /* Allocate buffers, align elements to 16 bytes.  */
  ctx_aligned_size = context_size + 15;
  ctx_aligned_size -= ctx_aligned_size & 0xf;

  memsize = ctx_aligned_size + (blocksize * 2) + (blocksize * nblocks * 3) + 16;

  mem = gcry_calloc (1, memsize);
  if (!mem)
    return "failed to allocate memory";

  offs = (16 - ((uintptr_t)mem & 15)) & 15;
  ctx = (void*)(mem + offs);
  iv = ctx + ctx_aligned_size;
  iv2 = iv + blocksize;
  plaintext = iv2 + blocksize;
  plaintext2 = plaintext + nblocks * blocksize;
  ciphertext = plaintext2 + nblocks * blocksize;

  /* Initialize ctx */
  setkey_func (ctx, key, sizeof(key));

  /* Test single block code path */
  memset (iv, 0x4e, blocksize);
  memset (iv2, 0x4e, blocksize);
  for (i = 0; i < blocksize; i++)
    plaintext[i] = i;

  /* CBC manually.  */
  buf_xor (ciphertext, iv, plaintext, blocksize);
  encrypt_one (ctx, ciphertext, ciphertext);
  memcpy (iv, ciphertext, blocksize);

  /* CBC decrypt.  */
  bulk_cbc_dec (ctx, iv2, plaintext2, ciphertext, 1);
  if (memcmp (plaintext2, plaintext, 16))
    {
      gcry_free (mem);
#ifdef HAVE_SYSLOG
      syslog (LOG_USER|LOG_WARNING, "Libgcrypt warning: "
              "%s-128-CBC test failed (plaintext mismatch)", cipher);
#endif
      return "selftest for 128 bit CBC failed - see syslog for details";
    }

  if (memcmp (iv2, iv, 16))
    {
      gcry_free (mem);
#ifdef HAVE_SYSLOG
      syslog (LOG_USER|LOG_WARNING, "Libgcrypt warning: "
              "%s-128-CBC test failed (IV mismatch)", cipher);
#endif
      return "selftest for 128 bit CBC failed - see syslog for details";
    }

  /* Test parallelized code paths */
  memset (iv, 0x5f, blocksize);
  memset (iv2, 0x5f, blocksize);

  for (i = 0; i < nblocks * blocksize; i++)
    plaintext[i] = i;

  /* Create CBC ciphertext manually.  */
  for (i = 0; i < nblocks * blocksize; i+=blocksize)
    {
      buf_xor (&ciphertext[i], iv, &plaintext[i], blocksize);
      encrypt_one (ctx, &ciphertext[i], &ciphertext[i]);
      memcpy (iv, &ciphertext[i], blocksize);
    }

  /* Decrypt using bulk CBC and compare result.  */
  bulk_cbc_dec (ctx, iv2, plaintext2, ciphertext, nblocks);

  if (memcmp (plaintext2, plaintext, nblocks * blocksize))
    {
      gcry_free (mem);
#ifdef HAVE_SYSLOG
      syslog (LOG_USER|LOG_WARNING, "Libgcrypt warning: "
              "%s-128-CBC test failed (plaintext mismatch, parallel path)",
	      cipher);
#endif
      return "selftest for 128 bit CBC failed - see syslog for details";
    }
  if (memcmp (iv2, iv, blocksize))
    {
      gcry_free (mem);
#ifdef HAVE_SYSLOG
      syslog (LOG_USER|LOG_WARNING, "Libgcrypt warning: "
              "%s-128-CBC test failed (IV mismatch, parallel path)",
	      cipher);
#endif
      return "selftest for 128 bit CBC failed - see syslog for details";
    }

  gcry_free (mem);
  return NULL;
}

/* Run the self-tests for <block cipher>-CTR-128, tests IV increment of bulk CTR
   encryption.  Returns NULL on success. */
const char *
_gcry_selftest_helper_ctr_128 (const char *cipher,
                               gcry_cipher_setkey_t setkey_func,
			       gcry_cipher_encrypt_t encrypt_one,
			       gcry_cipher_bulk_ctr_enc_t bulk_ctr_enc,
			       const int nblocks, const int blocksize,
			       const int context_size)
{
  int i, j, offs, diff;
  unsigned char *ctx, *plaintext, *plaintext2, *ciphertext, *iv, *iv2, *mem;
  unsigned int ctx_aligned_size, memsize;

  static const unsigned char key[16] ATTR_ALIGNED_16 = {
      0x06,0x9A,0x00,0x7F,0xC7,0x6A,0x45,0x9F,
      0x98,0xBA,0xF9,0x17,0xFE,0xDF,0x95,0x21
    };

  /* Allocate buffers, align elements to 16 bytes.  */
  ctx_aligned_size = context_size + 15;
  ctx_aligned_size -= ctx_aligned_size & 0xf;

  memsize = ctx_aligned_size + (blocksize * 2) + (blocksize * nblocks * 3) + 16;

  mem = gcry_calloc (1, memsize);
  if (!mem)
    return "failed to allocate memory";

  offs = (16 - ((uintptr_t)mem & 15)) & 15;
  ctx = (void*)(mem + offs);
  iv = ctx + ctx_aligned_size;
  iv2 = iv + blocksize;
  plaintext = iv2 + blocksize;
  plaintext2 = plaintext + nblocks * blocksize;
  ciphertext = plaintext2 + nblocks * blocksize;

  /* Initialize ctx */
  setkey_func (ctx, key, sizeof(key));

  /* Test single block code path */
  memset (iv, 0xff, blocksize);
  for (i = 0; i < blocksize; i++)
    plaintext[i] = i;

  /* CTR manually.  */
  encrypt_one (ctx, ciphertext, iv);
  for (i = 0; i < blocksize; i++)
    ciphertext[i] ^= plaintext[i];
  for (i = blocksize; i > 0; i--)
    {
      iv[i-1]++;
      if (iv[i-1])
        break;
    }

  memset (iv2, 0xff, blocksize);
  bulk_ctr_enc (ctx, iv2, plaintext2, ciphertext, 1);

  if (memcmp (plaintext2, plaintext, blocksize))
    {
      gcry_free (mem);
#ifdef HAVE_SYSLOG
      syslog (LOG_USER|LOG_WARNING, "Libgcrypt warning: "
              "%s-128-CTR test failed (plaintext mismatch)", cipher);
#endif
      return "selftest for 128 bit CTR failed - see syslog for details";
    }

  if (memcmp (iv2, iv, blocksize))
    {
      gcry_free (mem);
#ifdef HAVE_SYSLOG
      syslog (LOG_USER|LOG_WARNING, "Libgcrypt warning: "
              "%s-128-CTR test failed (IV mismatch)", cipher);
#endif
      return "selftest for 128 bit CTR failed - see syslog for details";
    }

  /* Test parallelized code paths */
  for (diff = 0; diff < nblocks; diff++) {
    memset(iv, 0xff, blocksize);
    iv[blocksize-1] -= diff;

    for (i = 0; i < blocksize * nblocks; i++)
      plaintext[i] = i;

    /* Create CTR ciphertext manually.  */
    for (i = 0; i < blocksize * nblocks; i+=blocksize)
      {
        encrypt_one (ctx, &ciphertext[i], iv);
        for (j = 0; j < blocksize; j++)
          ciphertext[i+j] ^= plaintext[i+j];
        for (j = blocksize; j > 0; j--)
          {
            iv[j-1]++;
            if (iv[j-1])
              break;
          }
      }

    /* Decrypt using bulk CTR and compare result.  */
    memset(iv2, 0xff, blocksize);
    iv2[blocksize-1] -= diff;

    bulk_ctr_enc (ctx, iv2, plaintext2, ciphertext, nblocks);

    if (memcmp (plaintext2, plaintext, blocksize * nblocks))
      {
        gcry_free (mem);
#ifdef HAVE_SYSLOG
        syslog (LOG_USER|LOG_WARNING, "Libgcrypt warning: "
                "%s-128-CTR test failed (plaintext mismatch, diff: %d)", cipher,
		diff);
#endif
        return "selftest for 128 bit CTR failed - see syslog for details";
      }
    if (memcmp(iv2, iv, blocksize))
      {
        gcry_free (mem);
#ifdef HAVE_SYSLOG
        syslog (LOG_USER|LOG_WARNING, "Libgcrypt warning: "
                "%s-128-CTR test failed (IV mismatch, diff: %d)", cipher, diff);
#endif
        return "selftest for 128 bit CTR failed - see syslog for details";
      }
  }

  gcry_free (mem);
  return NULL;
}