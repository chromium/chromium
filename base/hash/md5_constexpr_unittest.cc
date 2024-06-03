// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash/md5_constexpr.h"

namespace base {

// Ensure that everything works at compile-time by comparing to a few
// reference hashes.
constexpr char kMessage0[] = "message digest";
static_assert(MD5Hash64Constexpr(kMessage0) == 0xF96B697D7CB7938Dull,
              "incorrect MD5Hash64 implementation");

static_assert(MD5Hash32Constexpr(kMessage0) == 0xF96B697Dul,
              "incorrect MD5Hash32 implementation");

constexpr char kMessage1[] = "The quick brown fox jumps over the lazy dog";
static_assert(MD5Hash64Constexpr(kMessage1) == 0x9E107D9D372BB682ull,
              "incorrect MD5Hash64 implementation");

static_assert(MD5Hash32Constexpr(kMessage1) == 0x9E107D9Dul,
              "incorrect MD5Hash32 implementation");

}  // namespace base
