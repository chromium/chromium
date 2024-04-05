// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/uuid.h"

#include <stdint.h>

#include <limits>
#include <set>
#include <string_view>
#include <unordered_set>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// The format of Uuid version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
// where y is one of [8, 9, a, b].
bool IsValidV4(const Uuid& guid) {
  const std::string& lowercase = guid.AsLowercaseString();
  return guid.is_valid() && lowercase[14] == '4' &&
         (lowercase[19] == '8' || lowercase[19] == '9' ||
          lowercase[19] == 'a' || lowercase[19] == 'b');
}

}  // namespace

TEST(UuidTest, UuidBasicUniqueness) {
  constexpr int kIterations = 10;
  for (int i = 0; i < kIterations; ++i) {
    const Uuid guid1 = Uuid::GenerateRandomV4();
    const Uuid guid2 = Uuid::GenerateRandomV4();
    EXPECT_NE(guid1, guid2);
    EXPECT_TRUE(guid1.is_valid());
    EXPECT_TRUE(IsValidV4(guid1));
    EXPECT_TRUE(guid2.is_valid());
    EXPECT_TRUE(IsValidV4(guid2));
  }
}

namespace {

void TestUuidValidity(std::string_view input,
                      bool case_insensitive,
                      bool strict) {
  SCOPED_TRACE(input);
  {
    const Uuid guid = Uuid::ParseCaseInsensitive(input);
    EXPECT_EQ(case_insensitive, guid.is_valid());
  }
  {
    const Uuid guid = Uuid::ParseLowercase(input);
    EXPECT_EQ(strict, guid.is_valid());
  }
}

}  // namespace

TEST(UuidTest, Validity) {
  // Empty Uuid is invalid.
  EXPECT_FALSE(Uuid().is_valid());

  enum Parsability { kDoesntParse, kParsesCaseInsensitiveOnly, kAlwaysParses };

  static constexpr struct {
    std::string_view input;
    Parsability parsability;
  } kUuidValidity[] = {
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

  for (const auto& validity : kUuidValidity) {
    const bool case_insensitive = validity.parsability != kDoesntParse;
    const bool strict = validity.parsability == kAlwaysParses;
    TestUuidValidity(validity.input, case_insensitive, strict);
  }
}

TEST(UuidTest, EqualityAndRoundTrip) {
  static constexpr char kCanonicalStr[] =
      "deadbeef-dead-4eef-bead-beefdeadbeef";

  const Uuid from_lower =
      Uuid::ParseCaseInsensitive(ToLowerASCII(kCanonicalStr));
  EXPECT_EQ(kCanonicalStr, from_lower.AsLowercaseString());

  const Uuid from_upper =
      Uuid::ParseCaseInsensitive(ToUpperASCII(kCanonicalStr));
  EXPECT_EQ(kCanonicalStr, from_upper.AsLowercaseString());

  EXPECT_EQ(from_lower, from_upper);

  // Invalid Uuids are equal.
  EXPECT_EQ(Uuid(), Uuid());
}

TEST(UuidTest, UnorderedSet) {
  std::unordered_set<Uuid, UuidHash> guid_set;

  static constexpr char kUuid1[] = "01234567-89ab-cdef-fedc-ba9876543210";
  guid_set.insert(Uuid::ParseCaseInsensitive(ToLowerASCII(kUuid1)));
  EXPECT_EQ(1u, guid_set.size());
  guid_set.insert(Uuid::ParseCaseInsensitive(ToUpperASCII(kUuid1)));
  EXPECT_EQ(1u, guid_set.size());

  static constexpr char kUuid2[] = "deadbeef-dead-beef-dead-beefdeadbeef";
  guid_set.insert(Uuid::ParseCaseInsensitive(ToLowerASCII(kUuid2)));
  EXPECT_EQ(2u, guid_set.size());
  guid_set.insert(Uuid::ParseCaseInsensitive(ToUpperASCII(kUuid2)));
  EXPECT_EQ(2u, guid_set.size());
}

TEST(UuidTest, Set) {
  std::set<Uuid> guid_set;

  static constexpr char kUuid1[] = "01234567-89ab-cdef-0123-456789abcdef";
  const Uuid guid1 = Uuid::ParseLowercase(kUuid1);
  ASSERT_TRUE(guid1.is_valid());
  guid_set.insert(guid1);

  static constexpr char kUuid2[] = "deadbeef-dead-beef-dead-beefdeadbeef";
  const Uuid guid2 = Uuid::ParseLowercase(kUuid2);
  ASSERT_TRUE(guid2.is_valid());
  guid_set.insert(guid2);

  // Test that the order of the Uuids was preserved.
  auto it = guid_set.begin();
  EXPECT_EQ(guid1, *it);
  ++it;
  EXPECT_EQ(guid2, *it);
  ++it;
  EXPECT_EQ(guid_set.end(), it);
}

TEST(UuidTest, Compare) {
  static constexpr char kUuid[] = "21abd97f-73e8-4b88-9389-a9fee6abda5e";
  static constexpr char kUuidLess[] = "1e0dcaca-9e7c-4f4b-bcc6-e4c02b0c99df";
  static constexpr char kUuidGreater[] = "6eeb1bc8-186b-433c-9d6a-a827bc96b2d4";

  const Uuid guid = Uuid::ParseLowercase(kUuid);
  const Uuid guid_eq = Uuid::ParseLowercase(kUuid);
  const Uuid guid_lt = Uuid::ParseLowercase(kUuidLess);
  const Uuid guid_gt = Uuid::ParseLowercase(kUuidGreater);
  const Uuid guid_invalid = Uuid();

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

  // Invalid Uuids are the "least".
  EXPECT_FALSE(guid_invalid == guid);
  EXPECT_TRUE(guid_invalid != guid);
  EXPECT_TRUE(guid_invalid < guid);
  EXPECT_TRUE(guid_invalid <= guid);
  EXPECT_FALSE(guid_invalid > guid);
  EXPECT_FALSE(guid_invalid >= guid);
}

TEST(UuidTest, FormatRandomDataAsV4) {
  static constexpr uint64_t bytes1a[] = {0x0123456789abcdefull,
                                         0x5a5a5a5aa5a5a5a5ull};
  static constexpr uint64_t bytes1b[] = {bytes1a[0], bytes1a[1]};
  static constexpr uint64_t bytes2[] = {0xfffffffffffffffdull,
                                        0xfffffffffffffffeull};
  static constexpr uint64_t bytes3[] = {0xfffffffffffffffdull,
                                        0xfffffffffffffffcull};

  const Uuid guid1a =
      Uuid::FormatRandomDataAsV4ForTesting(as_bytes(make_span(bytes1a)));
  const Uuid guid1b =
      Uuid::FormatRandomDataAsV4ForTesting(as_bytes(make_span(bytes1b)));
  const Uuid guid2 =
      Uuid::FormatRandomDataAsV4ForTesting(as_bytes(make_span(bytes2)));
  const Uuid guid3 =
      Uuid::FormatRandomDataAsV4ForTesting(as_bytes(make_span(bytes3)));

  EXPECT_TRUE(guid1a.is_valid());
  EXPECT_TRUE(guid1b.is_valid());
  EXPECT_TRUE(guid2.is_valid());
  EXPECT_TRUE(guid3.is_valid());

  // The same input should give the same Uuid.
  EXPECT_EQ(guid1a, guid1b);

  EXPECT_NE(guid1a, guid2);
  EXPECT_NE(guid1a, guid3);
  EXPECT_NE(guid2, guid3);
}

}  // namespace base
