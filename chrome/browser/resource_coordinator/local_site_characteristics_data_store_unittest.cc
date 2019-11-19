// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store.h"

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_inspector.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_unittest_utils.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_coordinator {

namespace {

const url::Origin kTestOrigin = url::Origin::Create(GURL("http://www.foo.com"));
const url::Origin kTestOrigin2 =
    url::Origin::Create(GURL("http://www.bar.com"));

constexpr base::TimeDelta kDelay = base::TimeDelta::FromMinutes(1);

class MockLocalSiteCharacteristicsDatabase
    : public testing::NoopLocalSiteCharacteristicsDatabase {
 public:
  MockLocalSiteCharacteristicsDatabase() = default;
  ~MockLocalSiteCharacteristicsDatabase() = default;

  MOCK_METHOD1(RemoveSiteCharacteristicsFromDB,
               void(const std::vector<url::Origin>&));
  MOCK_METHOD0(ClearDatabase, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLocalSiteCharacteristicsDatabase);
};

}  // namespace

class LocalSiteCharacteristicsDataStoreTest : public ::testing::Test {
 protected:
  LocalSiteCharacteristicsDataStoreTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSiteCharacteristicsDatabase);
    data_store_ =
        std::make_unique<LocalSiteCharacteristicsDataStore>(&profile_);
    mock_db_ =
        new ::testing::StrictMock<MockLocalSiteCharacteristicsDatabase>();
    data_store_->SetDatabaseForTesting(base::WrapUnique(mock_db_));
    test_clock_.SetNowTicks(base::TimeTicks::UnixEpoch());
    test_clock_.Advance(base::TimeDelta::FromHours(1));
    WaitForAsyncOperationsToComplete();
  }

  void TearDown() override { WaitForAsyncOperationsToComplete(); }

  void WaitForAsyncOperationsToComplete() { task_environment_.RunUntilIdle(); }

  // Populates |writer_|, |reader_| and |data_| to refer to a tab navigated to
  // |kTestOrigin| that updated its title in background. Populates |writer2_|,
  // |reader2_| and |data2_| to refer to a tab navigated to |kTestOrigin2| that
  // updates its favicon in background.
  void SetupTwoSitesUsingFeaturesInBackground() {
    // Load a first origin, and then make use of a feature on it.
    ASSERT_FALSE(reader_);
    reader_ = data_store_->GetReaderForOrigin(kTestOrigin);
    EXPECT_TRUE(reader_);

    ASSERT_FALSE(writer_);
    writer_ = data_store_->GetWriterForOrigin(
        kTestOrigin, performance_manager::TabVisibility::kBackground);
    EXPECT_TRUE(writer_);

    ASSERT_FALSE(data_);
    data_ =
        data_store_->origin_data_map_for_testing().find(kTestOrigin)->second;
    EXPECT_TRUE(data_);

    EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
              reader_->UpdatesTitleInBackground());
    writer_->NotifySiteLoaded();
    writer_->NotifyUpdatesTitleInBackground();
    EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
              reader_->UpdatesTitleInBackground());
    test_clock_.Advance(kDelay);

    // Load a second origin, make use of a feature on it too.
    ASSERT_FALSE(reader2_);
    reader2_ = data_store_->GetReaderForOrigin(kTestOrigin2);
    EXPECT_TRUE(reader2_);

    ASSERT_FALSE(writer2_);
    writer2_ = data_store_->GetWriterForOrigin(
        kTestOrigin2, performance_manager::TabVisibility::kBackground);
    EXPECT_TRUE(writer2_);

    ASSERT_FALSE(data2_);
    data2_ =
        data_store_->origin_data_map_for_testing().find(kTestOrigin2)->second;
    EXPECT_TRUE(data2_);

    EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
              reader2_->UpdatesFaviconInBackground());
    writer2_->NotifySiteLoaded();
    writer2_->NotifyUpdatesFaviconInBackground();
    EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
              reader2_->UpdatesFaviconInBackground());
    test_clock_.Advance(kDelay);
  }

  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;

  // Owned by |data_store_|.
  ::testing::StrictMock<MockLocalSiteCharacteristicsDatabase>* mock_db_ =
      nullptr;
  std::unique_ptr<LocalSiteCharacteristicsDataStore> data_store_;

  std::unique_ptr<SiteCharacteristicsDataReader> reader_;
  std::unique_ptr<SiteCharacteristicsDataWriter> writer_;
  internal::LocalSiteCharacteristicsDataImpl* data_ = nullptr;

  std::unique_ptr<SiteCharacteristicsDataReader> reader2_;
  std::unique_ptr<SiteCharacteristicsDataWriter> writer2_;
  internal::LocalSiteCharacteristicsDataImpl* data2_ = nullptr;
};

