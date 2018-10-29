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
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_coordinator {

namespace {

const url::Origin kTestOrigin = url::Origin::Create(GURL("http://www.foo.com"));
const url::Origin kTestOrigin2 =
    url::Origin::Create(GURL("http://www.bar.com"));

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
 public:
  LocalSiteCharacteristicsDataStoreTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSiteCharacteristicsDatabase);
    data_store_ =
        std::make_unique<LocalSiteCharacteristicsDataStore>(&profile_);
    test_clock_.SetNowTicks(base::TimeTicks::UnixEpoch());
    test_clock_.Advance(base::TimeDelta::FromHours(1));
    WaitForAsyncOperationsToComplete();
  }

  void TearDown() override { WaitForAsyncOperationsToComplete(); }

  void WaitForAsyncOperationsToComplete() {
    test_browser_thread_bundle_.RunUntilIdle();
  }

 protected:
  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
  std::unique_ptr<LocalSiteCharacteristicsDataStore> data_store_;
};

TEST_F(LocalSiteCharacteristicsDataStoreTest, EndToEnd) {
  auto reader = data_store_->GetReaderForOrigin(kTestOrigin);
  EXPECT_TRUE(reader);
  auto writer =
      data_store_->GetWriterForOrigin(kTestOrigin, TabVisibility::kBackground);
  EXPECT_TRUE(writer);

  EXPECT_EQ(1U, data_store_->origin_data_map_for_testing().size());

  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  writer->NotifySiteLoaded();
  writer->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());
  writer->NotifySiteUnloaded();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
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

  data_store_->OnURLsDeleted(nullptr, history::DeletionInfo::ForAllHistory());
}

TEST_F(LocalSiteCharacteristicsDataStoreTest, HistoryServiceObserver) {
  // Mock the database to ensure that the delete events get propagated properly.
  ::testing::StrictMock<MockLocalSiteCharacteristicsDatabase>* mock_db =
      new ::testing::StrictMock<MockLocalSiteCharacteristicsDatabase>();
  data_store_->SetDatabaseForTesting(base::WrapUnique(mock_db));

  // Load a first origin, and then make use of a feature on it.

  auto reader = data_store_->GetReaderForOrigin(kTestOrigin);
  EXPECT_TRUE(reader);

  auto writer =
      data_store_->GetWriterForOrigin(kTestOrigin, TabVisibility::kBackground);
  EXPECT_TRUE(writer);

  internal::LocalSiteCharacteristicsDataImpl* data =
      data_store_->origin_data_map_for_testing().find(kTestOrigin)->second;
  EXPECT_TRUE(data);

  constexpr base::TimeDelta kDelay = base::TimeDelta::FromHours(1);

  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  writer->NotifySiteLoaded();
  base::TimeDelta last_loaded_time = data->last_loaded_time_for_testing();
  writer->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());
  test_clock_.Advance(kDelay);

  // Load a second origin, make use of a feature on it too.
  auto reader2 = data_store_->GetReaderForOrigin(kTestOrigin2);
  EXPECT_TRUE(reader2);
  auto writer2 =
      data_store_->GetWriterForOrigin(kTestOrigin2, TabVisibility::kBackground);
  EXPECT_TRUE(writer2);
  writer2->NotifySiteLoaded();
  writer2->NotifyUpdatesFaviconInBackground();

  // This site hasn'be been unloaded yet, so the last loaded time shouldn't have
  // changed.
  EXPECT_EQ(data->last_loaded_time_for_testing(), last_loaded_time);

  // Make sure that all data passed to |OnURLsDeleted| get passed to the
  // database, even if they're not in the internal map used by the data store.
  const url::Origin kOriginNotInMap =
      url::Origin::Create(GURL("http://www.url-not-in-map.com"));
  history::URLRows urls_to_delete = {history::URLRow(kTestOrigin.GetURL()),
                                     history::URLRow(kOriginNotInMap.GetURL())};
  EXPECT_CALL(*mock_db,
              RemoveSiteCharacteristicsFromDB(::testing::ContainerEq(
                  std::vector<url::Origin>({kTestOrigin, kOriginNotInMap}))));
  data_store_->OnURLsDeleted(nullptr, history::DeletionInfo::ForUrls(
                                          urls_to_delete, std::set<GURL>()));
  ::testing::Mock::VerifyAndClear(mock_db);

  // The information for this site have been reset, so the last loaded time
  // should now be equal to the current time and the title update feature
  // observation should have been cleared.
  EXPECT_NE(data->last_loaded_time_for_testing(), last_loaded_time);
  EXPECT_EQ(data->last_loaded_time_for_testing(),
            test_clock_.NowTicks() - base::TimeTicks::UnixEpoch());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  // The second site shouldn't have been cleared.
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            reader2->UpdatesFaviconInBackground());

  test_clock_.Advance(kDelay);

  // Delete all the information stored in the data store.
  EXPECT_CALL(*mock_db, ClearDatabase());
  data_store_->OnURLsDeleted(nullptr, history::DeletionInfo::ForAllHistory());
  ::testing::Mock::VerifyAndClear(mock_db);

  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader2->UpdatesFaviconInBackground());
  EXPECT_EQ(data->last_loaded_time_for_testing(),
            test_clock_.NowTicks() - base::TimeTicks::UnixEpoch());

  internal::LocalSiteCharacteristicsDataImpl* data2 =
      data_store_->origin_data_map_for_testing().find(kTestOrigin2)->second;
  EXPECT_TRUE(data2);
  EXPECT_EQ(data2->last_loaded_time_for_testing(),
            test_clock_.NowTicks() - base::TimeTicks::UnixEpoch());

  writer->NotifySiteUnloaded();
  writer2->NotifySiteUnloaded();
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
  std::unique_ptr<SiteCharacteristicsProto> data;
  bool is_dirty = false;
  EXPECT_FALSE(inspector->GetDataForOrigin(kTestOrigin, &is_dirty, &data));
  EXPECT_FALSE(is_dirty);
  EXPECT_EQ(nullptr, data.get());

  {
    // Add an entry, see that it's reflected in the inspector interface.
    auto writer = data_store_->GetWriterForOrigin(kTestOrigin,
                                                  TabVisibility::kBackground);

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
