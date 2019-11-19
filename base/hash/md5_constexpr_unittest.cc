// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash/md5_constexpr.h"

#include "base/hash/md5.h"
#include "base/stl_util.h"
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
  for (size_t i = 0; i < base::size(lhs.a); ++i) {
    if (lhs.a[i] != rhs.a[i])
      return false;
  }
  return true;
}

// Ensure that everything works at compile-time by comparing to a few
// reference hashes.
constexpr char kMessage0[] = "message digest";
static_assert(Equal(MD5SumConstexpr(kMessage0),
                    MD5Digest{0xF9, 0x6B, 0x69, 0x7D, 0x7C, 0xB7, 0x93, 0x8D,
                              0x52, 0x5A, 0x2F, 0x31, 0xAA, 0xF1, 0x61, 0xD0}),
              "incorrect MD5Sum implementation");

static_assert(MD5Hash64Constexpr(kMessage0) == 0xF96B697D7CB7938Dull,
              "incorrect MD5Hash64 implementation");

static_assert(MD5Hash32Constexpr(kMessage0) == 0xF96B697Dul,
              "incorrect MD5Hash32 implementation");

constexpr char kMessage1[] = "The quick brown fox jumps over the lazy dog";
static_assert(Equal(MD5SumConstexpr(kMessage1, base::size(kMessage1) - 1),
                    MD5Digest{0x9e, 0x10, 0x7d, 0x9d, 0x37, 0x2b, 0xb6, 0x82,
                              0x6b, 0xd8, 0x1d, 0x35, 0x42, 0xa4, 0x19, 0xd6}),
              "incorrect MD5Sum implementation");

static_assert(MD5Hash64Constexpr(kMessage1, base::size(kMessage1) - 1) ==
                  0x9E107D9D372BB682ull,
              "incorrect MD5Hash64 implementation");

static_assert(MD5Hash32Constexpr(kMessage1, base::size(kMessage1) - 1) ==
                  0x9E107D9Dul,
              "incorrect MD5Hash32 implementation");

// Comparison operator for checking that the constexpr MD5 implementation
// matches the default implementation.
void ExpectEqual(const MD5Digest& lhs, const MD5Digest& rhs) {
  for (size_t i = 0; i < base::size(lhs.a); ++i)
    EXPECT_EQ(lhs.a[i], rhs.a[i]);
}

TEST(MD5ConstExprTest, Correctness) {
  const char* kChunks[] = {"hey jude",           "don't make it bad",
                           "take a sad song",    "and make it better",
                           "remember",           "to let her into your heart",
                           "then you can start", "to make it better",
                           "hey jude",           "don't be afraid",
                           "you were made to",   "go out and get her",
                           "the minute",         "you let her under your skin",
                           "then you begin",     "to make it better"};

  // Compares our implemetantion to Chrome's default implementation.
  std::string message;
  for (size_t i = 0; i < base::size(kChunks); ++i) {
    if (i > 0)
      message += " ";
    message += kChunks[i];

    MD5Digest digest1 = MD5SumConstexpr(message.c_str(), message.size());

    MD5Digest digest2 = {};
    MD5Sum(message.c_str(), message.size(), &digest2);

    ExpectEqual(digest1, digest2);
  }
}

}  // namespace base
