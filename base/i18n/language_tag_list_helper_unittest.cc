// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_tag_list_helper.h"

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/i18n/language_tag.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(LanguageTagListHelperTest, GetLanguageTagListFromString) {
  EXPECT_THAT(GetLanguageTagListFromString("ja"),
              ElementsAre(GetKnownLanguageTag("ja")));
  EXPECT_THAT(
      GetLanguageTagListFromString("en-US,fr,zh-Hant-TW"),
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("fr"),
                  GetKnownLanguageTag("zh-Hant-TW")));
}

TEST(LanguageTagListHelperTest, GetLanguageTagListFromStringCanonicalizesTags) {
  EXPECT_THAT(
      GetLanguageTagListFromString("EN-us,en_GB,zh-hant-tw,iw"),
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("en-GB"),
                  GetKnownLanguageTag("zh-Hant-TW"),
                  GetKnownLanguageTag("he")));
}

TEST(LanguageTagListHelperTest, GetLanguageTagListFromStringTrimsWhitespace) {
  EXPECT_THAT(
      GetLanguageTagListFromString(" en-US ,\tfr\n,  ja"),
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("fr"),
                  GetKnownLanguageTag("ja")));
}

TEST(LanguageTagListHelperTest,
     GetLanguageTagListFromStringSkipsEmptyAndMalformedTags) {
  EXPECT_THAT(
      GetLanguageTagListFromString(
          ",en-US,, ,invalid---tag,x-private,e,toolongLanguageTag,fr,"),
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("fr")));
}

TEST(LanguageTagListHelperTest, GetLanguageTagListFromStringKeepsDuplicates) {
  EXPECT_THAT(GetLanguageTagListFromString("en,fr,EN"),
              ElementsAre(GetKnownLanguageTag("en"), GetKnownLanguageTag("fr"),
                          GetKnownLanguageTag("en")));
}

TEST(LanguageTagListHelperTest, GetLanguageTagListFromStringWithoutValidTags) {
  EXPECT_THAT(GetLanguageTagListFromString(""), IsEmpty());
  EXPECT_THAT(GetLanguageTagListFromString("   "), IsEmpty());
  EXPECT_THAT(GetLanguageTagListFromString(",,,"), IsEmpty());
  EXPECT_THAT(GetLanguageTagListFromString("invalid---tag"), IsEmpty());
}

TEST(LanguageTagListHelperTest, GetLanguageTagListFromStringWithFilter) {
  EXPECT_THAT(
      GetLanguageTagListFromString(
          "en-US,fr,de-DE,ja",
          [](const LanguageTag& tag) { return tag.region_subtag().empty(); }),
      ElementsAre(GetKnownLanguageTag("fr"), GetKnownLanguageTag("ja")));

  EXPECT_THAT(
      GetLanguageTagListFromString("en-US,fr",
                                   [](const LanguageTag&) { return true; }),
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("fr")));

  EXPECT_THAT(GetLanguageTagListFromString(
                  "en-US,fr", [](const LanguageTag&) { return false; }),
              IsEmpty());
}

TEST(LanguageTagListHelperTest, FilterIsCalledWithValidTagsInOrder) {
  std::vector<LanguageTag> filtered_tags;
  EXPECT_THAT(
      GetLanguageTagListFromString("ja, ,invalid---tag,EN-us,fr",
                                   [&filtered_tags](const LanguageTag& tag) {
                                     filtered_tags.push_back(tag);
                                     return tag != GetKnownLanguageTag("en-US");
                                   }),
      ElementsAre(GetKnownLanguageTag("ja"), GetKnownLanguageTag("fr")));

  // The filter is only called with the valid tags, after canonicalization.
  EXPECT_THAT(filtered_tags, ElementsAre(GetKnownLanguageTag("ja"),
                                         GetKnownLanguageTag("en-US"),
                                         GetKnownLanguageTag("fr")));
}

TEST(LanguageTagListHelperTest, FilterCanRemoveDuplicates) {
  base::flat_set<LanguageTag> seen_tags;
  EXPECT_THAT(
      GetLanguageTagListFromString("en-US,fr,en_US,FR,ja",
                                   [&seen_tags](const LanguageTag& tag) {
                                     return seen_tags.insert(tag).second;
                                   }),
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("fr"),
                  GetKnownLanguageTag("ja")));
}

TEST(LanguageTagListHelperTest, GetLanguageTagListFromStringList) {
  EXPECT_THAT(GetLanguageTagListFromStringList({}), IsEmpty());
  EXPECT_THAT(
      GetLanguageTagListFromStringList({"en-US", "fr", "zh-Hant-TW", "fr"}),
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("fr"),
                  GetKnownLanguageTag("zh-Hant-TW"),
                  GetKnownLanguageTag("fr")));
}

TEST(LanguageTagListHelperTest,
     GetLanguageTagListFromStringListCanonicalizesTags) {
  EXPECT_THAT(
      GetLanguageTagListFromStringList({"EN-us", "en_GB", "iw"}),
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("en-GB"),
                  GetKnownLanguageTag("he")));
}

TEST(LanguageTagListHelperTest,
     GetLanguageTagListFromStringListSkipsMalformedTags) {
  const std::vector<std::string> tag_strings = {
      "", "en-US", "invalid---tag", "x-private", "e", "fr"};
  EXPECT_THAT(
      GetLanguageTagListFromStringList(tag_strings),
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("fr")));
}

TEST(LanguageTagListHelperTest, GetLanguageTagListFromStringListWithFilter) {
  const std::vector<std::string> tag_strings = {"en-US", "fr", "invalid---tag",
                                                "de-DE", "ja"};
  std::vector<LanguageTag> filtered_tags;
  EXPECT_THAT(
      GetLanguageTagListFromStringList(
          tag_strings,
          [&filtered_tags](const LanguageTag& tag) {
            filtered_tags.push_back(tag);
            return tag.region_subtag().empty();
          }),
      ElementsAre(GetKnownLanguageTag("fr"), GetKnownLanguageTag("ja")));

  // The filter is only called with the valid tags, in order.
  EXPECT_THAT(
      filtered_tags,
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("fr"),
                  GetKnownLanguageTag("de-DE"), GetKnownLanguageTag("ja")));
}

TEST(LanguageTagListHelperTest, LanguageTagListToString) {
  EXPECT_EQ(LanguageTagListToString({}), "");
  EXPECT_EQ(LanguageTagListToString({GetKnownLanguageTag("en-US")}), "en-US");
  EXPECT_EQ(LanguageTagListToString({GetKnownLanguageTag("en-US"),
                                     GetKnownLanguageTag("fr"),
                                     GetKnownLanguageTag("zh-Hant-TW")}),
            "en-US,fr,zh-Hant-TW");

  const std::vector<LanguageTag> tags = {GetKnownLanguageTag("ja"),
                                         GetKnownLanguageTag("de-DE"),
                                         GetKnownLanguageTag("ja")};
  EXPECT_EQ(LanguageTagListToString(tags), "ja,de-DE,ja");
}

TEST(LanguageTagListHelperTest, RoundTrip) {
  const std::vector<LanguageTag> tags = GetLanguageTagListFromString(
      " en_us , FR,,zh-hant-tw,invalid---tag,en-US-u-ca-gregory");
  const std::string tags_string = LanguageTagListToString(tags);
  EXPECT_EQ(tags_string, "en-US,fr,zh-Hant-TW,en-US-u-ca-gregory");
  EXPECT_EQ(GetLanguageTagListFromString(tags_string), tags);
}

}  // namespace
}  // namespace base::i18n
