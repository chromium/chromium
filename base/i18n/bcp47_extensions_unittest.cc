// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/bcp47_extensions.h"

#include <string_view>

#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {

namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;

// Helper to create a UnicodeExtension for testing.
// Since the constructor is protected by a PassKey<LanguageTag>, we use
// LanguageTagConverter to create a LanguageTag and then extract the extension.
std::optional<UnicodeExtension> CreateUnicodeExtension(
    std::string_view subtags) {
  std::string locale_id = "und-u-" + std::string(subtags);
  std::optional<LanguageTag> lc =
      LanguageTagConverter::GetInstance().FromString(locale_id);
  if (!lc) {
    return std::nullopt;
  }

  return lc->GetExtension(bcp47_extensions::unicode());
}

PrivateUseSubtags CreatePrivateUseSubtags(std::string_view subtags) {
  std::string locale_id = "und-x-" + std::string(subtags);
  std::optional<LanguageTag> lc =
      LanguageTagConverter::GetInstance().FromString(locale_id);
  // Using .value() here since we expect the test inputs to be valid.
  return lc->GetExtension(bcp47_extensions::priv()).value();
}

// Helper to create a generic Extension for testing.
Extension CreateExtension(char singleton, std::string_view subtags) {
  std::string locale_id = "und-";
  locale_id.push_back(singleton);
  locale_id.push_back('-');
  locale_id.append(subtags);
  std::optional<LanguageTag> lc =
      LanguageTagConverter::GetInstance().FromString(locale_id);

  // We can't use GetExtension() with traits easily for generic singletons
  // unless we use ext<singleton>(). For testing, we'll use ext<'a'> as a
  // placeholder if we need a specific one, or just call GetExtensionInternal
  // if it were public (it's not).
  // Fortunately, we can use the ext<c> template.

  if (singleton == 'a') {
    return lc->GetExtension(bcp47_extensions::ext<'a'>()).value();
  }

  // Fallback for other singletons if needed, but 'a' and 'x' are enough for
  // basics.
  return lc->GetExtension(bcp47_extensions::ext<'a'>()).value();
}

}  // namespace

TEST(Bcp47ExtensionTest, GenericExtension) {
  Extension ext = CreateExtension('a', "myext");
  EXPECT_EQ(ext.singleton(), 'a');
  EXPECT_EQ(ext.SubtagsString(), "myext");

  PrivateUseSubtags ext2 = CreatePrivateUseSubtags("private-use");
  EXPECT_EQ(ext2.SubtagsString(), "private-use");
}

TEST(Bcp47ExtensionTest, UnicodeExtensionBasics) {
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext,
                       CreateUnicodeExtension("ca-gregory"));
  EXPECT_EQ(ext.SubtagsString(), "ca-gregory");
}

TEST(Bcp47ExtensionTest, UnicodeExtensionAttributes) {
  // Single attribute.
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext1, CreateUnicodeExtension("attr1"));
  EXPECT_TRUE(ext1.has_attribute("attr1"));
  EXPECT_FALSE(ext1.has_attribute("attr2"));

  // Multiple attributes.
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext2,
                       CreateUnicodeExtension("attr1-attr2-attr3"));
  EXPECT_TRUE(ext2.has_attribute("attr1"));
  EXPECT_TRUE(ext2.has_attribute("attr2"));
  EXPECT_TRUE(ext2.has_attribute("attr3"));
  EXPECT_FALSE(ext2.has_attribute("attr4"));

  // Attributes with keywords.
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext3,
                       CreateUnicodeExtension("foo-bar-ca-gregory"));
  EXPECT_TRUE(ext3.has_attribute("foo"));
  EXPECT_TRUE(ext3.has_attribute("bar"));
  EXPECT_FALSE(ext3.has_attribute("ca"));
  EXPECT_FALSE(ext3.has_attribute("gregory"));
}

