// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetched_pages_notifier.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

const ClientId kClientId("1234", kSuggestedArticlesNamespace);
const base::FilePath kFilePath("/");
const std::u16string kExampleHost = u"www.example.com";
const std::u16string kExampleHost2 = u"www.example2.com";
const std::u16string kExampleHost3 = u"www.example3.com";

OfflinePageItem ItemCreatedOn(std::u16string host, base::Time creation_time) {
  static int offline_id = 0;

  std::u16string scheme = u"https://";
  GURL url(scheme + host);

  OfflinePageItem item(url, ++offline_id, kClientId, kFilePath, 0);
  item.creation_time = creation_time;
  return item;
}

}  // namespace

TEST(PrefetchedPagesNotifierTest, CheckEmptyList) {
  std::vector<OfflinePageItem> empty_list = {};
  EXPECT_EQ(std::u16string(), ExtractRelevantHostFromOfflinePageItemList(
                                  base::Time(), empty_list));
}

TEST(PrefetchedPagesNotifierTest, CheckPageCreatedAfterTimes) {
  base::Time now = base::Time::Now();
  base::Time past = now - base::TimeDelta::FromSeconds(500);
  base::Time future = now + base::TimeDelta::FromSeconds(500);
  std::vector<OfflinePageItem> single_list = {ItemCreatedOn(kExampleHost, now)};

  EXPECT_EQ(kExampleHost,
            ExtractRelevantHostFromOfflinePageItemList(past, single_list));
  EXPECT_EQ(std::u16string(),
            ExtractRelevantHostFromOfflinePageItemList(future, single_list));

  // Should not crash
  EXPECT_EQ(kExampleHost, ExtractRelevantHostFromOfflinePageItemList(
                              base::Time(), single_list));
  EXPECT_EQ(kExampleHost, ExtractRelevantHostFromOfflinePageItemList(
                              base::Time::Min(), single_list));
  EXPECT_EQ(std::u16string(), ExtractRelevantHostFromOfflinePageItemList(
                                  base::Time::Max(), single_list));
}

TEST(PrefetchedPagesNotifierTest, CheckFilteredList) {
  base::Time now = base::Time::Now();
  base::Time past = now - base::TimeDelta::FromSeconds(500);
  base::Time future = now + base::TimeDelta::FromSeconds(500);
  base::Time more_future = now + base::TimeDelta::FromSeconds(1000);

  std::vector<OfflinePageItem> item_list = {
      ItemCreatedOn(kExampleHost, past), ItemCreatedOn(kExampleHost2, now),
      ItemCreatedOn(kExampleHost3, future)};

  EXPECT_EQ(kExampleHost3,
            ExtractRelevantHostFromOfflinePageItemList(past, item_list));
  EXPECT_EQ(kExampleHost3,
            ExtractRelevantHostFromOfflinePageItemList(now, item_list));
  EXPECT_EQ(kExampleHost3,
            ExtractRelevantHostFromOfflinePageItemList(future, item_list));
  EXPECT_EQ(std::u16string(),
            ExtractRelevantHostFromOfflinePageItemList(more_future, item_list));
  EXPECT_EQ(std::u16string(), ExtractRelevantHostFromOfflinePageItemList(
                                  base::Time::Max(), item_list));
}

}  // namespace offline_pages
