// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade_factory.h"
#include "chrome/browser/performance_manager/persistence/site_data/unittest_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/persistence/site_data/leveldb_site_data_store.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_impl.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

// Mock version of a SiteDataCacheImpl. In practice instances of this object
// live on the Performance Manager sequence and all the mocked methods will be
// called from there.
class LenientMockSiteDataCacheImpl : public SiteDataCacheImpl {
 public:
  explicit LenientMockSiteDataCacheImpl(const std::string& browser_context_id)
      : SiteDataCacheImpl(browser_context_id) {}

  LenientMockSiteDataCacheImpl(const LenientMockSiteDataCacheImpl&) = delete;
  LenientMockSiteDataCacheImpl& operator=(const LenientMockSiteDataCacheImpl&) =
      delete;

  ~LenientMockSiteDataCacheImpl() override = default;

  // The 2 following functions allow setting the expectations for the mocked
  // functions. Any call to one of these functions should be followed by the
  // call that will caused the mocked the function to be called and then by a
  // call to |WaitForExpectations|. Only one expectation can be set at a time.

  void SetClearSiteDataForOriginsExpectations(
      const std::vector<url::Origin>& expected_origins) {
    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    auto quit_closure = run_loop_->QuitClosure();
    EXPECT_CALL(*this, ClearSiteDataForOrigins(::testing::Eq(expected_origins)))
        .WillOnce(
            ::testing::InvokeWithoutArgs([closure = std::move(quit_closure)]() {
              std::move(closure).Run();
            }));
  }

  void SetClearAllSiteDataExpectations() {
    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    auto quit_closure = run_loop_->QuitClosure();
    EXPECT_CALL(*this, ClearAllSiteData())
        .WillOnce(
            ::testing::InvokeWithoutArgs([closure = std::move(quit_closure)]() {
              std::move(closure).Run();
            }));
  }

  void WaitForExpectations() {
    ASSERT_TRUE(run_loop_);
    run_loop_->Run();
    run_loop_.reset();
    ::testing::Mock::VerifyAndClear(this);
  }

 private:
  MOCK_METHOD1(ClearSiteDataForOrigins, void(const std::vector<url::Origin>&));
  MOCK_METHOD0(ClearAllSiteData, void());

  std::unique_ptr<base::RunLoop> run_loop_;
};
using MockSiteDataCache = ::testing::StrictMock<LenientMockSiteDataCacheImpl>;

}  // namespace

class SiteDataCacheFacadeTest : public testing::TestWithPerformanceManager {
 public:
  SiteDataCacheFacadeTest() = default;
  ~SiteDataCacheFacadeTest() override = default;

  void SetUp() override {
    testing::TestWithPerformanceManager::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    use_in_memory_db_for_testing_ =
        LevelDBSiteDataStore::UseInMemoryDBForTesting();
  }

  void TearDown() override {
    use_in_memory_db_for_testing_.RunAndReset();
    profile_.reset();
    testing::TestWithPerformanceManager::TearDown();
  }

  TestingProfile* profile() { return profile_.get(); }

  // Replace the SiteDataCache associated with |profile_| with a mock one.
  MockSiteDataCache* SetUpMockCache() {
    MockSiteDataCache* mock_cache_raw = nullptr;
    auto browser_context_id = profile()->UniqueId();
    RunInGraph([&] {
      auto mock_cache = std::make_unique<MockSiteDataCache>(browser_context_id);
      mock_cache_raw = mock_cache.get();

      auto* factory = SiteDataCacheFactory::GetInstance();
      ASSERT_TRUE(factory);
      factory->SetCacheForTesting(browser_context_id, std::move(mock_cache));
      factory->SetCacheInspectorForTesting(browser_context_id, mock_cache_raw);
    });
    return mock_cache_raw;
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  base::ScopedClosureRunner use_in_memory_db_for_testing_;
};

TEST_F(SiteDataCacheFacadeTest, IsDataCacheRecordingForTesting) {
  bool cache_is_recording = false;

  SiteDataCacheFacade data_cache_facade(profile());
  data_cache_facade.WaitUntilCacheInitializedForTesting();
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    data_cache_facade.IsDataCacheRecordingForTesting(
        base::BindLambdaForTesting([&](bool is_recording) {
          cache_is_recording = is_recording;
          std::move(quit_closure).Run();
        }));
    run_loop.Run();
  }
  EXPECT_TRUE(cache_is_recording);