TEST(Bcp47ExtensionTest, UnicodeExtensionKeywords) {
  // Basic keywords.
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext1,
                       CreateUnicodeExtension("ca-gregory-co-phonebk"));
  EXPECT_THAT(ext1.GetKeywordValue("ca"), Optional(Eq("gregory")));
  EXPECT_THAT(ext1.GetKeywordValue("co"), Optional(Eq("phonebk")));
  EXPECT_THAT(ext1.GetKeywordValue("hc"), Eq(std::nullopt));

  // Multi-subtag value.
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext2,
                       CreateUnicodeExtension("ca-islamic-civil-co-emoji"));
  EXPECT_THAT(ext2.GetKeywordValue("ca"), Optional(Eq("islamic-civil")));
  EXPECT_THAT(ext2.GetKeywordValue("co"), Optional(Eq("emoji")));

  // Keyword with no value (it should return an empty string_view).
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext3,
                       CreateUnicodeExtension("kb-ca-gregory"));
  EXPECT_THAT(ext3.GetKeywordValue("kb"), Optional(Eq("")));
  EXPECT_THAT(ext3.GetKeywordValue("ca"), Optional(Eq("gregory")));

  // Last keyword with no value.
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext4,
                       CreateUnicodeExtension("ca-gregory-kn"));
  EXPECT_THAT(ext4.GetKeywordValue("ca"), Optional(Eq("gregory")));
  EXPECT_THAT(ext4.GetKeywordValue("kn"), Optional(Eq("")));
}

TEST(Bcp47ExtensionTest, UnicodeExtensionAccessors) {
  // Test all common accessors.
  ASSERT_OK_AND_ASSIGN(
      UnicodeExtension ext,
      CreateUnicodeExtension(
          "ca-val1-cf-val2-co-val3-cu-val4-dx-val5-em-val6-fw-val7-hc-val8-kf-"
          "val9-kn-val10-lb-val11-lw-val12-ms-val13-mu-val14-nu-val15-rg-val16-"
          "sd-val17-ss-val18-tz-val19-va-val20"));

  EXPECT_THAT(ext.GetKeywordValue("ca"), Optional(Eq("val1")));
  EXPECT_THAT(ext.GetKeywordValue("cf"), Optional(Eq("val2")));
  EXPECT_THAT(ext.GetKeywordValue("co"), Optional(Eq("val3")));
  EXPECT_THAT(ext.GetKeywordValue("cu"), Optional(Eq("val4")));
  EXPECT_THAT(ext.GetKeywordValue("dx"), Optional(Eq("val5")));
  EXPECT_THAT(ext.GetKeywordValue("em"), Optional(Eq("val6")));
  EXPECT_THAT(ext.GetKeywordValue("fw"), Optional(Eq("val7")));
  EXPECT_THAT(ext.GetKeywordValue("hc"), Optional(Eq("val8")));
  EXPECT_THAT(ext.GetKeywordValue("kf"), Optional(Eq("val9")));
  EXPECT_THAT(ext.GetKeywordValue("kn"), Optional(Eq("val10")));
  EXPECT_THAT(ext.GetKeywordValue("lb"), Optional(Eq("val11")));
  EXPECT_THAT(ext.GetKeywordValue("lw"), Optional(Eq("val12")));
  EXPECT_THAT(ext.GetKeywordValue("ms"), Optional(Eq("val13")));
  EXPECT_THAT(ext.GetKeywordValue("mu"), Optional(Eq("val14")));
  EXPECT_THAT(ext.GetKeywordValue("nu"), Optional(Eq("val15")));
  EXPECT_THAT(ext.GetKeywordValue("rg"), Optional(Eq("val16")));
  EXPECT_THAT(ext.GetKeywordValue("sd"), Optional(Eq("val17")));
  EXPECT_THAT(ext.GetKeywordValue("ss"), Optional(Eq("val18")));
  EXPECT_THAT(ext.GetKeywordValue("tz"), Optional(Eq("val19")));
  EXPECT_THAT(ext.GetKeywordValue("va"), Optional(Eq("val20")));
}

