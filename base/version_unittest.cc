// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(VersionTest, DefaultConstructor) {
  base::Version v;
  EXPECT_FALSE(v.IsValid());
}

TEST(VersionTest, ValueSemantics) {
  base::Version v1("1.2.3.4");
  EXPECT_TRUE(v1.IsValid());
  base::Version v3;
  EXPECT_FALSE(v3.IsValid());
  {
    base::Version v2(v1);
    v3 = v2;
    EXPECT_TRUE(v2.IsValid());
    EXPECT_EQ(v1, v2);
  }
  EXPECT_EQ(v3, v1);
}

TEST(VersionTest, MoveSemantics) {
  const std::vector<uint32_t> components = {1, 2, 3, 4};
  base::Version v1(std::move(components));
  EXPECT_TRUE(v1.IsValid());
  base::Version v2("1.2.3.4");
  EXPECT_EQ(v1, v2);
}

TEST(VersionTest, GetVersionFromString) {
  static const struct version_string {
    const char* input;
    size_t parts;
    uint32_t firstpart;
    bool success;
  } cases[] = {
    {"", 0, 0, false},
    {" ", 0, 0, false},
    {"\t", 0, 0, false},
    {"\n", 0, 0, false},
    {"  ", 0, 0, false},
    {".", 0, 0, false},
    {" . ", 0, 0, false},
    {"0", 1, 0, true},
    {"0.", 0, 0, false},
    {"0.0", 2, 0, true},
    {"4294967295.0", 2, 4294967295, true},
    {"4294967296.0", 0, 0, false},
    {"-1.0", 0, 0, false},
    {"1.-1.0", 0, 0, false},
    {"1,--1.0", 0, 0, false},
    {"+1.0", 0, 0, false},
    {"1.+1.0", 0, 0, false},
    {"1+1.0", 0, 0, false},
    {"++1.0", 0, 0, false},
    {"1.0a", 0, 0, false},
    {"1.2.3.4.5.6.7.8.9.0", 10, 1, true},
    {"02.1", 0, 0, false},
    {"0.01", 2, 0, true},
    {"f.1", 0, 0, false},
    {"15.007.20011", 3, 15, true},
    {"15.5.28.130162", 4, 15, true},
  };

  for (const auto& i : cases) {
    base::Version version(i.input);
    EXPECT_EQ(i.success, version.IsValid());
    if (i.success) {
      EXPECT_EQ(i.parts, version.components().size());
      EXPECT_EQ(i.firstpart, version.components()[0]);
    }
  }
}

TEST(VersionTest, Compare) {
  static const struct version_compare {
    const char* lhs;
    const char* rhs;
    int expected;
  } cases[] = {
      {"1.0", "1.0", 0},
      {"1.0", "0.0", 1},
      {"1.0", "2.0", -1},
      {"1.0", "1.1", -1},
      {"1.1", "1.0", 1},
      {"1.0", "1.0.1", -1},
      {"1.1", "1.0.1", 1},
      {"1.1", "1.0.1", 1},
      {"1.0.0", "1.0", 0},
      {"1.0.3", "1.0.20", -1},
      {"11.0.10", "15.007.20011", -1},
      {"11.0.10", "15.5.28.130162", -1},
      {"15.5.28.130162", "15.5.28.130162", 0},
  };
  for (const auto& i : cases) {
    base::Version lhs(i.lhs);
    base::Version rhs(i.rhs);
    EXPECT_EQ(lhs.CompareTo(rhs), i.expected) << i.lhs << " ? " << i.rhs;
    // CompareToWildcardString() should have same behavior as CompareTo() when
    // no wildcards are present.
    EXPECT_EQ(lhs.CompareToWildcardString(i.rhs), i.expected)
        << i.lhs << " ? " << i.rhs;
    EXPECT_EQ(rhs.CompareToWildcardString(i.lhs), -i.expected)
        << i.lhs << " ? " << i.rhs;

    // Test comparison operators
    switch (i.expected) {
      case -1:
        EXPECT_LT(lhs, rhs);
        EXPECT_LE(lhs, rhs);
        EXPECT_NE(lhs, rhs);
        EXPECT_FALSE(lhs == rhs);
        EXPECT_FALSE(lhs >= rhs);
        EXPECT_FALSE(lhs > rhs);
        break;
      case 0:
        EXPECT_FALSE(lhs < rhs);
        EXPECT_LE(lhs, rhs);
        EXPECT_FALSE(lhs != rhs);
        EXPECT_EQ(lhs, rhs);
        EXPECT_GE(lhs, rhs);
        EXPECT_FALSE(lhs > rhs);
        break;
      case 1:
        EXPECT_FALSE(lhs < rhs);
        EXPECT_FALSE(lhs <= rhs);
        EXPECT_NE(lhs, rhs);
        EXPECT_FALSE(lhs == rhs);
        EXPECT_GE(lhs, rhs);
        EXPECT_GT(lhs, rhs);
        break;
    }
  }
}

TEST(VersionTest, CompareToWildcardString) {
  static const struct version_compare {
    const char* lhs;
    const char* rhs;
    int expected;
  } cases[] = {
    {"1.0", "1.*", 0},
    {"1.0", "0.*", 1},
    {"1.0", "2.*", -1},
    {"1.2.3", "1.2.3.*", 0},
    {"10.0", "1.0.*", 1},
    {"1.0", "3.0.*", -1},
    {"1.4", "1.3.0.*", 1},
    {"1.3.9", "1.3.*", 0},
    {"1.4.1", "1.3.*", 1},
    {"1.3", "1.4.5.*", -1},
    {"1.5", "1.4.5.*", 1},
    {"1.3.9", "1.3.*", 0},
    {"1.2.0.0.0.0", "1.2.*", 0},
  };
  for (const auto& i : cases) {
    const base::Version version(i.lhs);
    const int result = version.CompareToWildcardString(i.rhs);
    EXPECT_EQ(result, i.expected) << i.lhs << "?" << i.rhs;
  }
}

TEST(VersionTest, IsValidWildcardString) {
  static const struct version_compare {
    const char* version;
    bool expected;
  } cases[] = {
    {"1.0", true},
    {"", false},
    {"1.2.3.4.5.6", true},
    {"1.2.3.*", true},
    {"1.2.3.5*", false},
    {"1.2.3.56*", false},
    {"1.*.3", false},
    {"20.*", true},
    {"+2.*", false},
    {"*", false},
    {"*.2", false},
  };
  for (const auto& i : cases) {
    EXPECT_EQ(base::Version::IsValidWildcardString(i.version), i.expected)
        << i.version << "?" << i.expected;
  }
}

TEST(VersionTest, LeadingZeros) {
  {
    // Leading zeros in the first component are not allowed.
    base::Version v("01.1");
    EXPECT_FALSE(v.IsValid());
  }

  {
    // Leading zeros in subsequent components are allowed (and this behavior is
    // now important for compatibility with existing modules, like extensions),
    // but are ignored because the value is parsed as an integer...
    base::Version v1("1.01");
    EXPECT_TRUE(v1.IsValid());
    // ...and as a result, v1.01 == v1.1.
    EXPECT_EQ("1.1", v1.GetString());
    base::Version v2("1.1");
    EXPECT_EQ(v1, v2);
  }

  // Similarly, since leading zeros are ignored, v1.02 > v1.1 (because
  // v1.02 is translated to 1.2).
  EXPECT_GT(base::Version("1.02"), base::Version("1.1"));
}

}  // namespace
