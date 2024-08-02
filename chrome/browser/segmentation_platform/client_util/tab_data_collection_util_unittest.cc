// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/client_util/tab_data_collection_util.h"

#include <memory>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_rank_dispatcher.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Return;

class MockRankDispatcher : public TabRankDispatcher {
 public:
  MockRankDispatcher(SegmentationPlatformService* service,
                     sync_sessions::SessionSyncService* session_sync_service,
                     std::unique_ptr<TabFetcher> fetcher)
      : TabRankDispatcher(service, session_sync_service, std::move(fetcher)) {}

  MOCK_METHOD(void,
              GetTopRankedTabs,
              (const std::string& segmentation_key,
               const TabFilter& tab_filter,
               RankedTabsCallback callback),
              (override));
};

class MockSessionSyncService : public sync_sessions::SessionSyncService {
 public:
  MockSessionSyncService() = default;
  ~MockSessionSyncService() override = default;

  MOCK_METHOD(syncer::GlobalIdMapper*,
              GetGlobalIdMapper,
              (),
              (const, override));
  MOCK_METHOD(sync_sessions::OpenTabsUIDelegate*,
              GetOpenTabsUIDelegate,
              (),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              SubscribeToForeignSessionsChanged,
              (const base::RepeatingClosure& cb),
              (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              ());
};

class MockTabFecther : public TabFetcher {
 public:
  explicit MockTabFecther(MockSessionSyncService* session_sync_service)
      : TabFetcher(session_sync_service) {}

  MOCK_METHOD(bool,
              FillAllLocalTabsFromTabModel,
              (std::vector<TabEntry> & tabs),
              (override));
  MOCK_METHOD(Tab, FindLocalTab, (const TabEntry& entry), (override));
};

}  // namespace

class TabDataCollectionUtilTest : public testing::Test {
 public:
  TabDataCollectionUtilTest() = default;
  ~TabDataCollectionUtilTest() override = default;

  void SetUp() override {
    Test::SetUp();
    base::SetRecordActionTaskRunner(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    tab_model_ = std::make_unique<TestTabModel>(&profile_);
    segmentation_service_ = std::make_unique<MockSegmentationPlatformService>();
    auto tab_fetcher = std::make_unique<MockTabFecther>(&session_sync_service_);
    tab_fetcher_ = tab_fetcher.get();
    rank_dispatcher_ = std::make_unique<MockRankDispatcher>(
        segmentation_service_.get(), &session_sync_service_,
        std::move(tab_fetcher));
    collection_util_ = std::make_unique<TabDataCollectionUtil>(
        segmentation_service_.get(), rank_dispatcher_.get());
  }

  void TearDown() override { Test::TearDown(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<TestTabModel> tab_model_;
  MockSessionSyncService session_sync_service_;
  raw_ptr<MockTabFecther> tab_fetcher_;
  std::unique_ptr<MockSegmentationPlatformService> segmentation_service_;
  std::unique_ptr<MockRankDispatcher> rank_dispatcher_;
  std::unique_ptr<TabDataCollectionUtil> collection_util_;
};

TEST_F(TabDataCollectionUtilTest, AddRemoveTabModel) {
  ASSERT_FALSE(tab_model_->GetObserver());
  TabModelList::AddTabModel(tab_model_.get());
  ASSERT_TRUE(tab_model_->GetObserver());
  TabModelList::RemoveTabModel(tab_model_.get());
  ASSERT_FALSE(tab_model_->GetObserver());

  auto other_tab_model = std::make_unique<TestTabModel>(&profile_);
  TabModelList::AddTabModel(other_tab_model.get());
  ASSERT_FALSE(tab_model_->GetObserver());
  ASSERT_TRUE(other_tab_model->GetObserver());

  TabModelList::AddTabModel(tab_model_.get());
  ASSERT_TRUE(tab_model_->GetObserver());
  ASSERT_TRUE(other_tab_model->GetObserver());

  TabModelList::RemoveTabModel(tab_model_.get());
  TabModelList::RemoveTabModel(other_tab_model.get());
  ASSERT_FALSE(tab_model_->GetObserver());
  ASSERT_FALSE(other_tab_model->GetObserver());
}

TEST_F(TabDataCollectionUtilTest, RecordTrainingData) {
  TabModelList::AddTabModel(tab_model_.get());
  ASSERT_TRUE(tab_model_->GetObserver());

  TabAndroid* fake_tab_ptr = reinterpret_cast<TabAndroid*>(1);
  EXPECT_CALL(*tab_fetcher_, FindLocalTab(_))
      .WillOnce(Return(TabFetcher::Tab{.tab_android = fake_tab_ptr}));
  TrainingRequestId id1 = TrainingRequestId::FromUnsafeValue(5);
  TabFetcher::TabEntry entry(SessionID::NewUnique(), nullptr, fake_tab_ptr);
  TabRankDispatcher::RankedTab tab1{
      .tab = entry, .model_score = 0.5, .request_id = id1};
  std::multiset<TabRankDispatcher::RankedTab> tabs{tab1};
  EXPECT_CALL(*rank_dispatcher_,
              GetTopRankedTabs(kTabResumptionClassifierKey, _, _))
      .WillOnce(RunOnceCallback<2>(true, tabs));

  base::RunLoop wait_for_collection;
  EXPECT_CALL(*segmentation_service_,
              CollectTrainingData(proto::SegmentId::TAB_RESUMPTION_CLASSIFIER,
                                  id1, _, _))
      .WillOnce([&wait_for_collection](
                    proto::SegmentId segment_id, TrainingRequestId request_id,
                    const TrainingLabels& param,
                    SegmentationPlatformService::SuccessCallback callback) {
        wait_for_collection.QuitClosure().Run();
      });
  base::RecordAction(base::UserMetricsAction("MobileToolbarShowStackView"));
  tab_model_->GetObserver()->TabPendingClosure(fake_tab_ptr);
  wait_for_collection.Run();

  TabModelList::RemoveTabModel(tab_model_.get());
}

}  // namespace segmentation_platform
