// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/lazy_string_builder.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(LazyStringBuilderTest, Empty) {
  auto builder = LazyStringBuilder::CreateForTesting();
  EXPECT_EQ("", builder.Build());
}

TEST(LazyStringBuilderTest, AppendStringView) {
  auto builder = LazyStringBuilder::CreateForTesting();
  std::string_view sv = "hello";
  builder.AppendByReference(sv);
  EXPECT_EQ("hello", builder.Build());
}

TEST(LazyStringBuilderTest, AppendCString) {
  auto builder = LazyStringBuilder::CreateForTesting();
  builder.AppendByReference("hello");
  EXPECT_EQ("hello", builder.Build());
}

TEST(LazyStringBuilderTest, AppendLValueString) {
  // The string must outlive the builder, and so be allocated first.
  std::string lvalue_string = "lvalue";
  auto builder = LazyStringBuilder::CreateForTesting();
  builder.AppendByReference(lvalue_string);
  EXPECT_EQ("lvalue", builder.Build());
}

TEST(LazyStringBuilderTest, AppendMultiple) {
  auto builder = LazyStringBuilder::CreateForTesting();
  builder.AppendByReference("hello", " ", "world");
  EXPECT_EQ("hello world", builder.Build());
}

TEST(LazyStringBuilderTest, AppendMultipleStringViews) {
  auto builder = LazyStringBuilder::CreateForTesting();
  std::string_view hello = "hello";
  std::string_view space = " ";
  std::string_view world = "world";
  builder.AppendByReference(hello, space, world);
  EXPECT_EQ("hello world", builder.Build());
}

TEST(LazyStringBuilderTest, AppendMultipleLValueStrings) {
  std::string s1 = "hello";
  const std::string s2 = " ";
  std::string s3 = "world";
  auto builder = LazyStringBuilder::CreateForTesting();
  builder.AppendByReference(s1, s2, s3);
  EXPECT_EQ("hello world", builder.Build());
}
TEST(LazyStringBuilderTest, CopyAndAppend) {
  auto builder = LazyStringBuilder::CreateForTesting();
  std::string s = "world";
  builder.AppendByReference("hello ");
  builder.AppendByValue(s);
  EXPECT_EQ("hello world", builder.Build());
}

TEST(LazyStringBuilderTest, CopyAndAppendRvalue) {
  auto builder = LazyStringBuilder::CreateForTesting();
  builder.AppendByReference("hello ");
  builder.AppendByValue(std::string("world"));
  EXPECT_EQ("hello world", builder.Build());
}

TEST(LazyStringBuilderTest, MixedAppend) {
  auto builder = LazyStringBuilder::CreateForTesting();
  std::string_view part1 = "The quick brown fox";
  std::string part2 = "jumps over";
  const char* part3 = "the lazy dog.";

  builder.AppendByReference(part1);
  builder.AppendByReference(" ");
  builder.AppendByValue(std::move(part2));
  builder.AppendByReference(" ", part3);

  EXPECT_EQ("The quick brown fox jumps over the lazy dog.", builder.Build());
}

TEST(LazyStringBuilderTest, ManyAppends) {
  auto builder = LazyStringBuilder::CreateForTesting();
  std::string expected;
  for (int i = 0; i < 40; ++i) {
    std::string s = base::NumberToString(i);
    builder.AppendByValue(s);

    expected += s;
  }
  EXPECT_EQ(expected, builder.Build());
}

TEST(LazyStringBuilderTest, ManyAppendsVariadic) {
  auto builder = LazyStringBuilder::CreateForTesting();
  builder.AppendByReference("0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                            "10", "11", "12", "13", "14", "15", "16", "17",
                            "18", "19", "20", "21", "22", "23", "24", "25",
                            "26", "27", "28", "29", "30", "31", "32", "33");
  EXPECT_EQ("0123456789101112131415161718192021222324252627282930313233",
            builder.Build());
}

// Build() is a const method, so it can be called multiple times, and more
// things can be appended after calling it.
TEST(LazyStringBuilderTest, Reuse) {
  auto builder = LazyStringBuilder::CreateForTesting();
  builder.AppendByReference("hello");
  EXPECT_EQ("hello", builder.Build());

  builder.AppendByReference(" world");
  EXPECT_EQ("hello world", builder.Build());
}

}  // namespace base