TEST_F(LocalSiteCharacteristicsDataStoreTest, EndToEnd) {
  auto reader = data_store_->GetReaderForOrigin(kTestOrigin);
  EXPECT_TRUE(reader);
  auto writer = data_store_->GetWriterForOrigin(
      kTestOrigin, performance_manager::TabVisibility::kBackground);
  EXPECT_TRUE(writer);

  EXPECT_EQ(1U, data_store_->origin_data_map_for_testing().size());

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  writer->NotifySiteLoaded();
  writer->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());
  writer->NotifySiteUnloaded();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());

  auto reader_copy = data_store_->GetReaderForOrigin(kTestOrigin);
  EXPECT_EQ(1U, data_store_->origin_data_map_for_testing().size());
  auto reader2 = data_store_->GetReaderForOrigin(kTestOrigin2);
  EXPECT_EQ(2U, data_store_->origin_data_map_for_testing().size());
  reader2.reset();

  WaitForAsyncOperationsToComplete();
  EXPECT_EQ(1U, data_store_->origin_data_map_for_testing().size());
  reader_copy.reset();

  reader.reset();
  writer.reset();
  EXPECT_TRUE(data_store_->origin_data_map_for_testing().empty());

  EXPECT_CALL(*mock_db_, ClearDatabase());
  data_store_->OnURLsDeleted(nullptr, history::DeletionInfo::ForAllHistory());
}

// Verify that an origin is removed from the data store (in memory and on disk)
// when there are no more references to it in the history, after the history is
// partially cleared.
TEST_F(LocalSiteCharacteristicsDataStoreTest,
       OnURLsDeleted_Partial_OriginNotReferenced) {
  SetupTwoSitesUsingFeaturesInBackground();

  const base::TimeDelta last_loaded_time2_before_urls_deleted =
      data2_->last_loaded_time_for_testing();

  // Make sure that all data passed to |OnURLsDeleted| get passed to the
  // database, even if they're not in the internal map used by the data store.
  const url::Origin kOriginNotInMap =
      url::Origin::Create(GURL("http://www.url-not-in-map.com"));
  history::URLRows urls_to_delete = {history::URLRow(kTestOrigin.GetURL()),
                                     history::URLRow(kOriginNotInMap.GetURL())};
  history::DeletionInfo deletion_info =
      history::DeletionInfo::ForUrls(urls_to_delete, std::set<GURL>());
  deletion_info.set_deleted_urls_origin_map({
      {kTestOrigin.GetURL(), {0, base::Time::Now()}},
      {kOriginNotInMap.GetURL(), {0, base::Time::Now()}},
  });
  EXPECT_CALL(*mock_db_,
              RemoveSiteCharacteristicsFromDB(::testing::WhenSorted(
                  ::testing::ElementsAre(kTestOrigin, kOriginNotInMap))));
  data_store_->OnURLsDeleted(nullptr, deletion_info);
  ::testing::Mock::VerifyAndClear(mock_db_);

  // The information for the first site should have been cleared. The last
  // loaded time should be equal to the current time.
  EXPECT_EQ(data_->last_loaded_time_for_testing(),
            test_clock_.NowTicks() - base::TimeTicks::UnixEpoch());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader_->UpdatesTitleInBackground());
  // The second site shouldn't have been cleared.
  EXPECT_EQ(data2_->last_loaded_time_for_testing(),
            last_loaded_time2_before_urls_deleted);
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader2_->UpdatesFaviconInBackground());

  writer_->NotifySiteUnloaded();
  writer2_->NotifySiteUnloaded();
}

