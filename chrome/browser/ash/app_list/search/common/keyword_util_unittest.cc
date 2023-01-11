// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/keyword_util.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::list {

namespace {

TEST(KeywordUtilTest, ExtractKeyword) {
  // Test for successful matching of keyword in query returns pair of
  // {keyword, SearchProviders}.

  KeywordToProvidersPair p1 = {
      u"app",
      {ProviderType::kInstalledApp, ProviderType::kArcAppShortcut,
       ProviderType::kPlayStoreApp}};
  EXPECT_EQ(p1, ExtractKeyword(u"app test"));

  KeywordToProvidersPair p2 = {u"search", {ProviderType::kOmnibox}};
  EXPECT_EQ(p2, ExtractKeyword(u"test searching"));

  KeywordToProvidersPair p3 = {
      u"android", {ProviderType::kArcAppShortcut, ProviderType::kPlayStoreApp}};
  EXPECT_EQ(p3, ExtractKeyword(u"/testing android */"));

  // Test unsuccessful matching of keyword

  EXPECT_EQ(KeywordToProvidersPair(), ExtractKeyword(u"no keyword"));
}

}  // namespace
}  // namespace app_list::list
