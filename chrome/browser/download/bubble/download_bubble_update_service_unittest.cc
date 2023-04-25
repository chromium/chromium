// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_update_service.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/download/bubble/download_bubble_update_service_factory.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/offline_item_state.h"
#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::download::DownloadDangerType;
using ::offline_items_collection::OfflineItem;
using ::offline_items_collection::OfflineItemState;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;
using ::testing::WithArg;
using AllDownloadUIModelsInfo =
    DownloadDisplayController::AllDownloadUIModelsInfo;
using DownloadState = download::DownloadItem::DownloadState;
using DownloadUIModelPtrVector =
    std::vector<DownloadUIModel::DownloadUIModelPtr>;
using NiceMockDownloadItem = NiceMock<download::MockDownloadItem>;

constexpr char kProfileName[] = "testing_profile";
constexpr char kProviderNamespace[] = "mock_namespace";

// Helper for MockDownloadManager to properly mimic GetAllDownloads(), such that
// it appends all items in |items| to the argument of the action, which should
// be a std::vector<DownloadItem*>.
ACTION_P(AddDownloadItems, items) {
  for (download::DownloadItem* item : items) {
    arg0->push_back(item);
  }
}

void RemoveDownloadItemsObserver(
    std::vector<std::unique_ptr<NiceMockDownloadItem>>& download_items,
    download::DownloadItem::Observer& observer) {
  for (auto& item : download_items) {
    item->RemoveObserver(&observer);
  }
}

