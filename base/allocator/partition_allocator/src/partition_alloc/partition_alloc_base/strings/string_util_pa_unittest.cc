// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal::base::strings {

TEST(PartitionAllocStringUtilTest, FindLastOf) {
  EXPECT_EQ('c', *FindLastOf("abcdefg", "abc"));
  EXPECT_EQ('b', *FindLastOf("abcdefg", "abC"));
  EXPECT_EQ('g', *FindLastOf("abcdefg", "g"));

  EXPECT_EQ("abbbb", std::string(FindLastOf("aaabbbb", "a")));
  EXPECT_EQ("b", std::string(FindLastOf("aaabbbb", "ab")));

  EXPECT_EQ(nullptr, FindLastOf("abcdefg", "\0"));
  EXPECT_EQ(nullptr, FindLastOf("abcdefg", "hijk"));
  EXPECT_EQ(nullptr, FindLastOf("abcdefg", ""));
}

TEST(PartitionAllocStringUtilTest, FindLastNotOf) {
  EXPECT_EQ('g', *FindLastNotOf("abcdefg", "abc"));
  EXPECT_EQ('g', *FindLastNotOf("abcdefg", "abC"));
  EXPECT_EQ('f', *FindLastNotOf("abcdefg", "g"));

  EXPECT_EQ("b", std::string(FindLastNotOf("aaabbbb", "a")));
  EXPECT_EQ(nullptr, FindLastNotOf("aaabbbb", "ab"));

  EXPECT_EQ('g', *FindLastNotOf("abcdefg", "\0"));
  EXPECT_EQ('g', *FindLastNotOf("abcdefg", "hijk"));
  EXPECT_EQ('g', *FindLastNotOf("abcdefg", ""));
}

}  // namespace partition_alloc::internal::base::strings
