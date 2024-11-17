// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash/sha1_boringssl.h"

#include <stdint.h>

#include <cstddef>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace base {
static_assert(kSHA1Length == SHA_DIGEST_LENGTH,
              "SHA-1 digest length mismatch.");

SHA1Digest SHA1Hash(span<const uint8_t> data) {
  SHA1Digest digest;
  SHA1(data.data(), data.size(), digest.data());
  return digest;
}

std::string SHA1HashString(std::string_view str) {
  std::string digest(kSHA1Length, '\0');
  SHA1(reinterpret_cast<const uint8_t*>(str.data()), str.size(),
       reinterpret_cast<uint8_t*>(digest.data()));
  return digest;
}

// These functions allow streaming SHA-1 operations.
void SHA1Init(SHA1Context& context) {
  SHA1_Init(&context);
}

void SHA1Update(std::string_view data, SHA1Context& context) {
  SHA1_Update(&context, data.data(), data.size());
}

void SHA1Final(SHA1Context& context, SHA1Digest& digest) {
  SHA1Context ctx(context);
  SHA1_Final(digest.data(), &ctx);
}

}  // namespace base