  SiteDataCacheFacade off_record_data_cache_facade(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    off_record_data_cache_facade.IsDataCacheRecordingForTesting(
        base::BindLambdaForTesting([&](bool is_recording) {
          cache_is_recording = is_recording;
          quit_closure.Run();
        }));
    run_loop.Run();
  }

  EXPECT_FALSE(cache_is_recording);
}

// Verify that an origin is removed from the data cache (in memory and on disk)
// when there are no more references to it in the history, after the history is
// partially cleared.
TEST_F(SiteDataCacheFacadeTest, OnURLsDeleted_Partial_OriginNotReferenced) {
  const auto kOrigin1 = url::Origin::Create(GURL("http://www.a.com"));
  const auto kOrigin2 = url::Origin::Create(GURL("http://www.b.com"));
  history::URLRows urls_to_delete = {history::URLRow(kOrigin1.GetURL()),
                                     history::URLRow(kOrigin2.GetURL())};
  history::DeletionInfo deletion_info =
      history::DeletionInfo::ForUrls(urls_to_delete, std::set<GURL>());
  deletion_info.set_deleted_urls_origin_map({
      {kOrigin1.GetURL(), {0, base::Time::Now()}},
      {kOrigin2.GetURL(), {0, base::Time::Now()}},
  });

  SiteDataCacheFacade data_cache_facade(profile());
  data_cache_facade.WaitUntilCacheInitializedForTesting();

  auto* mock_cache_raw = SetUpMockCache();
  mock_cache_raw->SetClearSiteDataForOriginsExpectations({kOrigin1, kOrigin2});
  data_cache_facade.OnHistoryDeletions(nullptr, deletion_info);
  mock_cache_raw->WaitForExpectations();
}

// Verify that an origin is *not* removed from the data cache (in memory and on
// disk) when there remain references to it in the history, after the history is
// partially cleared.
TEST_F(SiteDataCacheFacadeTest, OnURLsDeleted_Partial_OriginStillReferenced) {
  const auto kOrigin1 = url::Origin::Create(GURL("http://www.a.com"));
  const auto kOrigin2 = url::Origin::Create(GURL("http://www.b.com"));
  history::URLRows urls_to_delete = {history::URLRow(kOrigin1.GetURL()),
                                     history::URLRow(kOrigin2.GetURL())};
  history::DeletionInfo deletion_info =
      history::DeletionInfo::ForUrls(urls_to_delete, std::set<GURL>());
  deletion_info.set_deleted_urls_origin_map({
      {kOrigin1.GetURL(), {0, base::Time::Now()}},
      {kOrigin2.GetURL(), {3, base::Time::Now()}},
  });

  SiteDataCacheFacade data_cache_facade(profile());
  data_cache_facade.WaitUntilCacheInitializedForTesting();

  auto* mock_cache_raw = SetUpMockCache();
  // |kOrigin2| shouldn't be removed as there's still some references to it
  // in the history.
  mock_cache_raw->SetClearSiteDataForOriginsExpectations({kOrigin1});
  data_cache_facade.OnHistoryDeletions(nullptr, deletion_info);
  mock_cache_raw->WaitForExpectations();
}

// Verify that origins are removed from the data cache (in memory and on disk)
// when the history is completely cleared.
TEST_F(SiteDataCacheFacadeTest, OnURLsDeleted_Full) {
  SiteDataCacheFacade data_cache_facade(profile());
  data_cache_facade.WaitUntilCacheInitializedForTesting();

  auto* mock_cache_raw = SetUpMockCache();
  mock_cache_raw->SetClearAllSiteDataExpectations();
  data_cache_facade.OnHistoryDeletions(nullptr,
                                       history::DeletionInfo::ForAllHistory());
  mock_cache_raw->WaitForExpectations();
}

}  // namespace performance_manager