TEST(Bcp47ExtensionTest, UnicodeExtensionComplex) {
  // Mixed attributes and keywords.
  ASSERT_OK_AND_ASSIGN(
      UnicodeExtension ext,
      CreateUnicodeExtension("attr1-attr2-ca-gregory-co-emoji-attr3"));
  EXPECT_TRUE(ext.has_attribute("attr1"));
  EXPECT_TRUE(ext.has_attribute("attr2"));
  // "attr3" is after a keyword, so it should be considered part of the previous
  // keyword's value (co-emoji-attr3).
  EXPECT_FALSE(ext.has_attribute("attr3"));
  EXPECT_THAT(ext.GetKeywordValue("ca"), Optional(Eq("gregory")));
  EXPECT_THAT(ext.GetKeywordValue("co"), Optional(Eq("emoji-attr3")));
}

TEST(Bcp47ExtensionTest, UnicodeExtensionFromString) {
  // Valid with attributes and keywords.
  auto ext1 = UnicodeExtension::FromString("u-attr1-ca-gregory");
  ASSERT_TRUE(ext1.has_value());
  EXPECT_EQ(ext1->SubtagsString(), "attr1-ca-gregory");
  EXPECT_TRUE(ext1->has_attribute("attr1"));
  EXPECT_THAT(ext1->GetKeywordValue("ca"), Optional(Eq("gregory")));

  // Overload with span.
  std::vector<std::string_view> span_input1 = {"attr1", "ca", "gregory"};
  auto ext1_span = UnicodeExtension::FromSubtags(span_input1);
  ASSERT_TRUE(ext1_span.has_value());
  EXPECT_EQ(ext1_span->SubtagsString(), "attr1-ca-gregory");
  EXPECT_TRUE(ext1_span->has_attribute("attr1"));
  EXPECT_THAT(ext1_span->GetKeywordValue("ca"), Optional(Eq("gregory")));

  // Valid with just attributes.
  auto ext2 = UnicodeExtension::FromString("u-attr1-attr2");
  ASSERT_TRUE(ext2.has_value());
  EXPECT_EQ(ext2->SubtagsString(), "attr1-attr2");

  // Valid with just keywords.
  auto ext3 = UnicodeExtension::FromString("u-ca-gregory-co-phonebk");
  ASSERT_TRUE(ext3.has_value());
  EXPECT_EQ(ext3->SubtagsString(), "ca-gregory-co-phonebk");

  // Valid: keyword without type.
  auto ext4 = UnicodeExtension::FromString("u-ca");
  ASSERT_TRUE(ext4.has_value());
  EXPECT_EQ(ext4->SubtagsString(), "ca");

  // Invalid: too short.
  EXPECT_FALSE(UnicodeExtension::FromString("u-").has_value());
  EXPECT_FALSE(UnicodeExtension::FromString("u").has_value());

  // Invalid: doesn't start with u-.
  EXPECT_FALSE(UnicodeExtension::FromString("a-myext").has_value());
  EXPECT_FALSE(UnicodeExtension::FromString("-u-ca-gregory").has_value());

  // Invalid: invalid attribute (too short - size 1).
  EXPECT_FALSE(UnicodeExtension::FromString("u-a").has_value());
  // Invalid: invalid attribute (too long - size 9).
  EXPECT_FALSE(UnicodeExtension::FromString("u-verylongattribute").has_value());

  // Invalid: keyword with invalid type (too short).
  EXPECT_FALSE(UnicodeExtension::FromString("u-ca-g").has_value());
}

TEST(Bcp47ExtensionTest, UnicodeExtensionHasKeyword) {
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext,
                       CreateUnicodeExtension("ca-gregory-kn-true"));
  EXPECT_TRUE(ext.GetKeywordValue("ca").has_value());
  EXPECT_TRUE(ext.GetKeywordValue("kn").has_value());
  EXPECT_FALSE(ext.GetKeywordValue("co").has_value());
  // These are not keywords (not size 2).
  EXPECT_FALSE(ext.GetKeywordValue("gregory").has_value());
  EXPECT_FALSE(ext.GetKeywordValue("true").has_value());
  EXPECT_FALSE(ext.GetKeywordValue("").has_value());
}

