// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_HASH_MD5_CONSTEXPR_H_
#define BASE_HASH_MD5_CONSTEXPR_H_

#include "base/hash/md5_constexpr_internal.h" // IWYU pragma: export

#include <string_view>

namespace base {

// Calculates the first 32/64 bits of the MD5 digest of the provided data,
// returned as a uint32_t/uint64_t. When passing |string| with no explicit
// length the terminating null will not be processed. This abstracts away
// endianness so that the integer will read as the first 4 or 8 bytes of the
// MD5 sum, ensuring that the following outputs are equivalent for
// convenience:
//
// printf("%08x\n", MD5Hash32Constexpr("foo"));
constexpr uint64_t MD5Hash64Constexpr(std::string_view string);
constexpr uint32_t MD5Hash32Constexpr(std::string_view string);

}  // namespace base

#endif  // BASE_HASH_MD5_CONSTEXPR_H_