class DownloadBubbleUpdateServiceTest : public testing::Test {
 public:
  DownloadBubbleUpdateServiceTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    feature_list_.InitWithFeatures(
        {safe_browsing::kDownloadBubble, safe_browsing::kDownloadBubbleV2}, {});
  }
  DownloadBubbleUpdateServiceTest(const DownloadBubbleUpdateServiceTest&) =
      delete;
  DownloadBubbleUpdateServiceTest& operator=(
      const DownloadBubbleUpdateServiceTest&) = delete;
  ~DownloadBubbleUpdateServiceTest() override = default;

  raw_ptr<NiceMock<content::MockDownloadManager>> SetUpDownloadManager(
      Profile* profile) {
    auto download_manager =
        std::make_unique<NiceMock<content::MockDownloadManager>>();
    raw_ptr<NiceMock<content::MockDownloadManager>> manager =
        download_manager.get();
    EXPECT_CALL(*download_manager, GetBrowserContext())
        .WillRepeatedly(Return(profile));
    EXPECT_CALL(*download_manager, RemoveObserver(_)).WillRepeatedly(Return());
    profile->SetDownloadManagerForTesting(std::move(download_manager));
    return manager;
  }

  // Pass a null |download_manager| to avoid registering the download manager.
  std::unique_ptr<KeyedService> InitUpdateService(
      NiceMock<content::MockDownloadManager>* download_manager,
      DownloadBubbleUpdateService* update_service,
      content::BrowserContext* context) {
    // Unregister the observer from the previous instance of the update service.
    // See note below in TearDown() for why this is necessary.
    if (update_service) {
      RemoveDownloadItemsObserver(
          download_items_,
          update_service->download_item_notifier_for_testing());
    }
    auto service = std::make_unique<DownloadBubbleUpdateService>(
        Profile::FromBrowserContext(context));
    if (download_manager) {
      service->Initialize(download_manager);
    }
    task_environment_.RunUntilIdle();
    return service;
  }

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile(kProfileName);
    download_manager_ = SetUpDownloadManager(profile_);

    offline_content_provider_ = std::make_unique<
        NiceMock<offline_items_collection::MockOfflineContentProvider>>();
    OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey())
        ->RegisterProvider(kProviderNamespace, offline_content_provider_.get());

    update_service_ = static_cast<DownloadBubbleUpdateService*>(
        DownloadBubbleUpdateServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_,
                base::BindRepeating(
                    &DownloadBubbleUpdateServiceTest::InitUpdateService,
                    base::Unretained(this), download_manager_.get(),
                    update_service_)));
  }

  void TearDown() override {
    // This would normally be done in the Notifier's destructor, but this
    // notifier doesn't know what items it's observing because we have manually
    // added it as an observer on each item, since the MockDownloadManager does
    // not actually call OnDownloadCreated on it.
    if (update_service_) {
      RemoveDownloadItemsObserver(
          download_items_,
          update_service_->download_item_notifier_for_testing());
    }
    offline_content_provider_.reset();
    testing_profile_manager_.DeleteTestingProfile(kProfileName);
  }

 protected:
  // TODO(chlily): Factor these out into a test utils file.
  download::MockDownloadItem& GetDownloadItem(size_t index) {
    return *download_items_[index];
  }

  // Forwards to the below version.
  void InitDownloadItem(DownloadState state,
                        const std::string& guid,
                        bool is_paused,
                        base::Time start_time = base::Time::Now(),
                        bool is_crx = false,
                        bool observe = true) {
    InitDownloadItem(*download_manager_, *update_service_, download_items_,
                     profile_, state, guid, is_paused, start_time, is_crx,
                     observe);
  }

  void InitDownloadItem(
      NiceMock<content::MockDownloadManager>& download_manager,
      DownloadBubbleUpdateService& update_service,
      std::vector<std::unique_ptr<NiceMockDownloadItem>>& download_items,
      Profile* profile,
      DownloadState state,
      const std::string& guid,
      bool is_paused,
      base::Time start_time = base::Time::Now(),
      bool is_crx = false,
      bool observe = true) {
    size_t index = download_items.size();
    download_items.push_back(std::make_unique<NiceMockDownloadItem>());
    auto& item = *download_items[index];
    EXPECT_CALL(item, GetId())
        .WillRepeatedly(
            Return(static_cast<uint32_t>(download_items.size() + 1)));
    EXPECT_CALL(item, GetGuid()).WillRepeatedly(ReturnRefOfCopy(guid));
    EXPECT_CALL(item, GetState()).WillRepeatedly(Return(state));
    EXPECT_CALL(item, GetStartTime()).WillRepeatedly(Return(start_time));
    EXPECT_CALL(item, GetEndTime()).WillRepeatedly(Return(start_time));
    EXPECT_CALL(item, GetTargetFilePath())
        .WillRepeatedly(
            ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
    int received_bytes =
        state == download::DownloadItem::IN_PROGRESS ? 50 : 100;
    EXPECT_CALL(item, GetReceivedBytes())
        .WillRepeatedly(Return(received_bytes));
    EXPECT_CALL(item, GetTotalBytes()).WillRepeatedly(Return(100));
    EXPECT_CALL(item, IsDone()).WillRepeatedly(Return(false));
    EXPECT_CALL(item, IsTransient()).WillRepeatedly(Return(false));
    EXPECT_CALL(item, IsPaused()).WillRepeatedly(Return(is_paused));
    EXPECT_CALL(item, GetTargetDisposition())
        .WillRepeatedly(
            Return(is_crx ? download::DownloadItem::TARGET_DISPOSITION_OVERWRITE
                          : download::DownloadItem::TARGET_DISPOSITION_PROMPT));
    EXPECT_CALL(item, GetMimeType())
        .WillRepeatedly(Return(is_crx ? "application/x-chrome-extension" : ""));
    std::vector<download::DownloadItem*> items;
    for (auto& download_item : download_items) {
      items.push_back(download_item.get());
    }
    EXPECT_CALL(download_manager, GetAllDownloads(_))
        .WillRepeatedly(WithArg<0>(AddDownloadItems(items)));
    EXPECT_CALL(download_manager, GetDownloadByGuid(guid))
        .WillRepeatedly(Return(&item));
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile, nullptr);
    if (observe) {
      item.AddObserver(&update_service.download_item_notifier_for_testing());
      item.NotifyObserversDownloadUpdated();
    }
  }

  void UpdateDownloadItem(
      int item_index,
      DownloadState state,
      bool is_paused = false,
      DownloadDangerType danger_type =
          DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    DCHECK_GT(download_items_.size(), static_cast<size_t>(item_index));
    auto& item = GetDownloadItem(item_index);
    EXPECT_CALL(item, GetState()).WillRepeatedly(Return(state));
    EXPECT_CALL(item, IsDone())
        .WillRepeatedly(Return(state == DownloadState::COMPLETE));
    EXPECT_CALL(item, IsDangerous())
        .WillRepeatedly(
            Return(danger_type !=
                   DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    EXPECT_CALL(item, GetDangerType()).WillRepeatedly(Return(danger_type));
    EXPECT_CALL(item, IsPaused()).WillRepeatedly(Return(is_paused));
    item.NotifyObserversDownloadUpdated();
  }

  void RemoveDownloadItem(size_t item_index) {
    download_items_[item_index]->NotifyObserversDownloadRemoved();
    download_items_.erase(download_items_.begin() + item_index);

    std::vector<download::DownloadItem*> items;
    for (size_t i = 0; i < download_items_.size(); ++i) {
      items.push_back(&GetDownloadItem(i));
    }
    EXPECT_CALL(*download_manager_, GetAllDownloads(_))
        .WillRepeatedly(WithArg<0>(AddDownloadItems(items)));
  }

  void InitOfflineItems(const std::vector<OfflineItemState>& states,
                        const std::vector<std::string>& ids,
                        const std::vector<base::Time>& start_times) {
    ASSERT_EQ(states.size(), ids.size());
    ASSERT_EQ(states.size(), start_times.size());
    std::vector<OfflineItem> new_items;
    for (size_t i = 0; i < states.size(); ++i) {
      OfflineItem item;
      item.state = states[i];
      item.id.id = ids[i];
      item.creation_time = start_times[i];
      new_items.push_back(item);
      offline_items_.push_back(std::move(item));
    }
    offline_content_provider_->NotifyOnItemsAdded(new_items);
  }

  void UpdateOfflineItem(int item_index, OfflineItemState state) {
    offline_items_[item_index].state = state;
    UpdateDelta delta;
    delta.state_changed = true;
    offline_content_provider_->NotifyOnItemUpdated(offline_items_[item_index],
                                                   delta);
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<NiceMock<content::MockDownloadManager>> download_manager_ = nullptr;
  std::vector<std::unique_ptr<NiceMockDownloadItem>> download_items_;
  std::vector<offline_items_collection::OfflineItem> offline_items_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<
      NiceMock<offline_items_collection::MockOfflineContentProvider>>
      offline_content_provider_;
  raw_ptr<DownloadBubbleUpdateService> update_service_ = nullptr;
};

TEST_F(DownloadBubbleUpdateServiceTest, PopulatesCaches) {
  base::Time now = base::Time::Now();
  base::Time older_time = now - base::Hours(1);
  InitDownloadItem(DownloadState::IN_PROGRESS, "in_progress_active_download",
                   /*is_paused=*/false, older_time);
  InitDownloadItem(DownloadState::IN_PROGRESS, "in_progress_paused_download",
                   /*is_paused=*/true, now);
  InitDownloadItem(DownloadState::COMPLETE, "completed_download",
                   /*is_paused=*/false, now);

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_download");
  EXPECT_EQ(models[1]->GetContentId().id, "in_progress_paused_download");
  EXPECT_EQ(models[2]->GetContentId().id, "completed_download");

  // Recreate the update service to check that it pulls in the existing
  // download items upon initialization.
  auto service =
      InitUpdateService(download_manager_, update_service_, profile_);
  update_service_ = static_cast<DownloadBubbleUpdateService*>(service.get());

  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_download");
  EXPECT_EQ(models[1]->GetContentId().id, "in_progress_paused_download");
  EXPECT_EQ(models[2]->GetContentId().id, "completed_download");

  // Add some more (offline) items.
  InitOfflineItems(
      {OfflineItemState::IN_PROGRESS, OfflineItemState::PAUSED,
       OfflineItemState::COMPLETE},
      {"in_progress_active_offline_item", "in_progress_paused_offline_item",
       "completed_offline_item"},
      {now, older_time, older_time});

  // All items are returned in sorted order.
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 6u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_offline_item");
  EXPECT_EQ(models[1]->GetContentId().id, "in_progress_active_download");
  EXPECT_EQ(models[2]->GetContentId().id, "in_progress_paused_download");
  EXPECT_EQ(models[3]->GetContentId().id, "in_progress_paused_offline_item");
  EXPECT_EQ(models[4]->GetContentId().id, "completed_download");
  EXPECT_EQ(models[5]->GetContentId().id, "completed_offline_item");

  // Manually clean up the second service instance to avoid UAF.
  RemoveDownloadItemsObserver(
      download_items_, update_service_->download_item_notifier_for_testing());
  update_service_ = nullptr;
}

TEST_F(DownloadBubbleUpdateServiceTest, AddsNonCrxDownloadItems) {
  InitDownloadItem(DownloadState::IN_PROGRESS, "new_download",
                   /*is_paused=*/false, base::Time::Now(), /*is_crx=*/false,
                   /*observe=*/false);
  // Manually notify the service of the new download rather than going through
  // the observer update notification in InitDownloadItem().
  update_service_->OnDownloadCreated(download_manager_, &GetDownloadItem(0));
  DownloadUIModelPtrVector models;
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "new_download");
}

TEST_F(DownloadBubbleUpdateServiceTest, DelaysCrx) {
  InitDownloadItem(DownloadState::IN_PROGRESS, "in_progress_crx",
                   /*is_paused=*/false, base::Time::Now(), /*is_crx=*/true,
                   /*observe=*/false);
  // Manually notify the service of the new download rather than going through
  // the observer update notification in InitDownloadItem().
  update_service_->OnDownloadCreated(download_manager_, &GetDownloadItem(0));

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  // The crx download does not show up immediately.
  EXPECT_EQ(models.size(), 0u);

  // Updates are also withheld.
  UpdateDownloadItem(0, DownloadState::IN_PROGRESS, /*is_paused=*/true);
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  EXPECT_EQ(models.size(), 0u);

  task_environment_.FastForwardBy(base::Seconds(2));

  // After the delay, the crx is added.
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_crx");
}

TEST_F(DownloadBubbleUpdateServiceTest, EvictsExcessItemsAndBackfills) {
  update_service_->set_max_num_items_to_show_for_testing(3);
  update_service_->set_extra_items_to_cache_for_testing(0);
  base::Time now = base::Time::Now();
  base::Time older_time = now - base::Hours(1);
  InitDownloadItem(DownloadState::IN_PROGRESS, "in_progress_active_download",
                   /*is_paused=*/false, older_time);
  InitDownloadItem(DownloadState::IN_PROGRESS, "in_progress_paused_download",
                   /*is_paused=*/true, now);
  InitDownloadItem(DownloadState::COMPLETE, "completed_download_older",
                   /*is_paused=*/false, older_time);

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_download");
  EXPECT_EQ(models[1]->GetContentId().id, "in_progress_paused_download");
  EXPECT_EQ(models[2]->GetContentId().id, "completed_download_older");

  // Add another item. This sorts before the last item and should cause it to be
  // evicted.
  InitDownloadItem(DownloadState::COMPLETE, "completed_download_newer",
                   /*is_paused=*/false, now);

  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_download");
  EXPECT_EQ(models[1]->GetContentId().id, "in_progress_paused_download");
  EXPECT_EQ(models[2]->GetContentId().id, "completed_download_newer");

  // Remove an item. The previously evicted item should come back via backfill.
  RemoveDownloadItem(1);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_download");
  EXPECT_EQ(models[1]->GetContentId().id, "completed_download_newer");
  EXPECT_EQ(models[2]->GetContentId().id, "completed_download_older");
}

TEST_F(DownloadBubbleUpdateServiceTest, BackfillsOnUpdate) {
  update_service_->set_max_num_items_to_show_for_testing(3);
  update_service_->set_extra_items_to_cache_for_testing(0);
  base::Time now = base::Time::Now();
  base::Time older_time = now - base::Hours(1);
  base::Time even_older_time = older_time - base::Hours(1);
  base::Time oldest_time = even_older_time - base::Hours(1);
  InitDownloadItem(DownloadState::IN_PROGRESS, "now",
                   /*is_paused=*/false, now);
  InitDownloadItem(DownloadState::IN_PROGRESS, "older",
                   /*is_paused=*/false, older_time);
  InitDownloadItem(DownloadState::IN_PROGRESS, "even_older",
                   /*is_paused=*/false, even_older_time);
  InitDownloadItem(DownloadState::IN_PROGRESS, "oldest",
                   /*is_paused=*/false, oldest_time);

  // Only the 3 newest downloads are shown at first.
  DownloadUIModelPtrVector models;
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "older");
  EXPECT_EQ(models[2]->GetContentId().id, "even_older");

  // Update the newest item to be paused, so it should sort behind all the other
  // items.
  UpdateDownloadItem(0, DownloadState::IN_PROGRESS, /*is_paused=*/true);
  task_environment_.RunUntilIdle();

  // The oldest download, previously too low in sort order to display, is
  // retrieved after backfilling.
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "older");
  EXPECT_EQ(models[1]->GetContentId().id, "even_older");
  EXPECT_EQ(models[2]->GetContentId().id, "oldest");
}