TEST(Bcp47ExtensionTest, UnicodeExtensionAttributesMethod) {
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext1,
                       CreateUnicodeExtension("attr1-attr2"));
  EXPECT_THAT(ext1.attributes(), ElementsAre("attr1", "attr2"));

  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext2,
                       CreateUnicodeExtension("attr2-attr1"));
  // flat_set will sort them.
  EXPECT_THAT(ext2.attributes(), ElementsAre("attr1", "attr2"));

  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext3,
                       CreateUnicodeExtension("ca-gregory"));
  EXPECT_THAT(ext3.attributes(), IsEmpty());
}

TEST(Bcp47ExtensionTest, UnicodeExtensionKeywordKeys) {
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext1,
                       CreateUnicodeExtension("ca-gregory-co-phonebk"));
  EXPECT_THAT(ext1.GetKeywordKeys(), ElementsAre("ca", "co"));

  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext2,
                       CreateUnicodeExtension("co-phonebk-ca-gregory"));
  // Should be sorted alphabetically.
  EXPECT_THAT(ext2.GetKeywordKeys(), ElementsAre("ca", "co"));

  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext3,
                       CreateUnicodeExtension("attr1-attr2"));
  EXPECT_THAT(ext3.GetKeywordKeys(), IsEmpty());
}

TEST(Bcp47ExtensionTest, UnicodeExtensionSubtagsStringComplex) {
  // Test ordering: attributes (sorted) then keywords (sorted).
  // "attr2-attr1" should be sorted to "attr1-attr2".
  ASSERT_OK_AND_ASSIGN(
      UnicodeExtension ext,
      CreateUnicodeExtension("attr2-attr1-co-phonebk-ca-gregory"));
  // Attributes: attr1, attr2.
  // Keywords: ca: gregory, co: phonebk.
  EXPECT_EQ(ext.SubtagsString(), "attr1-attr2-ca-gregory-co-phonebk");
}

TEST(Bcp47ExtensionTest, UnicodeExtensionDuplicates) {
  // Duplicate attributes: only one should remain.
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext1,
                       CreateUnicodeExtension("attr1-attr1-attr2"));
  EXPECT_THAT(ext1.attributes(), ElementsAre("attr1", "attr2"));

  // Duplicate keywords: the first one should win (based on flat_map::emplace).
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext2,
                       CreateUnicodeExtension("ca-gregory-ca-buddhist"));
  EXPECT_THAT(ext2.GetKeywordValue("ca"), Optional(Eq("gregory")));
  EXPECT_EQ(ext2.SubtagsString(), "ca-gregory");
}

TEST(Bcp47ExtensionTest, UnicodeExtensionMalformedInput) {
  // Spaces are not allowed.
  EXPECT_FALSE(UnicodeExtension::FromString("u-ca gregory").has_value());
  EXPECT_FALSE(UnicodeExtension::FromString("u- ca-gregory").has_value());
  EXPECT_FALSE(UnicodeExtension::FromString("u-ca-gregory ").has_value());

  // Special characters are not allowed.
  EXPECT_FALSE(UnicodeExtension::FromString("u-ca!gregory").has_value());
  EXPECT_FALSE(UnicodeExtension::FromString("u-ca@gregory").has_value());
  EXPECT_FALSE(UnicodeExtension::FromString("u-ca#gregory").has_value());

  // Empty subtags (e.g. double hyphens) are not allowed.
  EXPECT_FALSE(UnicodeExtension::FromString("u-ca--gregory").has_value());

  // Key MUST be exactly 2 characters.
  EXPECT_FALSE(UnicodeExtension::FromString("u-c-gregory").has_value());
  // The extension contains only attributes.
  EXPECT_THAT(UnicodeExtension::FromString("u-caa-gregory")->GetKeywordKeys(),
              IsEmpty());

  // Attribute/Type MUST be 3-8 characters ca-gr gets
  // identified as two different keys.
  EXPECT_TRUE(UnicodeExtension::FromString("u-ca-gr").has_value());
  EXPECT_FALSE(UnicodeExtension::FromString("u-ca-toolongtype").has_value());
  // Too short for attribute but "at" gets identified as a key.
  EXPECT_THAT(UnicodeExtension::FromString("u-at")->GetKeywordKeys(),
              ElementsAre("at"));
  EXPECT_FALSE(UnicodeExtension::FromString("u-toolongattribute").has_value());
}

