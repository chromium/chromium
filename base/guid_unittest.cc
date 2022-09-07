// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/guid.h"

#include <stdint.h>

#include <limits>
#include <set>
#include <unordered_set>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(GUIDTest, GUIDGeneratesAllZeroes) {
  static constexpr uint64_t kBytes[] = {0, 0};
  const std::string clientid = RandomDataToGUIDString(kBytes);
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", clientid);
}

TEST(GUIDTest, GUIDGeneratesCorrectly) {
  static constexpr uint64_t kBytes[] = {0x0123456789ABCDEFULL,
                                        0xFEDCBA9876543210ULL};
  const std::string clientid = RandomDataToGUIDString(kBytes);
  EXPECT_EQ("01234567-89ab-cdef-fedc-ba9876543210", clientid);
}

TEST(GUIDTest, DeprecatedGUIDCorrectlyFormatted) {
  constexpr int kIterations = 10;
  for (int i = 0; i < kIterations; ++i) {
    const std::string guid = GenerateGUID();
    EXPECT_TRUE(IsValidGUID(guid));
    EXPECT_TRUE(IsValidGUIDOutputString(guid));
    EXPECT_TRUE(IsValidGUID(ToLowerASCII(guid)));
    EXPECT_TRUE(IsValidGUID(ToUpperASCII(guid)));
  }
}

TEST(GUIDTest, DeprecatedGUIDBasicUniqueness) {
  constexpr int kIterations = 10;
  for (int i = 0; i < kIterations; ++i) {
    const std::string guid_str1 = GenerateGUID();
    const std::string guid_str2 = GenerateGUID();
    EXPECT_EQ(36U, guid_str1.length());
    EXPECT_EQ(36U, guid_str2.length());
    EXPECT_NE(guid_str1, guid_str2);

    const GUID guid1 = GUID::ParseCaseInsensitive(guid_str1);
    EXPECT_TRUE(guid1.is_valid());
    const GUID guid2 = GUID::ParseCaseInsensitive(guid_str2);
    EXPECT_TRUE(guid2.is_valid());
  }
}

namespace {

// The format of GUID version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
// where y is one of [8, 9, a, b].
bool IsValidV4(const GUID& guid) {
  const std::string& lowercase = guid.AsLowercaseString();
  return guid.is_valid() && lowercase[14] == '4' &&
         (lowercase[19] == '8' || lowercase[19] == '9' ||
          lowercase[19] == 'a' || lowercase[19] == 'b');
}

}  // namespace

TEST(GUIDTest, GUIDBasicUniqueness) {
  constexpr int kIterations = 10;
  for (int i = 0; i < kIterations; ++i) {
    const GUID guid1 = GUID::GenerateRandomV4();
    const GUID guid2 = GUID::GenerateRandomV4();
    EXPECT_NE(guid1, guid2);
    EXPECT_TRUE(guid1.is_valid());
    EXPECT_TRUE(IsValidV4(guid1));
    EXPECT_TRUE(guid2.is_valid());
    EXPECT_TRUE(IsValidV4(guid2));
  }
}

namespace {

void TestGUIDValidity(StringPiece input, bool case_insensitive, bool strict) {
  SCOPED_TRACE(input);
  {
    const GUID guid = GUID::ParseCaseInsensitive(input);
    EXPECT_EQ(case_insensitive, guid.is_valid());
  }
  {
    const GUID guid = GUID::ParseLowercase(input);
    EXPECT_EQ(strict, guid.is_valid());
  }
}

}  // namespace

TEST(GUIDTest, Validity) {
  // Empty GUID is invalid.
  EXPECT_FALSE(GUID().is_valid());

  enum Parsability { kDoesntParse, kParsesCaseInsensitiveOnly, kAlwaysParses };

  static constexpr struct {
    StringPiece input;
    Parsability parsability;
  } kGUIDValidity[] = {
      {"invalid", kDoesntParse},
      {"0123456789ab-cdef-fedc-ba98-76543210", kDoesntParse},
      {"0123456789abcdeffedcba9876543210", kDoesntParse},
      {"01234567-89Zz-ZzZz-ZzZz-Zz9876543210", kDoesntParse},
      {"DEADBEEFDEADBEEFDEADBEEFDEADBEEF", kDoesntParse},
      {"deadbeefWdeadXbeefYdeadZbeefdeadbeef", kDoesntParse},
      {"XXXdeadbeefWdeadXbeefYdeadZbeefdeadbeefXXX", kDoesntParse},
      {"01234567-89aB-cDeF-fEdC-bA9876543210", kParsesCaseInsensitiveOnly},
      {"DEADBEEF-DEAD-BEEF-DEAD-BEEFDEADBEEF", kParsesCaseInsensitiveOnly},
      {"00000000-0000-0000-0000-000000000000", kAlwaysParses},
      {"deadbeef-dead-beef-dead-beefdeadbeef", kAlwaysParses},
  };

  for (const auto& validity : kGUIDValidity) {
    const bool case_insensitive = validity.parsability != kDoesntParse;
    const bool strict = validity.parsability == kAlwaysParses;
    TestGUIDValidity(validity.input, case_insensitive, strict);
  }
}