TEST_F(DownloadBubbleUpdateServiceTest, UpdatesOfflineItems) {
  base::Time now = base::Time::Now();
  InitOfflineItems({OfflineItemState::IN_PROGRESS},
                   {"in_progress_active_offline_item"}, {now});

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_offline_item");
  EXPECT_EQ(models[0]->GetState(), DownloadState::IN_PROGRESS);

  UpdateOfflineItem(0, OfflineItemState::COMPLETE);
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_offline_item");
  EXPECT_EQ(models[0]->GetState(), DownloadState::COMPLETE);
}

TEST_F(DownloadBubbleUpdateServiceTest, RemovesOfflineItems) {
  base::Time now = base::Time::Now();
  InitOfflineItems(
      {OfflineItemState::IN_PROGRESS, OfflineItemState::PAUSED,
       OfflineItemState::COMPLETE},
      {"in_progress_active_offline_item", "in_progress_paused_offline_item",
       "completed_offline_item"},
      {now, now, now});

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_offline_item");
  EXPECT_EQ(models[1]->GetContentId().id, "in_progress_paused_offline_item");
  EXPECT_EQ(models[2]->GetContentId().id, "completed_offline_item");

  offline_content_provider_->NotifyOnItemRemoved(models[0]->GetContentId());
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 2u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_paused_offline_item");
  EXPECT_EQ(models[1]->GetContentId().id, "completed_offline_item");
}

