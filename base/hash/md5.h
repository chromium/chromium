// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_HASH_MD5_H_
#define BASE_HASH_MD5_H_

#include <stdint.h>

#include <array>
#include <string>
#include <string_view>

#include "base/base_export.h"
#include "base/containers/span.h"

// MD5 stands for Message Digest algorithm 5.
//
// DANGER DANGER DANGER:
// MD5 is extremely obsolete and it is trivial for a malicious party to find MD5
// collisions. Do not use MD5 for any security-related purposes whatsoever, and
// especially do not use MD5 to validate that files or other data have not been
// modified maliciously. This entire interface is obsolete and you should either
// use a non-cryptographic hash (which will be much faster) or a cryptographic
// hash (which will be collision-resistant against adversarial inputs). If you
// believe you need to add a new use of MD5, consult a member of
// //CRYPTO_OWNERS.
//
// NEW USES OF THIS API ARE FORBIDDEN FOR ANY PURPOSE. INSTEAD, YOU MUST USE
// //crypto/obsolete/md5.h.

namespace base {

// The output of an MD5 operation.
struct MD5Digest {
  std::array<uint8_t, 16> a;
};

// Converts a digest into human-readable hexadecimal.
BASE_EXPORT std::string MD5DigestToBase16(const MD5Digest& digest);

// Computes the MD5 sum of the given `data`.
// The 'digest' structure will be filled with the result.
BASE_EXPORT void MD5Sum(base::span<const uint8_t> data, MD5Digest* digest);

// Returns the MD5 (in hexadecimal) of a string.
BASE_EXPORT std::string MD5String(std::string_view str);

}  // namespace base

#endif  // BASE_HASH_MD5_H_