TEST(Bcp47ExtensionTest, UnicodeExtensionMutation) {
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext,
                       CreateUnicodeExtension("ca-gregory"));
  EXPECT_EQ(ext.SubtagsString(), "ca-gregory");

  // Add attribute.
  EXPECT_TRUE(ext.AddAttribute("attr1"));
  EXPECT_TRUE(ext.has_attribute("attr1"));
  EXPECT_THAT(ext.attributes(), ElementsAre("attr1"));
  EXPECT_EQ(ext.SubtagsString(), "attr1-ca-gregory");

  // Add another attribute (should be sorted).
  EXPECT_TRUE(ext.AddAttribute("aaa"));
  EXPECT_THAT(ext.attributes(), ElementsAre("aaa", "attr1"));
  EXPECT_EQ(ext.SubtagsString(), "aaa-attr1-ca-gregory");

  // SetKeyword (new keyword).
  EXPECT_TRUE(ext.SetKeyword("co", "phonebk"));
  EXPECT_THAT(ext.GetKeywordValue("co"), Optional(Eq("phonebk")));
  EXPECT_EQ(ext.SubtagsString(), "aaa-attr1-ca-gregory-co-phonebk");

  // SetKeyword (update existing keyword).
  EXPECT_TRUE(ext.SetKeyword("ca", "buddhist"));
  EXPECT_THAT(ext.GetKeywordValue("ca"), Optional(Eq("buddhist")));
  EXPECT_EQ(ext.SubtagsString(), "aaa-attr1-ca-buddhist-co-phonebk");

  // SetKeyword (multi-subtag value).
  EXPECT_TRUE(ext.SetKeyword("co", "emoji-attr3"));
  EXPECT_THAT(ext.GetKeywordValue("co"), Optional(Eq("emoji-attr3")));
  EXPECT_EQ(ext.SubtagsString(), "aaa-attr1-ca-buddhist-co-emoji-attr3");

  // Validation: invalid attribute (too short) should be ignored.
  EXPECT_FALSE(ext.AddAttribute("at"));
  EXPECT_FALSE(ext.has_attribute("at"));
  EXPECT_EQ(ext.SubtagsString(), "aaa-attr1-ca-buddhist-co-emoji-attr3");

  // Validation: invalid keyword (too long) should be ignored.
  EXPECT_FALSE(ext.SetKeyword("ccc", "val"));
  EXPECT_FALSE(ext.GetKeywordValue("ccc").has_value());
  EXPECT_EQ(ext.SubtagsString(), "aaa-attr1-ca-buddhist-co-emoji-attr3");

  // Validation: invalid value (too short subtag) should be ignored.
  EXPECT_FALSE(ext.SetKeyword("nu", "a"));
  EXPECT_FALSE(ext.GetKeywordValue("nu").has_value());
  EXPECT_EQ(ext.SubtagsString(), "aaa-attr1-ca-buddhist-co-emoji-attr3");
}