TEST_F(DownloadBubbleUpdateServiceTest, DoesNotAddExpiredItems) {
  base::Time too_old_time = base::Time::Now() - base::Hours(25);
  InitDownloadItem(DownloadState::IN_PROGRESS, "old",
                   /*is_paused=*/false, too_old_time);
  DownloadUIModelPtrVector models;
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  EXPECT_EQ(models.size(), 0u);
}

TEST_F(DownloadBubbleUpdateServiceTest, PrunesExpiredItems) {
  base::Time now = base::Time::Now();
  base::Time two_hours_ago = now - base::Hours(2);
  // Add some items that are currently recent enough to add.
  InitDownloadItem(DownloadState::IN_PROGRESS, "now_download",
                   /*is_paused=*/false, now);
  InitDownloadItem(DownloadState::IN_PROGRESS, "two_hours_ago_download",
                   /*is_paused=*/false, two_hours_ago);
  InitOfflineItems({OfflineItemState::PAUSED, OfflineItemState::PAUSED},
                   {"now_offline_item", "two_hours_ago_offline_item"},
                   {now, two_hours_ago});

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 4u);
  EXPECT_EQ(models[0]->GetContentId().id, "now_download");
  EXPECT_EQ(models[1]->GetContentId().id, "two_hours_ago_download");
  EXPECT_EQ(models[2]->GetContentId().id, "now_offline_item");
  EXPECT_EQ(models[3]->GetContentId().id, "two_hours_ago_offline_item");

  // Fast forward so that the older items become too old.
  task_environment_.FastForwardBy(base::Hours(23));

  // Only the newer items should remain.
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 2u);
  EXPECT_EQ(models[0]->GetContentId().id, "now_download");
  EXPECT_EQ(models[1]->GetContentId().id, "now_offline_item");
}

