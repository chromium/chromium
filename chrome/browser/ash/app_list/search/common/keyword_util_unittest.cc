// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/keyword_util.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::list {

namespace {

// Test for successful matching of ONE keyword in query returns pair of
// {keyword, SearchProviders}.
TEST(KeywordUtilTest, OneKeyword) {
  KeywordToProvidersPairs p1 = {
      {u"app",
       {ProviderType::kInstalledApp, ProviderType::kArcAppShortcut,
        ProviderType::kPlayStoreApp}}};
  EXPECT_EQ(p1, ExtractKeyword(u"app test"));

  KeywordToProvidersPairs p2 = {{u"search", {ProviderType::kOmnibox}}};
  EXPECT_EQ(p2, ExtractKeyword(u"test search"));

  KeywordToProvidersPairs p3 = {
      {u"android",
       {ProviderType::kArcAppShortcut, ProviderType::kPlayStoreApp}}};
  EXPECT_EQ(p3, ExtractKeyword(u"/testing android */"));
}

// Test unsuccessful matching of keyword
TEST(KeywordUtilTest, NoKeyword) {
  EXPECT_EQ(KeywordToProvidersPairs(), ExtractKeyword(u"no keyword"));
  EXPECT_EQ(KeywordToProvidersPairs(), ExtractKeyword(u"searching driver"));
}

// Test for successful matching of multiple keywords in query
// Pairs of keywords-to-providers are ordered in the same order in which the
// keywords are displayed in the query e.g. For query "help app change
// brightness", the order of keyword pairs is {"help", "app"}
TEST(KeywordUtilTest, MultipleKeywords) {
  KeywordToProvidersPairs p1 = {
      {u"help", {ProviderType::kHelpApp}},
      {u"app",
       {ProviderType::kInstalledApp, ProviderType::kArcAppShortcut,
        ProviderType::kPlayStoreApp}}};

  EXPECT_EQ(p1, ExtractKeyword(u"help app change brightness"));

  KeywordToProvidersPairs p2 = {{u"google", {ProviderType::kOmnibox}},
                                {u"gaming", {ProviderType::kGames}},
                                {u"assistant", {ProviderType::kAssistantText}}};

  EXPECT_EQ(p2, ExtractKeyword(u"google gaming assistant"));
}

}  // namespace
}  // namespace app_list::list
