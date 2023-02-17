// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/keyword_util.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/logging.h"

namespace app_list::list {

namespace {

/*********************** Exact String Matching Tests ***********************/

// Test for successful EXACT matching of ONE keyword in query returns pair of
// {keyword, SearchProviders}.
TEST(KeywordUtilTest, OneExactKeyword) {
  KeywordExtractedInfoList expected_1 = {
      KeywordInfo(u"app", /*relevance_score*/ 1.0,
                  {ProviderType::kInstalledApp, ProviderType::kArcAppShortcut,
                   ProviderType::kPlayStoreApp})};

  EXPECT_EQ(expected_1, ExtractKeywords(u"app test"));

  KeywordExtractedInfoList expected_2 = {
      {u"search", /*relevance_score*/ 1.0, {ProviderType::kOmnibox}}};
  EXPECT_EQ(expected_2, ExtractKeywords(u"test search"));

  KeywordExtractedInfoList expected_3 = {
      {u"android",
       /*relevance_score*/ 1.0,
       {ProviderType::kArcAppShortcut, ProviderType::kPlayStoreApp}}};
  EXPECT_EQ(expected_3, ExtractKeywords(u"/testing android */"));
}

// Test unsuccessful matching of keyword.
TEST(KeywordUtilTest, NoKeyword) {
  EXPECT_EQ(KeywordExtractedInfoList(), ExtractKeywords(u"no keyword"));
  EXPECT_EQ(KeywordExtractedInfoList(), ExtractKeywords(u"dskfhwidkj"));
  EXPECT_EQ(KeywordExtractedInfoList(), ExtractKeywords(u"beehives"));
  EXPECT_EQ(KeywordExtractedInfoList(), ExtractKeywords(u"kelp"));
}

// Test for successful matching of multiple keywords in query
// Pairs of keywords-to-providers are ordered in the same order in which the
// keywords are displayed in the query e.g. For query "help app change
// brightness", the order of keyword pairs is {"help", "app"}.
TEST(KeywordUtilTest, MultipleExactKeywords) {
  KeywordExtractedInfoList expected_1 = {
      {u"help", /*relevance_score*/ 1.0, {ProviderType::kHelpApp}},
      {u"app",
       1.0,
       {ProviderType::kInstalledApp, ProviderType::kArcAppShortcut,
        ProviderType::kPlayStoreApp}}};

  EXPECT_EQ(expected_1, ExtractKeywords(u"help app change brightness"));

  KeywordExtractedInfoList expected_2 = {
      {u"google", /*relevance_score*/ 1.0, {ProviderType::kOmnibox}},
      {u"gaming", /*relevance_score*/ 1.0, {ProviderType::kGames}},
      {u"assistant", /*relevance_score*/ 1.0, {ProviderType::kAssistantText}}};

  EXPECT_EQ(expected_2, ExtractKeywords(u"google gaming assistant"));
}

/*********************** Fuzzy String Matching Tests ***********************/

// Test for one successful fuzzy matching of keywords.
TEST(KeywordUtilTest, OneFuzzyKeyword) {
  KeywordExtractedInfoList actual_1 = ExtractKeywords(u"searchd boba");

  EXPECT_EQ(size_t{1}, actual_1.size());
  EXPECT_EQ(u"searchd", actual_1[0].query_token);
  EXPECT_EQ(std::vector<ProviderType>{ProviderType::kOmnibox},
            actual_1[0].search_providers);

  // Test for British english spelling of "personalisation" is able to match
  // with our canonical keyword which uses American spelling of
  // "personalization".
  KeywordExtractedInfoList actual_2 =
      ExtractKeywords(u"personalisation background");

  EXPECT_EQ(size_t{1}, actual_2.size());
  EXPECT_EQ(u"personalisation", actual_2[0].query_token);
  EXPECT_EQ(std::vector<ProviderType>{ProviderType::kPersonalization},
            actual_2[0].search_providers);

  KeywordExtractedInfoList actual_3 = ExtractKeywords(u"minecraft ap");

  EXPECT_EQ(size_t{1}, actual_3.size());
  EXPECT_EQ(u"ap", actual_3[0].query_token);
  EXPECT_EQ((std::vector<ProviderType>{ProviderType::kInstalledApp,
                                       ProviderType::kArcAppShortcut,
                                       ProviderType::kPlayStoreApp}),
            actual_3[0].search_providers);
}

// Test for multiple successful fuzzy matching of keywords.
TEST(KeywordUtilTest, MultipleFuzzyKeywords) {
  KeywordExtractedInfoList actual_1 = ExtractKeywords(u"setting keyboord");

  EXPECT_EQ(size_t{2}, actual_1.size());
  EXPECT_EQ(u"setting", actual_1[0].query_token);
  EXPECT_EQ(std::vector<ProviderType>{ProviderType::kOsSettings},
            actual_1[0].search_providers);

  EXPECT_EQ(u"keyboord", actual_1[1].query_token);
  EXPECT_EQ(std::vector<ProviderType>{ProviderType::kKeyboardShortcut},
            actual_1[1].search_providers);
}

// Test for both fuzzy and exact matching of keywords.
TEST(KeywordUtilTest, ExactAndFuzzyMatching) {
  KeywordExtractedInfoList actual_1 = ExtractKeywords(u"google asistant");

  EXPECT_EQ(size_t{2}, actual_1.size());
  // Check the first keyword "google" is exact match.
  EXPECT_EQ(
      KeywordInfo(u"google", /*relevance_score*/ 1.0, {ProviderType::kOmnibox}),
      actual_1[0]);
  // Check the second keyword "asisstant" is fuzzily matched.
  EXPECT_EQ(u"asistant", actual_1[1].query_token);
  EXPECT_EQ(std::vector<ProviderType>{ProviderType::kAssistantText},
            actual_1[1].search_providers);
}

/*********************** Query Stripping Tests ***********************/

// Test stripping one keyword from query
TEST(KeywordUtilTest, StripOneKeyword) {
  std::u16string query1 = u"strip keyboard from query";
  EXPECT_EQ(StripQuery(query1), u"strip from query");

  std::u16string query2 = u"help";
  EXPECT_EQ(StripQuery(query2), u"");

  std::u16string query3 = u"personalization background";
  EXPECT_EQ(StripQuery(query3), u"background");
}

// Test stripping multiple keywords form query
TEST(KeywordUtilTest, StripMultipleKeywords) {
  std::u16string query1 =
      u"strip keyboard random word in between files from query";
  EXPECT_EQ(StripQuery(query1), u"strip random word in between from query");

  std::u16string query2 = u"google gaming minecraft";
  EXPECT_EQ(StripQuery(query2), u"minecraft");

  std::u16string query3 = u"file cat drive boba gaming tetris";
  EXPECT_EQ(StripQuery(query3), u"cat boba tetris");
}

}  // namespace
}  // namespace app_list::list