TEST_F(DownloadBubbleUpdateServiceTest, DoesNotBackfillIfNotForced) {
  update_service_->set_max_num_items_to_show_for_testing(3);
  update_service_->set_extra_items_to_cache_for_testing(0);
  base::Time now = base::Time::Now();
  base::Time recent = now - base::Minutes(1);
  base::Time two_hours_ago = now - base::Hours(2);

  InitDownloadItem(DownloadState::IN_PROGRESS, "now",
                   /*is_paused=*/false, now);
  InitDownloadItem(DownloadState::IN_PROGRESS, "recent",
                   /*is_paused=*/false, recent);
  InitDownloadItem(DownloadState::IN_PROGRESS, "two_hours_ago",
                   /*is_paused=*/false, two_hours_ago);
  InitDownloadItem(DownloadState::IN_PROGRESS, "now_paused",
                   /*is_paused=*/true, now);

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "recent");
  EXPECT_EQ(models[2]->GetContentId().id, "two_hours_ago");

  task_environment_.FastForwardBy(base::Hours(23));

  // Since items are pruned, return the unpruned ones immediately and indicate
  // that results are not complete.
  EXPECT_FALSE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 2u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "recent");

  // Sometime later, once the backfilling is complete, we will start to return
  // all the non-expired items.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "recent");
  EXPECT_EQ(models[2]->GetContentId().id, "now_paused");
}

