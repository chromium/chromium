// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string_view>

#include "base/hash/sha1.h"
#include "base/numerics/byte_conversions.h"

namespace base {
// Implementation of SHA-1. Only handles data in byte-sized blocks,
// which simplifies the code a fair bit.

// Identifier names follow notation in FIPS PUB 180-3, where you'll
// also find a description of the algorithm:
// http://csrc.nist.gov/publications/fips/fips180-3/fips180-3_final.pdf

// Usage example:
//
// SecureHashAlgorithm sha;
// while(there is data to hash)
//   sha.Update(moredata, size of data);
// sha.Final();
// memcpy(somewhere, sha.Digest(), 20);
//
// to reuse the instance of sha, call sha.Init();

static inline uint32_t f(uint32_t t, uint32_t B, uint32_t C, uint32_t D) {
  if (t < 20)
    return (B & C) | ((~B) & D);
  if (t < 40)
    return B ^ C ^ D;
  if (t < 60)
    return (B & C) | (B & D) | (C & D);
  return B ^ C ^ D;
}

static inline uint32_t S(uint32_t n, uint32_t X) {
  return (X << n) | (X >> (32 - n));
}

static inline uint32_t K(uint32_t t) {
  if (t < 20)
    return 0x5a827999;
  if (t < 40)
    return 0x6ed9eba1;
  if (t < 60)
    return 0x8f1bbcdc;
  return 0xca62c1d6;
}

void SHA1Context::Init() {
  A = 0;
  B = 0;
  C = 0;
  D = 0;
  E = 0;
  cursor = 0;
  l = 0;
  H[0] = 0x67452301;
  H[1] = 0xefcdab89;
  H[2] = 0x98badcfe;
  H[3] = 0x10325476;
  H[4] = 0xc3d2e1f0;
}

void SHA1Context::Update(const void* data, size_t nbytes) {
  const uint8_t* d = reinterpret_cast<const uint8_t*>(data);
  while (nbytes--) {
    M[cursor++] = *d++;
    if (cursor >= 64) {
      Process();
    }
    l += 8;
  }
}

void SHA1Context::Final() {
  Pad();
  Process();

  for (auto& t : H) {
    t = ByteSwap(t);
  }
}

const unsigned char* SHA1Context::GetDigest() const {
  return reinterpret_cast<const unsigned char*>(H);
}

void SHA1Context::Pad() {
  M[cursor++] = 0x80;

  if (cursor > 64 - 8) {
    // pad out to next block
    while (cursor < 64) {
      M[cursor++] = 0;
    }

    Process();
  }

  while (cursor < 64 - 8) {
    M[cursor++] = 0;
  }

  M[cursor++] = (l >> 56) & 0xff;
  M[cursor++] = (l >> 48) & 0xff;
  M[cursor++] = (l >> 40) & 0xff;
  M[cursor++] = (l >> 32) & 0xff;
  M[cursor++] = (l >> 24) & 0xff;
  M[cursor++] = (l >> 16) & 0xff;
  M[cursor++] = (l >> 8) & 0xff;
  M[cursor++] = l & 0xff;
}

void SHA1Context::Process() {
  uint32_t t;

  // Each a...e corresponds to a section in the FIPS 180-3 algorithm.

  // a.
  //
  // W and M are in a union, so no need to memcpy.
  // memcpy(W, M, sizeof(M));
  for (t = 0; t < 16; ++t) {
    W[t] = ByteSwap(W[t]);
  }

  // b.
  for (t = 16; t < 80; ++t) {
    W[t] = S(1, W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);
  }

  // c.
  A = H[0];
  B = H[1];
  C = H[2];
  D = H[3];
  E = H[4];

  // d.
  for (t = 0; t < 80; ++t) {
    uint32_t TEMP = S(5, A) + f(t, B, C, D) + E + W[t] + K(t);
    E = D;
    D = C;
    C = S(30, B);
    B = A;
    A = TEMP;
  }

  // e.
  H[0] += A;
  H[1] += B;
  H[2] += C;
  H[3] += D;
  H[4] += E;

  cursor = 0;
}

// These functions allow streaming SHA-1 operations.
void SHA1Init(SHA1Context& context) {
  context.Init();
}

void SHA1Update(std::string_view data, SHA1Context& context) {
  context.Update(data.data(), data.size());
}

void SHA1Final(SHA1Context& context, SHA1Digest& digest) {
  context.Final();
  memcpy(digest.data(), context.GetDigest(), kSHA1Length);
}

SHA1Digest SHA1Hash(span<const uint8_t> data) {
  SHA1Context context;
  context.Init();
  context.Update(data.data(), data.size());
  context.Final();

  SHA1Digest digest;
  memcpy(digest.data(), context.GetDigest(), kSHA1Length);
  return digest;
}

std::string SHA1HashString(std::string_view str) {
  return std::string(as_string_view(SHA1Hash(base::as_byte_span(str))));
}

}  // namespace base
