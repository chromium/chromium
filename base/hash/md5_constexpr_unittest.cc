// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash/md5_constexpr.h"

#include "base/hash/md5.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace internal {
// Provide storage for the various constants, allowing the functions to be used
// at runtime for these tests.
constexpr std::array<uint32_t, 64> MD5CE::kConstants;
constexpr std::array<uint32_t, 16> MD5CE::kShifts;
constexpr MD5CE::IntermediateData MD5CE::kInitialIntermediateData;
}  // namespace internal

// A constexpr comparison operator for MD5Results, allowing compile time tests
// to be expressed.
constexpr bool Equal(const MD5Digest& lhs, const MD5Digest& rhs) {
  for (size_t i = 0; i < std::size(lhs.a); ++i) {
    if (lhs.a[i] != rhs.a[i])
      return false;
  }
  return true;
}

// Ensure that everything works at compile-time by comparing to a few
// reference hashes.
constexpr char kMessage0[] = "message digest";
static_assert(MD5Hash64Constexpr(kMessage0) == 0xF96B697D7CB7938Dull,
              "incorrect MD5Hash64 implementation");

static_assert(MD5Hash32Constexpr(kMessage0) == 0xF96B697Dul,
              "incorrect MD5Hash32 implementation");

constexpr char kMessage1[] = "The quick brown fox jumps over the lazy dog";
static_assert(MD5Hash64Constexpr(kMessage1, std::size(kMessage1) - 1) ==
                  0x9E107D9D372BB682ull,
              "incorrect MD5Hash64 implementation");

static_assert(MD5Hash32Constexpr(kMessage1, std::size(kMessage1) - 1) ==
                  0x9E107D9Dul,
              "incorrect MD5Hash32 implementation");

// Comparison operator for checking that the constexpr MD5 implementation
// matches the default implementation.
void ExpectEqual(const MD5Digest& lhs, const MD5Digest& rhs) {
  for (size_t i = 0; i < std::size(lhs.a); ++i)
    EXPECT_EQ(lhs.a[i], rhs.a[i]);
}

}  // namespace base