TEST_F(DownloadBubbleUpdateServiceTest, BackfillsSynchronouslyIfForced) {
  update_service_->set_max_num_items_to_show_for_testing(3);
  update_service_->set_extra_items_to_cache_for_testing(0);
  base::Time now = base::Time::Now();
  base::Time recent = now - base::Minutes(1);
  base::Time two_hours_ago = now - base::Hours(2);

  InitDownloadItem(DownloadState::IN_PROGRESS, "now",
                   /*is_paused=*/false, now);
  InitDownloadItem(DownloadState::IN_PROGRESS, "recent",
                   /*is_paused=*/false, recent);
  InitDownloadItem(DownloadState::IN_PROGRESS, "two_hours_ago",
                   /*is_paused=*/false, two_hours_ago);
  InitDownloadItem(DownloadState::IN_PROGRESS, "now_paused",
                   /*is_paused=*/true, now);

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "recent");
  EXPECT_EQ(models[2]->GetContentId().id, "two_hours_ago");

  task_environment_.FastForwardBy(base::Hours(23));

  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(
      models, /*force_backfill_download_items=*/true));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "recent");
  EXPECT_EQ(models[2]->GetContentId().id, "now_paused");
}

TEST_F(DownloadBubbleUpdateServiceTest, CachesExtraItems) {
  update_service_->set_max_num_items_to_show_for_testing(3);
  // Cache an extra item.
  update_service_->set_extra_items_to_cache_for_testing(1);
  base::Time now = base::Time::Now();
  base::Time recent = now - base::Minutes(2);
  base::Time two_hours_ago = now - base::Hours(2);

  InitDownloadItem(DownloadState::IN_PROGRESS, "now",
                   /*is_paused=*/false, now);
  InitDownloadItem(DownloadState::IN_PROGRESS, "recent",
                   /*is_paused=*/false, recent);
  InitDownloadItem(DownloadState::IN_PROGRESS, "two_hours_ago",
                   /*is_paused=*/false, two_hours_ago);
  InitDownloadItem(DownloadState::IN_PROGRESS, "now_paused",
                   /*is_paused=*/true, now);

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "recent");
  EXPECT_EQ(models[2]->GetContentId().id, "two_hours_ago");

  task_environment_.FastForwardBy(base::Hours(23));

  // This returns true despite not forcing a backfill, because the extra item
  // in the cache is available to be returned.
  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "recent");
  EXPECT_EQ(models[2]->GetContentId().id, "now_paused");
}

TEST_F(DownloadBubbleUpdateServiceTest, GetProgressInfo) {
  InitDownloadItem(DownloadState::IN_PROGRESS, "in_progress_active",
                   /*is_paused=*/false);
  InitDownloadItem(DownloadState::IN_PROGRESS, "in_progress_paused",
                   /*is_paused=*/true);
  InitDownloadItem(DownloadState::COMPLETE, "completed",
                   /*is_paused=*/false);

  DownloadDisplayController::ProgressInfo progress_info =
      update_service_->GetProgressInfo();
  EXPECT_EQ(progress_info.download_count, 2);
  EXPECT_TRUE(progress_info.progress_certain);
  EXPECT_EQ(progress_info.progress_percentage, 50);

  InitOfflineItems({OfflineItemState::IN_PROGRESS}, {"offline_item"},
                   {base::Time::Now()});

  progress_info = update_service_->GetProgressInfo();
  EXPECT_EQ(progress_info.download_count, 3);
  EXPECT_FALSE(progress_info.progress_certain);
  EXPECT_EQ(progress_info.progress_percentage, 50);
}

TEST_F(DownloadBubbleUpdateServiceTest, GetAllUIModelsInfo) {
  base::Time now = base::Time::Now();
  base::Time two_hours_ago = now - base::Hours(2);
  InitDownloadItem(DownloadState::IN_PROGRESS, "now_download",
                   /*is_paused=*/false, now);
  InitDownloadItem(DownloadState::IN_PROGRESS, "two_hours_ago_download",
                   /*is_paused=*/false, two_hours_ago);
  InitDownloadItem(DownloadState::COMPLETE, "completed_download",
                   /*is_paused=*/false, two_hours_ago);
  InitOfflineItems({OfflineItemState::PAUSED, OfflineItemState::PAUSED},
                   {"now_offline_item", "two_hours_ago_offline_item"},
                   {now, two_hours_ago});

  AllDownloadUIModelsInfo info = update_service_->GetAllModelsInfo();
  EXPECT_EQ(info.all_models_size, 5u);
  EXPECT_EQ(info.last_completed_time, now);
  EXPECT_EQ(info.in_progress_count, 4);
  EXPECT_EQ(info.paused_count, 2);
  EXPECT_TRUE(info.has_unactioned);
  EXPECT_FALSE(info.has_deep_scanning);
}