TEST(GUIDTest, Equality) {
  static constexpr uint64_t kBytes[] = {0xDEADBEEFDEADBEEFULL,
                                        0xDEADBEEFDEADBEEFULL};
  const std::string clientid = RandomDataToGUIDString(kBytes);

  static constexpr char kExpectedCanonicalStr[] =
      "deadbeef-dead-beef-dead-beefdeadbeef";
  ASSERT_EQ(kExpectedCanonicalStr, clientid);

  const GUID from_lower = GUID::ParseCaseInsensitive(ToLowerASCII(clientid));
  EXPECT_EQ(kExpectedCanonicalStr, from_lower.AsLowercaseString());

  const GUID from_upper = GUID::ParseCaseInsensitive(ToUpperASCII(clientid));
  EXPECT_EQ(kExpectedCanonicalStr, from_upper.AsLowercaseString());

  EXPECT_EQ(from_lower, from_upper);

  // Invalid GUIDs are equal.
  EXPECT_EQ(GUID(), GUID());
}

TEST(GUIDTest, UnorderedSet) {
  std::unordered_set<GUID, GUIDHash> guid_set;

  static constexpr char kGUID1[] = "01234567-89ab-cdef-fedc-ba9876543210";
  guid_set.insert(GUID::ParseCaseInsensitive(ToLowerASCII(kGUID1)));
  EXPECT_EQ(1u, guid_set.size());
  guid_set.insert(GUID::ParseCaseInsensitive(ToUpperASCII(kGUID1)));
  EXPECT_EQ(1u, guid_set.size());

  static constexpr char kGUID2[] = "deadbeef-dead-beef-dead-beefdeadbeef";
  guid_set.insert(GUID::ParseCaseInsensitive(ToLowerASCII(kGUID2)));
  EXPECT_EQ(2u, guid_set.size());
  guid_set.insert(GUID::ParseCaseInsensitive(ToUpperASCII(kGUID2)));
  EXPECT_EQ(2u, guid_set.size());
}

TEST(GUIDTest, Set) {
  std::set<GUID> guid_set;

  static constexpr char kGUID1[] = "01234567-89ab-cdef-0123-456789abcdef";
  const GUID guid1 = GUID::ParseLowercase(kGUID1);
  ASSERT_TRUE(guid1.is_valid());
  guid_set.insert(guid1);

  static constexpr char kGUID2[] = "deadbeef-dead-beef-dead-beefdeadbeef";
  const GUID guid2 = GUID::ParseLowercase(kGUID2);
  ASSERT_TRUE(guid2.is_valid());
  guid_set.insert(guid2);

  // Test that the order of the GUIDs was preserved.
  auto it = guid_set.begin();
  EXPECT_EQ(guid1, *it);
  ++it;
  EXPECT_EQ(guid2, *it);
  ++it;
  EXPECT_EQ(guid_set.end(), it);
}

TEST(GUIDTest, Compare) {
  static constexpr char kGUID[] = "21abd97f-73e8-4b88-9389-a9fee6abda5e";
  static constexpr char kGUIDLess[] = "1e0dcaca-9e7c-4f4b-bcc6-e4c02b0c99df";
  static constexpr char kGUIDGreater[] = "6eeb1bc8-186b-433c-9d6a-a827bc96b2d4";

  const GUID guid = GUID::ParseLowercase(kGUID);
  const GUID guid_eq = GUID::ParseLowercase(kGUID);
  const GUID guid_lt = GUID::ParseLowercase(kGUIDLess);
  const GUID guid_gt = GUID::ParseLowercase(kGUIDGreater);
  const GUID guid_invalid = GUID();

  EXPECT_TRUE(guid_eq == guid);
  EXPECT_FALSE(guid_eq != guid);
  EXPECT_FALSE(guid_eq < guid);
  EXPECT_TRUE(guid_eq <= guid);
  EXPECT_FALSE(guid_eq > guid);
  EXPECT_TRUE(guid_eq >= guid);

  EXPECT_FALSE(guid_lt == guid);
  EXPECT_TRUE(guid_lt != guid);
  EXPECT_TRUE(guid_lt < guid);
  EXPECT_TRUE(guid_lt <= guid);
  EXPECT_FALSE(guid_lt > guid);
  EXPECT_FALSE(guid_lt >= guid);

  EXPECT_FALSE(guid_gt == guid);
  EXPECT_TRUE(guid_gt != guid);
  EXPECT_FALSE(guid_gt < guid);
  EXPECT_FALSE(guid_gt <= guid);
  EXPECT_TRUE(guid_gt > guid);
  EXPECT_TRUE(guid_gt >= guid);

  // Invalid GUIDs are the "least".
  EXPECT_FALSE(guid_invalid == guid);
  EXPECT_TRUE(guid_invalid != guid);
  EXPECT_TRUE(guid_invalid < guid);
  EXPECT_TRUE(guid_invalid <= guid);
  EXPECT_FALSE(guid_invalid > guid);
  EXPECT_FALSE(guid_invalid >= guid);
}

}  // namespace base