// Verify that an origin is *not* removed from the data store (in memory and on
// disk) when there remain references to it in the history, after the history is
// partially cleared.
TEST_F(LocalSiteCharacteristicsDataStoreTest,
       OnURLsDeleted_Partial_OriginStillReferenced) {
  SetupTwoSitesUsingFeaturesInBackground();

  const base::TimeDelta last_loaded_time_before_urls_deleted =
      data_->last_loaded_time_for_testing();
  const base::TimeDelta last_loaded_time2_before_urls_deleted =
      data2_->last_loaded_time_for_testing();

  // Make sure that all data passed to |OnURLsDeleted| get passed to the
  // database, even if they're not in the internal map used by the data store.
  const url::Origin kOriginNotInMap =
      url::Origin::Create(GURL("http://www.url-not-in-map.com"));
  history::URLRows urls_to_delete = {history::URLRow(kTestOrigin.GetURL()),
                                     history::URLRow(kOriginNotInMap.GetURL())};
  history::DeletionInfo deletion_info =
      history::DeletionInfo::ForUrls(urls_to_delete, std::set<GURL>());
  deletion_info.set_deleted_urls_origin_map({
      {kTestOrigin.GetURL(), {4, base::Time::Now()}},
      {kOriginNotInMap.GetURL(), {3, base::Time::Now()}},
  });
  data_store_->OnURLsDeleted(nullptr, deletion_info);
  ::testing::Mock::VerifyAndClear(mock_db_);

  // Sites shouldn't have been cleared.
  EXPECT_EQ(data_->last_loaded_time_for_testing(),
            last_loaded_time_before_urls_deleted);
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader_->UpdatesTitleInBackground());
  EXPECT_EQ(data2_->last_loaded_time_for_testing(),
            last_loaded_time2_before_urls_deleted);
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader2_->UpdatesFaviconInBackground());

  writer_->NotifySiteUnloaded();
  writer2_->NotifySiteUnloaded();
}

// Verify that origins are removed from the data store (in memory and on disk)
// when the history is completely cleared.
TEST_F(LocalSiteCharacteristicsDataStoreTest, OnURLsDeleted_Full) {
  SetupTwoSitesUsingFeaturesInBackground();

  // Delete all the information stored in the data store.
  EXPECT_CALL(*mock_db_, ClearDatabase());
  data_store_->OnURLsDeleted(nullptr, history::DeletionInfo::ForAllHistory());
  ::testing::Mock::VerifyAndClear(mock_db_);

  // The information for both sites should have been cleared.
  EXPECT_EQ(data_->last_loaded_time_for_testing(),
            test_clock_.NowTicks() - base::TimeTicks::UnixEpoch());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader_->UpdatesTitleInBackground());
  EXPECT_EQ(data2_->last_loaded_time_for_testing(),
            test_clock_.NowTicks() - base::TimeTicks::UnixEpoch());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader2_->UpdatesFaviconInBackground());

  writer_->NotifySiteUnloaded();
  writer2_->NotifySiteUnloaded();
}

TEST_F(LocalSiteCharacteristicsDataStoreTest, InspectorWorks) {
  // Make sure the inspector interface was registered at construction.
  LocalSiteCharacteristicsDataStoreInspector* inspector =
      LocalSiteCharacteristicsDataStoreInspector::GetForProfile(&profile_);
  EXPECT_NE(nullptr, inspector);
  EXPECT_EQ(data_store_.get(), inspector);

  EXPECT_STREQ("LocalSiteCharacteristicsDataStore",
               inspector->GetDataStoreName());

  // We expect an empty data store at the outset.
  EXPECT_EQ(0U, inspector->GetAllInMemoryOrigins().size());
  std::unique_ptr<SiteDataProto> data;
  bool is_dirty = false;
  EXPECT_FALSE(inspector->GetDataForOrigin(kTestOrigin, &is_dirty, &data));
  EXPECT_FALSE(is_dirty);
  EXPECT_EQ(nullptr, data.get());

  {
    // Add an entry, see that it's reflected in the inspector interface.
    auto writer = data_store_->GetWriterForOrigin(
        kTestOrigin, performance_manager::TabVisibility::kBackground);

    EXPECT_EQ(1U, inspector->GetAllInMemoryOrigins().size());
    EXPECT_TRUE(inspector->GetDataForOrigin(kTestOrigin, &is_dirty, &data));
    EXPECT_FALSE(is_dirty);
    ASSERT_NE(nullptr, data.get());

    // Touch the underlying data, see that the dirty bit updates.
    writer->NotifySiteLoaded();
    EXPECT_TRUE(inspector->GetDataForOrigin(kTestOrigin, &is_dirty, &data));
    EXPECT_TRUE(is_dirty);
  }

  // Make sure the interface is unregistered from the profile on destruction.
  data_store_.reset();
  EXPECT_EQ(nullptr, LocalSiteCharacteristicsDataStoreInspector::GetForProfile(
                         &profile_));
}

}  // namespace resource_coordinator