class DownloadBubbleUpdateServiceIncognitoTest
    : public DownloadBubbleUpdateServiceTest {
 public:
  DownloadBubbleUpdateServiceIncognitoTest() = default;
  DownloadBubbleUpdateServiceIncognitoTest(
      const DownloadBubbleUpdateServiceIncognitoTest&) = delete;
  DownloadBubbleUpdateServiceIncognitoTest& operator=(
      const DownloadBubbleUpdateServiceIncognitoTest&) = delete;
  ~DownloadBubbleUpdateServiceIncognitoTest() override = default;

  void SetUp() override {
    DownloadBubbleUpdateServiceTest::SetUp();

    incognito_profile_ = profile_->GetOffTheRecordProfile(
        Profile::OTRProfileID::CreateUniqueForTesting(),
        /*create_if_needed=*/true);
    incognito_download_manager_ = SetUpDownloadManager(incognito_profile_);
    // Pass nullptr for the download_manager to delay RegisterDownloadManager()
    // call for the test.
    incognito_update_service_ = static_cast<DownloadBubbleUpdateService*>(
        DownloadBubbleUpdateServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                incognito_profile_,
                base::BindRepeating(
                    &DownloadBubbleUpdateServiceTest::InitUpdateService,
                    base::Unretained(this), /*download_manager=*/nullptr,
                    incognito_update_service_)));
  }

  void TearDown() override {
    RemoveDownloadItemsObserver(
        incognito_download_items_,
        incognito_update_service_->download_item_notifier_for_testing());
    RemoveDownloadItemsObserver(
        download_items_, incognito_update_service_
                             ->original_download_item_notifier_for_testing());
    DownloadBubbleUpdateServiceTest::TearDown();
  }

 protected:
  raw_ptr<Profile> incognito_profile_ = nullptr;
  raw_ptr<NiceMock<content::MockDownloadManager>> incognito_download_manager_ =
      nullptr;
  std::vector<std::unique_ptr<NiceMockDownloadItem>> incognito_download_items_;
  raw_ptr<DownloadBubbleUpdateService> incognito_update_service_ = nullptr;
};

// Tests that initializing an update service for an incognito profile sets both
// the download manager and the original download manager.
TEST_F(DownloadBubbleUpdateServiceIncognitoTest, InitIncognito) {
  base::Time now = base::Time::Now();
  // |observe| is false because this only tests initialization.
  InitDownloadItem(DownloadState::COMPLETE, "regular_profile_download",
                   /*is_paused=*/false, now - base::Hours(1), /*is_crx=*/false,
                   /*observe=*/false);
  InitDownloadItem(*incognito_download_manager_, *incognito_update_service_,
                   incognito_download_items_, incognito_profile_,
                   DownloadState::COMPLETE, "incognito_profile_download",
                   /*is_paused=*/false, now, /*is_crx=*/false,
                   /*observe=*/false);

  // Initial state: Regular profile's update service has a manager, incognito's
  // does not.
  EXPECT_NE(update_service_->GetDownloadManager(), nullptr);
  EXPECT_EQ(update_service_->GetDownloadManager(), download_manager_);
  EXPECT_EQ(incognito_update_service_->GetDownloadManager(), nullptr);

  incognito_update_service_->Initialize(incognito_download_manager_);
  // Regular profile's update service's manager hasn't changed.
  EXPECT_EQ(update_service_->GetDownloadManager(), download_manager_);
  // Incognito profile's update service's manager now set correctly.
  EXPECT_EQ(incognito_update_service_->GetDownloadManager(),
            incognito_download_manager_);

  // Both download items should be present after initialization.
  DownloadUIModelPtrVector models;
  EXPECT_TRUE(incognito_update_service_->GetAllModelsToDisplay(models));
  ASSERT_EQ(models.size(), 2u);
  EXPECT_EQ(models[0]->GetContentId().id, "incognito_profile_download");
  EXPECT_EQ(models[1]->GetContentId().id, "regular_profile_download");
}

}  // namespace
