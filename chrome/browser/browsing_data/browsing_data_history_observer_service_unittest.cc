// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_history_observer_service.h"

#include <set>
#include <utility>

#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

struct RemovalData {
  uint32_t removal_mask = 0;
  uint32_t quota_storage_removal_mask = 0;
  content::StoragePartition::OriginMatcherFunction origin_matcher;
  base::Time begin;
  base::Time end;
};

class RemovalDataTestStoragePartition : public content::TestStoragePartition {
 public:
  RemovalDataTestStoragePartition() = default;
  ~RemovalDataTestStoragePartition() override = default;

  void ClearData(uint32_t removal_mask,
                 uint32_t quota_storage_removal_mask,
                 OriginMatcherFunction origin_matcher,
                 network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
                 bool perform_storage_cleanup,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override {
    RemovalData removal_data;
    removal_data.removal_mask = removal_mask;
    removal_data.quota_storage_removal_mask = quota_storage_removal_mask;
    removal_data.origin_matcher = std::move(origin_matcher);
    removal_data.begin = begin;
    removal_data.end = end;
    removal_data_ = removal_data;

    std::move(callback).Run();
  }

  const base::Optional<RemovalData>& GetRemovalData() { return removal_data_; }

 private:
  base::Optional<RemovalData> removal_data_;
};

}  // namespace

class BrowsingDataHistoryObserverServiceTest : public testing::Test {
 public:
  BrowsingDataHistoryObserverServiceTest() = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowsingDataHistoryObserverServiceTest, AllHistoryDeleted_DataCleared) {
  TestingProfile profile;
  BrowsingDataHistoryObserverService service(&profile);
  RemovalDataTestStoragePartition partition;
  service.OverrideStoragePartitionForTesting(&partition);

  service.OnURLsDeleted(nullptr /* history_service */,
                        history::DeletionInfo::ForAllHistory());

  const base::Optional<RemovalData>& removal_data = partition.GetRemovalData();
  EXPECT_TRUE(removal_data.has_value());
  EXPECT_EQ(content::StoragePartition::REMOVE_DATA_MASK_CONVERSIONS,
            removal_data->removal_mask);
  EXPECT_EQ(0u, removal_data->quota_storage_removal_mask);
  EXPECT_EQ(base::Time(), removal_data->begin);
  EXPECT_EQ(base::Time::Max(), removal_data->end);

  // A null origin matcher indicates to remove all origins.
  EXPECT_TRUE(removal_data->origin_matcher.is_null());
}

TEST_F(BrowsingDataHistoryObserverServiceTest,
       OriginCompletelyDeleted_OriginDataCleared) {
  TestingProfile profile;
  BrowsingDataHistoryObserverService service(&profile);
  RemovalDataTestStoragePartition partition;
  service.OverrideStoragePartitionForTesting(&partition);

  GURL origin_a = GURL("https://a.test");
  GURL origin_b = GURL("https://b.test");

  // Create a map which shows that `origin_a` is completely deleted and
  // `origin_b` is not.
  history::OriginCountAndLastVisitMap origin_map;
  origin_map[origin_a] = std::make_pair(0, base::Time());
  origin_map[origin_b] = std::make_pair(1, base::Time());

  history::DeletionInfo deletion_info = history::DeletionInfo::ForUrls(
      {} /* deleted_rows */, {} /* favicon_urls */);
  deletion_info.set_deleted_urls_origin_map(std::move(origin_map));

  service.OnURLsDeleted(nullptr /* history_service */, deletion_info);

  const base::Optional<RemovalData>& removal_data = partition.GetRemovalData();
  EXPECT_TRUE(removal_data.has_value());

  EXPECT_EQ(base::Time(), removal_data->begin);
  EXPECT_EQ(base::Time::Max(), removal_data->end);

  // Data for `origin_a` should be cleared, but not for `origin_b`.
  EXPECT_TRUE(removal_data->origin_matcher.Run(
      url::Origin::Create(origin_a), nullptr /* special_storage_policy */));
  EXPECT_FALSE(removal_data->origin_matcher.Run(
      url::Origin::Create(origin_b), nullptr /* special_storage_policy */));
}

TEST_F(BrowsingDataHistoryObserverServiceTest,
       TimeRangeHistoryDeleted_DataCleared) {
  TestingProfile profile;
  BrowsingDataHistoryObserverService service(&profile);
  RemovalDataTestStoragePartition partition;
  service.OverrideStoragePartitionForTesting(&partition);

  base::Time begin = base::Time::Now();
  base::Time end = begin + base::TimeDelta::FromDays(1);
  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(begin, end), false /* is_from_expiration */,
      {} /* deleted_rows */, {} /* favicon_urls */,
      base::nullopt /* restrict_urls */);

  service.OnURLsDeleted(nullptr /* history_service */, deletion_info);

  const base::Optional<RemovalData>& removal_data = partition.GetRemovalData();
  EXPECT_TRUE(removal_data.has_value());

  EXPECT_EQ(begin, removal_data->begin);
  EXPECT_EQ(end, removal_data->end);
  EXPECT_TRUE(removal_data->origin_matcher.is_null());
}

TEST_F(BrowsingDataHistoryObserverServiceTest,
       TimeRangeHistoryWithRestrictions_DataCleared) {
  TestingProfile profile;
  BrowsingDataHistoryObserverService service(&profile);
  RemovalDataTestStoragePartition partition;
  service.OverrideStoragePartitionForTesting(&partition);

  GURL origin_a = GURL("https://a.test");
  GURL origin_b = GURL("https://b.test");

  std::set<GURL> restrict_urls = {origin_a};

  base::Time begin = base::Time::Now();
  base::Time end = begin + base::TimeDelta::FromDays(1);
  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(begin, end), false /* is_from_expiration */,
      {} /* deleted_rows */, {} /* favicon_urls */,
      restrict_urls /* restrict_urls */);

  service.OnURLsDeleted(nullptr /* history_service */, deletion_info);

  const base::Optional<RemovalData>& removal_data = partition.GetRemovalData();
  EXPECT_TRUE(removal_data.has_value());

  EXPECT_EQ(begin, removal_data->begin);
  EXPECT_EQ(end, removal_data->end);
  EXPECT_FALSE(removal_data->origin_matcher.is_null());

  // Data for `origin_a` should be cleared, but not for `origin_b`.
  EXPECT_TRUE(removal_data->origin_matcher.Run(
      url::Origin::Create(origin_a), nullptr /* special_storage_policy */));
  EXPECT_FALSE(removal_data->origin_matcher.Run(
      url::Origin::Create(origin_b), nullptr /* special_storage_policy */));
}