TEST(Bcp47ExtensionTest, CopyAndMove) {
  ASSERT_OK_AND_ASSIGN(UnicodeExtension original,
                       CreateUnicodeExtension("attr1-ca-gregory"));
  EXPECT_TRUE(original.has_attribute("attr1"));
  EXPECT_THAT(original.GetKeywordValue("ca"), Optional(Eq("gregory")));

  // Copy
  UnicodeExtension copy = original;
  EXPECT_TRUE(copy.has_attribute("attr1"));
  EXPECT_THAT(copy.GetKeywordValue("ca"), Optional(Eq("gregory")));

  // Move
  UnicodeExtension moved = std::move(original);
  EXPECT_TRUE(moved.has_attribute("attr1"));
  EXPECT_THAT(moved.GetKeywordValue("ca"), Optional(Eq("gregory")));
}

TEST(Bcp47ExtensionTest, UnicodeExtensionCaseInsensitivity) {
  // Parsing should be case-insensitive and canonicalize to lowercase.
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext,
                       CreateUnicodeExtension("ATTR1-CA-GREGORY"));
  EXPECT_TRUE(ext.has_attribute("attr1"));
  EXPECT_TRUE(ext.has_attribute("ATTR1"));
  EXPECT_THAT(ext.GetKeywordValue("ca"), Optional(Eq("gregory")));
  EXPECT_THAT(ext.GetKeywordValue("CA"), Optional(Eq("gregory")));
  EXPECT_EQ(ext.SubtagsString(), "attr1-ca-gregory");

  // Mutators and lookups should be case-insensitive.
  ASSERT_OK_AND_ASSIGN(UnicodeExtension ext2, CreateUnicodeExtension("attr1"));
  EXPECT_TRUE(ext2.AddAttribute("ATTR2"));
  EXPECT_TRUE(ext2.has_attribute("attr2"));
  EXPECT_TRUE(ext2.SetKeyword("CO", "PHONEBK"));
  EXPECT_THAT(ext2.GetKeywordValue("co"), Optional(Eq("phonebk")));
  EXPECT_TRUE(ext2.has_keyword("CO"));
}

TEST(Bcp47ExtensionTest, ExtensionFromString) {
  // Valid.
  auto ext1 = Extension::FromString("a-myext");
  ASSERT_TRUE(ext1.has_value());
  EXPECT_EQ(ext1->singleton(), 'a');
  EXPECT_EQ(ext1->SubtagsString(), "myext");

  // Overload with span.
  std::vector<std::string_view> span_input1 = {"myext"};
  auto ext1_span = Extension::FromSubtags('a', span_input1);
  ASSERT_TRUE(ext1_span.has_value());
  EXPECT_EQ(ext1_span->singleton(), 'a');
  EXPECT_EQ(ext1_span->SubtagsString(), "myext");

  auto ext2 = Extension::FromString("b-sub1-sub2");
  ASSERT_TRUE(ext2.has_value());
  EXPECT_EQ(ext2->singleton(), 'b');
  EXPECT_EQ(ext2->SubtagsString(), "sub1-sub2");

  // Case insensitivity.
  auto ext3 = Extension::FromString("A-MYEXT");
  ASSERT_TRUE(ext3.has_value());
  EXPECT_EQ(ext3->singleton(), 'A');  // Singleton is kept as is?
  EXPECT_EQ(ext3->SubtagsString(), "myext");

  // Invalid: too short.
  EXPECT_FALSE(Extension::FromString("a-").has_value());
  EXPECT_FALSE(Extension::FromString("a").has_value());

  // Invalid: doesn't start with singleton-.
  EXPECT_FALSE(Extension::FromString("-a-myext").has_value());

  // Invalid: invalid subtag (too short).
  EXPECT_FALSE(Extension::FromString("a-at").has_value());
  // Invalid: invalid subtag (too long).
  EXPECT_FALSE(Extension::FromString("a-toolongsubtag").has_value());
}

