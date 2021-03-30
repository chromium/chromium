// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash/sha1.h"

#include <stdint.h>

#include "base/strings/string_util.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace base {
static_assert(kSHA1Length == SHA_DIGEST_LENGTH,
              "SHA-1 digest length mismatch.");

SHA1Digest SHA1HashSpan(span<const uint8_t> data) {
  CRYPTO_library_init();
  SHA1Digest digest;
  SHA1(data.data(), data.size(), digest.data());
  return digest;
}

std::string SHA1HashString(StringPiece str) {
  CRYPTO_library_init();
  std::string digest;
  SHA1(reinterpret_cast<const uint8_t*>(str.data()), str.size(),
       reinterpret_cast<uint8_t*>(WriteInto(&digest, kSHA1Length + 1)));
  return digest;
}

void SHA1HashBytes(const unsigned char* data, size_t len, unsigned char* hash) {
  CRYPTO_library_init();
  SHA1(data, len, hash);
}

// These functions allow streaming SHA-1 operations.
void SHA1Init(SHA1Context& context) {
  SHA1_Init(&context);
}

void SHA1Update(const StringPiece data, SHA1Context& context) {
  SHA1_Update(&context, data.data(), data.size());
}

void SHA1Final(SHA1Context& context, SHA1Digest& digest) {
  SHA1Context ctx(context);
  SHA1_Final(digest.data(), &ctx);
}

}  // namespace base