TEST(Bcp47ExtensionTest, ExtensionMutation) {
  ASSERT_OK_AND_ASSIGN(Extension ext, Extension::FromString("a-abc"));
  EXPECT_EQ(ext.singleton(), 'a');
  EXPECT_EQ(ext.SubtagsString(), "abc");

  EXPECT_TRUE(ext.AddSubtag("sub1"));
  EXPECT_EQ(ext.SubtagsString(), "abc-sub1");

  EXPECT_TRUE(ext.AddSubtag("sub2"));
  EXPECT_EQ(ext.SubtagsString(), "abc-sub1-sub2");

  // Sorted.
  EXPECT_TRUE(ext.AddSubtag("aaa"));
  EXPECT_EQ(ext.SubtagsString(), "aaa-abc-sub1-sub2");

  // Case insensitive.
  EXPECT_TRUE(ext.AddSubtag("BBB"));
  EXPECT_EQ(ext.SubtagsString(), "aaa-abc-bbb-sub1-sub2");

  // Duplicates.
  EXPECT_TRUE(ext.AddSubtag("sub1"));
  EXPECT_EQ(ext.SubtagsString(), "aaa-abc-bbb-sub1-sub2");

  // Invalid.
  EXPECT_FALSE(ext.AddSubtag("at"));
  EXPECT_FALSE(ext.AddSubtag("toolongsubtag"));
}

TEST(Bcp47ExtensionTest, PrivateUseSubtagsFromString) {
  // Valid.
  auto ext1 = PrivateUseSubtags::FromString("x-private");
  ASSERT_TRUE(ext1.has_value());
  EXPECT_EQ(ext1->SubtagsString(), "private");

  // Overload with span.
  std::vector<std::string_view> span_input1 = {"private"};
  auto ext1_span = PrivateUseSubtags::FromSubtags(span_input1);
  ASSERT_TRUE(ext1_span.has_value());
  EXPECT_EQ(ext1_span->SubtagsString(), "private");

  auto ext2 = PrivateUseSubtags::FromString("x-sub1-sub2");
  ASSERT_TRUE(ext2.has_value());
  EXPECT_EQ(ext2->SubtagsString(), "sub1-sub2");

  // Without 'x-' prefix.
  auto ext3 = PrivateUseSubtags::FromString("onlysub");
  ASSERT_TRUE(ext3.has_value());
  EXPECT_EQ(ext3->SubtagsString(), "onlysub");

  // Case sensitive (private use subtags can be case sensitive, though often
  // treated as insensitive).
  // The current implementation uses flat_set<std::string, std::less<>> without
  // canonicalization for private use.
  auto ext4 = PrivateUseSubtags::FromString("x-Sub1");
  ASSERT_TRUE(ext4.has_value());
  EXPECT_EQ(ext4->SubtagsString(), "Sub1");

  // Invalid: empty subtag.
  EXPECT_FALSE(PrivateUseSubtags::FromString("x-").has_value());
  EXPECT_FALSE(PrivateUseSubtags::FromString("x--sub").has_value());
  // Valid: private-use subtag of length 1.
  EXPECT_TRUE(PrivateUseSubtags::FromString("x").has_value());
}

TEST(Bcp47ExtensionTest, PrivateUseSubtagsMutation) {
  ASSERT_OK_AND_ASSIGN(PrivateUseSubtags ext,
                       PrivateUseSubtags::FromString("x-sub1"));
  EXPECT_EQ(ext.SubtagsString(), "sub1");

  EXPECT_TRUE(ext.AddSubtag("sub2"));
  EXPECT_EQ(ext.SubtagsString(), "sub1-sub2");

  // Sorted.
  EXPECT_TRUE(ext.AddSubtag("aaa"));
  EXPECT_EQ(ext.SubtagsString(), "sub1-sub2-aaa");

  // Case is preserved.
  EXPECT_TRUE(ext.AddSubtag("BBB"));
  EXPECT_EQ(ext.SubtagsString(), "sub1-sub2-aaa-BBB");

  // Invalid.
  EXPECT_FALSE(ext.AddSubtag(""));
  EXPECT_FALSE(ext.AddSubtag("sub!"));
}

}  // namespace base::i18n
