// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_update_service.h"

#include <memory>

#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/bubble/download_bubble_display_info.h"
#include "chrome/browser/download/bubble/download_bubble_update_service_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_web_app_data.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/offline_item_state.h"
#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"
#include "components/webapps/common/web_app_id.h"
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
using ::testing::AllOf;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;
using Alert = DownloadBubbleAccessibleAlertsMap::Alert;
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

// Matcher that works with substrings of u16string.
MATCHER_P(HasSubstr16, substring, "") {
  return arg.find(substring) != std::u16string::npos;
}

void RemoveDownloadItemsObserver(
    std::vector<std::unique_ptr<NiceMockDownloadItem>>& download_items,
    download::DownloadItem::Observer& observer) {
  for (auto& item : download_items) {
    item->RemoveObserver(&observer);
  }
}

std::unique_ptr<KeyedService> SetUpDownloadCoreService(
    content::DownloadManager* download_manager,
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  auto dcs = std::make_unique<DownloadCoreServiceImpl>(profile);
  auto cdmd = std::make_unique<ChromeDownloadManagerDelegate>(profile);
  cdmd->SetDownloadManager(download_manager);
  dcs->SetDownloadManagerDelegateForTesting(std::move(cdmd));
  auto download_history = std::make_unique<DownloadHistory>(
      download_manager, std::make_unique<DownloadHistory::HistoryAdapter>(
                            HistoryServiceFactory::GetInstance()->GetForProfile(
                                profile, ServiceAccessType::EXPLICIT_ACCESS)));
  dcs->SetDownloadHistoryForTesting(std::move(download_history));
  return dcs;
}

class DownloadBubbleUpdateServiceTest : public testing::Test {
 public:
  DownloadBubbleUpdateServiceTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  DownloadBubbleUpdateServiceTest(const DownloadBubbleUpdateServiceTest&) =
      delete;
  DownloadBubbleUpdateServiceTest& operator=(
      const DownloadBubbleUpdateServiceTest&) = delete;
  ~DownloadBubbleUpdateServiceTest() override = default;

  std::unique_ptr<NiceMock<content::MockDownloadManager>>
  SetUpDownloadManager() {
    auto download_manager =
        std::make_unique<NiceMock<content::MockDownloadManager>>();
    EXPECT_CALL(*download_manager, RemoveObserver(_)).WillRepeatedly(Return());
    // Default case for when no download exists with the requested guid.
    EXPECT_CALL(*download_manager, GetDownloadByGuid(_))
        .WillRepeatedly(Return(nullptr));
    return download_manager;
  }

  std::unique_ptr<KeyedService> CreateUpdateService(
      DownloadBubbleUpdateService* preexisting_update_service,
      content::BrowserContext* context) {
    // Unregister the observer from the previous instance of the update service.
    // See note below in TearDown() for why this is necessary.
    if (preexisting_update_service) {
      RemoveDownloadItemsObserver(
          download_items_,
          preexisting_update_service->download_item_notifier_for_testing());
    }
    auto service = std::make_unique<DownloadBubbleUpdateService>(
        Profile::FromBrowserContext(context));
    return service;
  }

  void InitializeUpdateService() {
    update_service_->Initialize(download_manager_);
    // Run the offline content provider initialization.
    task_environment_.RunUntilIdle();
  }

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    // The order of these initialization steps is important. The DownloadManager
    // must be created and set on the DownloadCoreService before setting up the
    // history service and calling DownloadBubbleUpdateService::Initialize.
    // Otherwise GetDownloadHistory will cause Initialize() to be called a
    // second time, violating invariants assumed by the Update Service.
    std::unique_ptr<NiceMock<content::MockDownloadManager>> download_manager =
        SetUpDownloadManager();
    download_manager_ = download_manager.get();
    TestingProfile::TestingFactories testing_factories;
    testing_factories.emplace_back(HistoryServiceFactory::GetInstance(),
                                   HistoryServiceFactory::GetDefaultFactory());
    testing_factories.emplace_back(
        DownloadCoreServiceFactory::GetInstance(),
        base::BindRepeating(&SetUpDownloadCoreService, download_manager_));
    profile_ = testing_profile_manager_.CreateTestingProfile(
        kProfileName, std::move(testing_factories));
    profile_->SetDownloadManagerForTesting(std::move(download_manager));
    EXPECT_CALL(*download_manager_, GetBrowserContext())
        .WillRepeatedly(Return(profile_));

    download_core_service_ =
        DownloadCoreServiceFactory::GetInstance()->GetForBrowserContext(
            profile_);

    offline_content_provider_ = std::make_unique<
        NiceMock<offline_items_collection::MockOfflineContentProvider>>();
    OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey())
        ->RegisterProvider(kProviderNamespace, offline_content_provider_.get());

    update_service_ = static_cast<DownloadBubbleUpdateService*>(
        DownloadBubbleUpdateServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_,
                base::BindRepeating(
                    &DownloadBubbleUpdateServiceTest::CreateUpdateService,
                    base::Unretained(this), update_service_)));
    InitializeUpdateService();
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
    // This must be shut down before the DCS is destroyed.
    download_core_service_->GetDownloadManagerDelegate()->Shutdown();
    offline_content_provider_.reset();
    update_service_ = nullptr;
    download_manager_ = nullptr;
    download_core_service_ = nullptr;
    profile_ = nullptr;
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
                        const webapps::AppId* web_app_id = nullptr,
                        bool is_crx = false,
                        bool observe = true) {
    InitDownloadItem(*download_manager_, *update_service_, download_items_,
                     profile_, state, guid, is_paused, start_time, web_app_id,
                     is_crx, observe);
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
      const webapps::AppId* web_app_id = nullptr,
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
    if (state == DownloadState::COMPLETE) {
      EXPECT_CALL(item, GetEndTime())
          .WillRepeatedly(Return(start_time + base::Minutes(1)));
    } else {
      EXPECT_CALL(item, GetEndTime()).WillRepeatedly(Return(base::Time()));
    }
    base::FilePath::StringType filename;
#if BUILDFLAG(IS_WIN)
    filename = base::UTF8ToWide(guid);
#else
    filename = guid;
#endif
    EXPECT_CALL(item, GetTargetFilePath())
        .WillRepeatedly(ReturnRefOfCopy(base::FilePath(filename)));
    EXPECT_CALL(item, GetFileNameToReportUser())
        .WillRepeatedly(Return(base::FilePath(filename)));
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
    if (web_app_id != nullptr) {
      DownloadItemWebAppData::CreateAndAttachToItem(&item, *web_app_id);
    }
    if (observe) {
      item.AddObserver(&update_service.download_item_notifier_for_testing());
      item.NotifyObserversDownloadUpdated();
    }
  }

  void UpdateDownloadItem(
      download::MockDownloadItem& item,
      DownloadState state,
      bool is_paused = false,
      DownloadDangerType danger_type =
          DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
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

  // Overload of the above that identifies the item by index.
  void UpdateDownloadItem(
      int item_index,
      DownloadState state,
      bool is_paused = false,
      DownloadDangerType danger_type =
          DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    DCHECK_GT(download_items_.size(), static_cast<size_t>(item_index));
    auto& item = GetDownloadItem(item_index);
    UpdateDownloadItem(item, state, is_paused, danger_type);
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
      item.title = ids[i];
      item.creation_time = start_times[i];
      if (states[i] != OfflineItemState::COMPLETE) {
        item.time_remaining_ms = 1000 * 60;
        item.progress.value = 50;
        item.progress.max = 100;
        item.progress.unit =
            offline_items_collection::OfflineItemProgressUnit::PERCENTAGE;
      }
      new_items.push_back(item);
      offline_items_.push_back(std::move(item));
    }
    offline_content_provider_->SetItems(new_items);
    offline_content_provider_->NotifyOnItemsAdded(new_items);
  }

  void UpdateOfflineItem(int item_index, OfflineItemState state) {
    offline_items_[item_index].state = state;
    UpdateDelta delta;
    delta.state_changed = true;
    offline_content_provider_->NotifyOnItemUpdated(offline_items_[item_index],
                                                   delta);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<NiceMock<content::MockDownloadManager>, DanglingUntriaged>
      download_manager_ = nullptr;
  std::vector<std::unique_ptr<NiceMockDownloadItem>> download_items_;
  std::vector<offline_items_collection::OfflineItem> offline_items_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_ = nullptr;
  std::unique_ptr<
      NiceMock<offline_items_collection::MockOfflineContentProvider>>
      offline_content_provider_;
  raw_ptr<DownloadBubbleUpdateService, DanglingUntriaged> update_service_ =
      nullptr;
  raw_ptr<DownloadCoreService> download_core_service_ = nullptr;
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
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_download");
  EXPECT_EQ(models[1]->GetContentId().id, "in_progress_paused_download");
  EXPECT_EQ(models[2]->GetContentId().id, "completed_download");
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              UnorderedElementsAre(HasSubstr16(u"in_progress_active_download"),
                                   HasSubstr16(u"in_progress_paused_download"),
                                   HasSubstr16(u"completed_download")));

  // Recreate the update service to check that it pulls in the existing
  // download items upon initialization.
  auto service = CreateUpdateService(update_service_, profile_);
  update_service_ = static_cast<DownloadBubbleUpdateService*>(service.get());
  InitializeUpdateService();

  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
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
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 6u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_offline_item");
  EXPECT_EQ(models[1]->GetContentId().id, "in_progress_active_download");
  EXPECT_EQ(models[2]->GetContentId().id, "in_progress_paused_download");
  EXPECT_EQ(models[3]->GetContentId().id, "in_progress_paused_offline_item");
  EXPECT_EQ(models[4]->GetContentId().id, "completed_download");
  EXPECT_EQ(models[5]->GetContentId().id, "completed_offline_item");
  // Only have alerts for the new offline items, not the download items that
  // were preexisting when this service instance was initialized.
  EXPECT_THAT(
      update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
      UnorderedElementsAre(HasSubstr16(u"in_progress_active_offline_item"),
                           HasSubstr16(u"in_progress_paused_offline_item"),
                           HasSubstr16(u"completed_offline_item")));

  // Manually clean up the second service instance to avoid UAF.
  RemoveDownloadItemsObserver(
      download_items_, update_service_->download_item_notifier_for_testing());
  update_service_ = nullptr;
}

TEST_F(DownloadBubbleUpdateServiceTest, AddsNonCrxDownloadItems) {
  InitDownloadItem(DownloadState::IN_PROGRESS, "new_download",
                   /*is_paused=*/false, base::Time::Now(),
                   /*web_app_id=*/nullptr, /*is_crx=*/false,
                   /*observe=*/false);
  // Manually notify the service of the new download rather than going through
  // the observer update notification in InitDownloadItem().
  update_service_->OnDownloadCreated(download_manager_.get(),
                                     &GetDownloadItem(0));
  DownloadUIModelPtrVector models;
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "new_download");
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              UnorderedElementsAre(HasSubstr16(u"new_download")));
}

TEST_F(DownloadBubbleUpdateServiceTest, DelaysCrx) {
  InitDownloadItem(DownloadState::IN_PROGRESS, "in_progress_crx",
                   /*is_paused=*/false, base::Time::Now(),
                   /*web_app_id=*/nullptr, /*is_crx=*/true,
                   /*observe=*/false);
  // Manually notify the service of the new download rather than going through
  // the observer update notification in InitDownloadItem().
  update_service_->OnDownloadCreated(download_manager_, &GetDownloadItem(0));

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  // The crx download does not show up immediately.
  EXPECT_EQ(models.size(), 0u);
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              IsEmpty());

  // Updates are also withheld.
  UpdateDownloadItem(0, DownloadState::IN_PROGRESS, /*is_paused=*/true);
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  EXPECT_EQ(models.size(), 0u);
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              IsEmpty());

  task_environment_.FastForwardBy(base::Seconds(2));

  // After the delay, the crx is added.
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_crx");
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              UnorderedElementsAre(HasSubstr16(u"in_progress_crx")));
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
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_download");
  EXPECT_EQ(models[1]->GetContentId().id, "in_progress_paused_download");
  EXPECT_EQ(models[2]->GetContentId().id, "completed_download_older");

  // Add another item. This sorts before the last item and should cause it to be
  // evicted.
  InitDownloadItem(DownloadState::COMPLETE, "completed_download_newer",
                   /*is_paused=*/false, now);

  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_download");
  EXPECT_EQ(models[1]->GetContentId().id, "in_progress_paused_download");
  EXPECT_EQ(models[2]->GetContentId().id, "completed_download_newer");
  // Alerts for all the downloads are included.
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              UnorderedElementsAre(HasSubstr16(u"in_progress_active_download"),
                                   HasSubstr16(u"in_progress_paused_download"),
                                   HasSubstr16(u"completed_download_older"),
                                   HasSubstr16(u"completed_download_newer")));

  // Remove an item. The previously evicted item should come back via backfill.
  RemoveDownloadItem(1);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_download");
  EXPECT_EQ(models[1]->GetContentId().id, "completed_download_newer");
  EXPECT_EQ(models[2]->GetContentId().id, "completed_download_older");
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              IsEmpty());
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
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "older");
  EXPECT_EQ(models[2]->GetContentId().id, "even_older");
  // Alerts for all the downloads are included.
  EXPECT_THAT(
      update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
      UnorderedElementsAre(HasSubstr16(u"now"), HasSubstr16(u"older"),
                           HasSubstr16(u"even_older"), HasSubstr16(u"oldest")));

  // Update the newest item to be paused, so it should sort behind all the other
  // items.
  UpdateDownloadItem(0, DownloadState::IN_PROGRESS, /*is_paused=*/true);
  task_environment_.RunUntilIdle();

  // The oldest download, previously too low in sort order to display, is
  // retrieved after backfilling.
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "older");
  EXPECT_EQ(models[1]->GetContentId().id, "even_older");
  EXPECT_EQ(models[2]->GetContentId().id, "oldest");
  // Only the item that was updated has an alert.
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              UnorderedElementsAre(HasSubstr16(u"now")));
}

TEST_F(DownloadBubbleUpdateServiceTest, UpdatesOfflineItems) {
  base::Time now = base::Time::Now();
  InitOfflineItems({OfflineItemState::IN_PROGRESS},
                   {"in_progress_active_offline_item"}, {now});

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_offline_item");
  EXPECT_EQ(models[0]->GetState(), DownloadState::IN_PROGRESS);
  EXPECT_THAT(
      update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
      UnorderedElementsAre(HasSubstr16(u"in_progress_active_offline_item")));

  UpdateOfflineItem(0, OfflineItemState::COMPLETE);
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_offline_item");
  EXPECT_EQ(models[0]->GetState(), DownloadState::COMPLETE);
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              UnorderedElementsAre(
                  AllOf(HasSubstr16(u"in_progress_active_offline_item"),
                        HasSubstr16(u"complete"))));
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
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_active_offline_item");
  EXPECT_EQ(models[1]->GetContentId().id, "in_progress_paused_offline_item");
  EXPECT_EQ(models[2]->GetContentId().id, "completed_offline_item");
  EXPECT_THAT(
      update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
      UnorderedElementsAre(HasSubstr16(u"in_progress_active_offline_item"),
                           HasSubstr16(u"in_progress_paused_offline_item"),
                           HasSubstr16(u"completed_offline_item")));

  offline_content_provider_->NotifyOnItemRemoved(models[0]->GetContentId());
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 2u);
  EXPECT_EQ(models[0]->GetContentId().id, "in_progress_paused_offline_item");
  EXPECT_EQ(models[1]->GetContentId().id, "completed_offline_item");
  // No alert generated for removal.
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              IsEmpty());
}

TEST_F(DownloadBubbleUpdateServiceTest, DoesNotAddExpiredItems) {
  base::Time too_old_time = base::Time::Now() - base::Hours(25);
  InitDownloadItem(DownloadState::IN_PROGRESS, "old",
                   /*is_paused=*/false, too_old_time);
  DownloadUIModelPtrVector models;
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  EXPECT_EQ(models.size(), 0u);
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              IsEmpty());
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
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 4u);
  EXPECT_EQ(models[0]->GetContentId().id, "now_download");
  EXPECT_EQ(models[1]->GetContentId().id, "two_hours_ago_download");
  EXPECT_EQ(models[2]->GetContentId().id, "now_offline_item");
  EXPECT_EQ(models[3]->GetContentId().id, "two_hours_ago_offline_item");
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              UnorderedElementsAre(HasSubstr16(u"now_download"),
                                   HasSubstr16(u"two_hours_ago_download"),
                                   HasSubstr16(u"now_offline_item"),
                                   HasSubstr16(u"two_hours_ago_offline_item")));

  // Fast forward so that the older items become too old.
  task_environment_.FastForwardBy(base::Hours(23));

  // Only the newer items should remain.
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 2u);
  EXPECT_EQ(models[0]->GetContentId().id, "now_download");
  EXPECT_EQ(models[1]->GetContentId().id, "now_offline_item");
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              IsEmpty());
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
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "recent");
  EXPECT_EQ(models[2]->GetContentId().id, "two_hours_ago");

  task_environment_.FastForwardBy(base::Hours(23));

  // Since items are pruned, return the unpruned ones immediately and indicate
  // that results are not complete.
  EXPECT_FALSE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 2u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "recent");

  // Sometime later, once the backfilling is complete, we will start to return
  // all the non-expired items.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
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
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "recent");
  EXPECT_EQ(models[2]->GetContentId().id, "two_hours_ago");

  task_environment_.FastForwardBy(base::Hours(23));

  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(
      models, /*web_app_id=*/nullptr, /*force_backfill_download_items=*/true));
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
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "recent");
  EXPECT_EQ(models[2]->GetContentId().id, "two_hours_ago");

  task_environment_.FastForwardBy(base::Hours(23));

  // This returns true despite not forcing a backfill, because the extra item
  // in the cache is available to be returned.
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 3u);
  EXPECT_EQ(models[0]->GetContentId().id, "now");
  EXPECT_EQ(models[1]->GetContentId().id, "recent");
  EXPECT_EQ(models[2]->GetContentId().id, "now_paused");
}

// Test that downloads from web apps are only displayed when queried for the
// specific web app.
TEST_F(DownloadBubbleUpdateServiceTest, GetAllModelsToDisplayForWebApp) {
  base::Time now = base::Time::Now();
  base::Time before = now - base::Hours(1);
  webapps::AppId app_a_id = "app_a";
  webapps::AppId app_b_id = "app_b";
  InitDownloadItem(DownloadState::IN_PROGRESS, "app_a_download",
                   /*is_paused=*/false, now, &app_a_id);
  InitDownloadItem(DownloadState::IN_PROGRESS, "app_b_download",
                   /*is_paused=*/false, now, &app_b_id);
  InitDownloadItem(DownloadState::IN_PROGRESS, "non_app_download",
                   /*is_paused=*/false, now);

  // Offline items should only be returned for non-web-app queries.
  InitOfflineItems({OfflineItemState::IN_PROGRESS}, {"offline_item"}, {before});

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 2u);
  EXPECT_EQ(models[0]->GetContentId().id, "non_app_download");
  EXPECT_EQ(models[1]->GetContentId().id, "offline_item");
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(nullptr),
              UnorderedElementsAre(HasSubstr16(u"non_app_download"),
                                   HasSubstr16(u"offline_item")));

  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models, &app_a_id));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "app_a_download");
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(&app_a_id),
              UnorderedElementsAre(HasSubstr16(u"app_a_download")));

  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models, &app_b_id));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "app_b_download");
  EXPECT_THAT(update_service_->TakeAccessibleAlertsForAnnouncement(&app_b_id),
              UnorderedElementsAre(HasSubstr16(u"app_b_download")));
}

TEST_F(DownloadBubbleUpdateServiceTest, GetProgressInfo) {
  InitDownloadItem(DownloadState::IN_PROGRESS, "in_progress_active",
                   /*is_paused=*/false);
  InitDownloadItem(DownloadState::IN_PROGRESS, "in_progress_paused",
                   /*is_paused=*/true);
  InitDownloadItem(DownloadState::COMPLETE, "completed",
                   /*is_paused=*/false);

  DownloadDisplay::ProgressInfo progress_info =
      update_service_->GetProgressInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(progress_info.download_count, 2);
  EXPECT_TRUE(progress_info.progress_certain);
  EXPECT_EQ(progress_info.progress_percentage, 50);

  InitOfflineItems({OfflineItemState::IN_PROGRESS}, {"offline_item"},
                   {base::Time::Now()});

  progress_info = update_service_->GetProgressInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(progress_info.download_count, 3);
  EXPECT_FALSE(progress_info.progress_certain);
  EXPECT_EQ(progress_info.progress_percentage, 50);
}

TEST_F(DownloadBubbleUpdateServiceTest, GetProgressInfoForWebApp) {
  base::Time now = base::Time::Now();
  webapps::AppId app_a_id = "app_a";
  webapps::AppId app_b_id = "app_b";
  InitDownloadItem(DownloadState::IN_PROGRESS, "app_a_download1",
                   /*is_paused=*/false, now, &app_a_id);
  InitDownloadItem(DownloadState::IN_PROGRESS, "app_a_download2",
                   /*is_paused=*/false, now, &app_a_id);
  InitDownloadItem(DownloadState::IN_PROGRESS, "app_b_download1",
                   /*is_paused=*/false, now, &app_b_id);
  InitDownloadItem(DownloadState::IN_PROGRESS, "app_b_download2",
                   /*is_paused=*/false, now, &app_b_id);
  InitDownloadItem(DownloadState::IN_PROGRESS, "app_b_download3",
                   /*is_paused=*/false, now, &app_b_id);
  InitDownloadItem(DownloadState::IN_PROGRESS, "non_app_download",
                   /*is_paused=*/false, now);

  DownloadDisplay::ProgressInfo non_app_progress_info =
      update_service_->GetProgressInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(non_app_progress_info.download_count, 1);
  EXPECT_TRUE(non_app_progress_info.progress_certain);
  EXPECT_EQ(non_app_progress_info.progress_percentage, 50);

  DownloadDisplay::ProgressInfo app_a_progress_info =
      update_service_->GetProgressInfo(&app_a_id);
  EXPECT_EQ(app_a_progress_info.download_count, 2);
  EXPECT_TRUE(app_a_progress_info.progress_certain);
  EXPECT_EQ(app_a_progress_info.progress_percentage, 50);

  DownloadDisplay::ProgressInfo app_b_progress_info =
      update_service_->GetProgressInfo(&app_b_id);
  EXPECT_EQ(app_b_progress_info.download_count, 3);
  EXPECT_TRUE(app_b_progress_info.progress_certain);
  EXPECT_EQ(app_b_progress_info.progress_percentage, 50);
}

TEST_F(DownloadBubbleUpdateServiceTest, GetDisplayInfo_InProgress) {
  base::Time now = base::Time::Now();
  base::Time two_hours_ago = now - base::Hours(2);
  InitDownloadItem(DownloadState::IN_PROGRESS, "now_download",
                   /*is_paused=*/false, now);
  InitDownloadItem(DownloadState::IN_PROGRESS, "two_hours_ago_download",
                   /*is_paused=*/false, two_hours_ago);
  InitOfflineItems({OfflineItemState::PAUSED, OfflineItemState::PAUSED},
                   {"now_offline_item", "two_hours_ago_offline_item"},
                   {now, two_hours_ago});

  DownloadBubbleDisplayInfo info =
      update_service_->GetDisplayInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(info.all_models_size, 4u);
  // The last_completed_time is null, because no downloads have finished.
  EXPECT_EQ(info.last_completed_time, base::Time());
  EXPECT_EQ(info.in_progress_count, 4);
  EXPECT_EQ(info.paused_count, 2);
  EXPECT_TRUE(info.has_unactioned);
  EXPECT_FALSE(info.has_deep_scanning);
}

TEST_F(DownloadBubbleUpdateServiceTest,
       GetDisplayInfo_UpdateForCompleteDownload) {
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

  // Make the download completed to update the last_completed_time.
  UpdateDownloadItem(0, DownloadState::COMPLETE);

  DownloadBubbleDisplayInfo info =
      update_service_->GetDisplayInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(info.all_models_size, 5u);
  EXPECT_EQ(info.last_completed_time, now);
  EXPECT_EQ(info.in_progress_count, 3);
  EXPECT_EQ(info.paused_count, 2);
  EXPECT_TRUE(info.has_unactioned);
  EXPECT_FALSE(info.has_deep_scanning);
}

TEST_F(DownloadBubbleUpdateServiceTest,
       GetDisplayInfo_ExistingCompleteDownload) {
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

  DownloadBubbleDisplayInfo info =
      update_service_->GetDisplayInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(info.all_models_size, 5u);
  // The last_completed_time should be set to the end time of the completed
  // download, which is 1 minute past its start time (set in InitDownloadItem).
  EXPECT_EQ(info.last_completed_time, two_hours_ago + base::Minutes(1));
  EXPECT_EQ(info.in_progress_count, 4);
  EXPECT_EQ(info.paused_count, 2);
  EXPECT_TRUE(info.has_unactioned);
  EXPECT_FALSE(info.has_deep_scanning);
}

TEST_F(DownloadBubbleUpdateServiceTest, GetDisplayInfo_UpdateForDangerous) {
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

  // Make the download dangerous to update the last_completed_time.
  UpdateDownloadItem(
      0, DownloadState::IN_PROGRESS, /*is_paused=*/false,
      DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT);

  DownloadBubbleDisplayInfo info =
      update_service_->GetDisplayInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(info.all_models_size, 5u);
  EXPECT_EQ(info.last_completed_time, now);
  EXPECT_EQ(info.in_progress_count, 3);
  EXPECT_EQ(info.paused_count, 2);
  EXPECT_TRUE(info.has_unactioned);
  EXPECT_FALSE(info.has_deep_scanning);
}

TEST_F(DownloadBubbleUpdateServiceTest, GetDisplayInfoForWebApp) {
  base::Time now = base::Time::Now();
  base::Time two_hours_ago = now - base::Hours(2);
  webapps::AppId app_a_id = "app_a";
  webapps::AppId app_b_id = "app_b";
  InitDownloadItem(DownloadState::IN_PROGRESS, "non_app_download",
                   /*is_paused=*/false, now);
  InitOfflineItems({OfflineItemState::PAUSED, OfflineItemState::PAUSED},
                   {"now_offline_item", "two_hours_ago_offline_item"},
                   {now, two_hours_ago});
  InitDownloadItem(DownloadState::IN_PROGRESS, "app_a_download1",
                   /*is_paused=*/false, now, &app_a_id);
  InitDownloadItem(DownloadState::IN_PROGRESS, "app_a_download2",
                   /*is_paused=*/false, now, &app_a_id);
  InitDownloadItem(DownloadState::IN_PROGRESS, "app_b_download",
                   /*is_paused=*/false, now, &app_b_id);

  DownloadBubbleDisplayInfo non_app_info =
      update_service_->GetDisplayInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(non_app_info.all_models_size, 3u);
  EXPECT_EQ(non_app_info.paused_count, 2);
  DownloadBubbleDisplayInfo app_a_info =
      update_service_->GetDisplayInfo(&app_a_id);
  EXPECT_EQ(app_a_info.all_models_size, 2u);
  DownloadBubbleDisplayInfo app_b_info =
      update_service_->GetDisplayInfo(&app_b_id);
  EXPECT_EQ(app_b_info.all_models_size, 1u);
}

TEST_F(DownloadBubbleUpdateServiceTest,
       DownloadUpdatedWithWebAppDataAfterCreation) {
  base::Time now = base::Time::Now();
  webapps::AppId app_id = "app";
  // This simulates the restoration of a web app download from the history
  // database, during which the item is created first without the
  // DownloadItemWebAppData, and then subsequently tagged with the data.
  InitDownloadItem(DownloadState::IN_PROGRESS, "app_download",
                   /*is_paused=*/false, now);
  DownloadItemWebAppData::CreateAndAttachToItem(&GetDownloadItem(0), app_id);
  GetDownloadItem(0).NotifyObserversDownloadUpdated();

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  EXPECT_TRUE(models.empty());

  EXPECT_TRUE(update_service_->GetAllModelsToDisplay(models, &app_id));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "app_download");

  DownloadDisplay::ProgressInfo non_app_progress_info =
      update_service_->GetProgressInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(non_app_progress_info.download_count, 0);

  DownloadDisplay::ProgressInfo app_progress_info =
      update_service_->GetProgressInfo(&app_id);
  EXPECT_EQ(app_progress_info.download_count, 1);

  DownloadBubbleDisplayInfo non_app_info =
      update_service_->GetDisplayInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(non_app_info.all_models_size, 0u);
  DownloadBubbleDisplayInfo app_info = update_service_->GetDisplayInfo(&app_id);
  EXPECT_EQ(app_info.all_models_size, 1u);
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
    TestingProfile::Builder builder;
    TestingProfile::TestingFactories testing_factories;
    testing_factories.emplace_back(HistoryServiceFactory::GetInstance(),
                                   HistoryServiceFactory::GetDefaultFactory());
    testing_factories.emplace_back(
        DownloadCoreServiceFactory::GetInstance(),
        base::BindRepeating(&SetUpDownloadCoreService, download_manager_));
    builder.AddTestingFactories(std::move(testing_factories));
    incognito_profile_ = builder.BuildIncognito(profile_);
    std::unique_ptr<NiceMock<content::MockDownloadManager>>
        incognito_download_manager = SetUpDownloadManager();
    incognito_download_manager_ = incognito_download_manager.get();
    EXPECT_CALL(*incognito_download_manager_, GetBrowserContext())
        .WillRepeatedly(Return(incognito_profile_));
    incognito_profile_->SetDownloadManagerForTesting(
        std::move(incognito_download_manager));
    incognito_update_service_ = static_cast<DownloadBubbleUpdateService*>(
        DownloadBubbleUpdateServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                incognito_profile_,
                base::BindRepeating(
                    &DownloadBubbleUpdateServiceTest::CreateUpdateService,
                    base::Unretained(this), incognito_update_service_)));
    incognito_download_core_service_ =
        DownloadCoreServiceFactory::GetInstance()->GetForBrowserContext(
            incognito_profile_);
    // Do not call Initialize here to delay registering the download manager for
    // the test.
  }

  void TearDown() override {
    RemoveDownloadItemsObserver(
        incognito_download_items_,
        incognito_update_service_->download_item_notifier_for_testing());
    RemoveDownloadItemsObserver(
        download_items_, incognito_update_service_
                             ->original_download_item_notifier_for_testing());
    incognito_download_core_service_->GetDownloadManagerDelegate()->Shutdown();
    incognito_update_service_ = nullptr;
    incognito_download_manager_ = nullptr;
    incognito_download_core_service_ = nullptr;
    incognito_profile_ = nullptr;
    DownloadBubbleUpdateServiceTest::TearDown();
  }

 protected:
  raw_ptr<Profile, DanglingUntriaged> incognito_profile_ = nullptr;
  raw_ptr<NiceMock<content::MockDownloadManager>, DanglingUntriaged>
      incognito_download_manager_ = nullptr;
  std::vector<std::unique_ptr<NiceMockDownloadItem>> incognito_download_items_;
  raw_ptr<DownloadBubbleUpdateService, DanglingUntriaged>
      incognito_update_service_ = nullptr;
  raw_ptr<DownloadCoreService> incognito_download_core_service_ = nullptr;
};

// Tests that initializing an update service for an incognito profile sets both
// the download manager and the original download manager.
TEST_F(DownloadBubbleUpdateServiceIncognitoTest, InitIncognito) {
  base::Time now = base::Time::Now();
  // |observe| is false because this only tests initialization.
  InitDownloadItem(DownloadState::COMPLETE, "regular_profile_download",
                   /*is_paused=*/false, now - base::Hours(1),
                   /*web_app_id=*/nullptr, /*is_crx=*/false,
                   /*observe=*/false);
  InitDownloadItem(*incognito_download_manager_, *incognito_update_service_,
                   incognito_download_items_, incognito_profile_,
                   DownloadState::COMPLETE, "incognito_profile_download",
                   /*is_paused=*/false, now, /*web_app_id=*/nullptr,
                   /*is_crx=*/false,
                   /*observe=*/false);

  // Initial state: Regular profile's update service has a manager, incognito's
  // does not.
  EXPECT_NE(update_service_->GetDownloadManager(), nullptr);
  EXPECT_EQ(update_service_->GetDownloadManager(), download_manager_);
  EXPECT_EQ(incognito_update_service_->GetDownloadManager(), nullptr);

  incognito_update_service_->Initialize(incognito_download_manager_);
  // Run the offline content provider initialization.
  task_environment_.RunUntilIdle();
  // Regular profile's update service's manager hasn't changed.
  EXPECT_EQ(update_service_->GetDownloadManager(), download_manager_);
  // Incognito profile's update service's manager now set correctly.
  EXPECT_EQ(incognito_update_service_->GetDownloadManager(),
            incognito_download_manager_);

  // Both download items should be present after initialization.
  DownloadUIModelPtrVector models;
  EXPECT_TRUE(incognito_update_service_->GetAllModelsToDisplay(
      models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 2u);
  EXPECT_EQ(models[0]->GetContentId().id, "incognito_profile_download");
  EXPECT_EQ(models[1]->GetContentId().id, "regular_profile_download");
}

// Ephemeral warnings are only enabled when the download bubble is enabled,
// which it is not on ChromeOS Ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that the DownloadBubbleDisplayInfo is updated when a download with an
// ephemeral warning expires.
TEST_F(DownloadBubbleUpdateServiceTest, OnEphemeralWarningExpired) {
  base::Time now = base::Time::Now();
  InitDownloadItem(DownloadState::IN_PROGRESS, "normal_download",
                   /*is_paused=*/false, now);
  InitDownloadItem(DownloadState::IN_PROGRESS, "ephemeral_warning_download",
                   /*is_paused=*/false, now);

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  EXPECT_EQ(models.size(), 2u);
  DownloadBubbleDisplayInfo info =
      update_service_->GetDisplayInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(info.all_models_size, 2u);

  // Mark the download with an ephemeral warning.
  UpdateDownloadItem(1, DownloadState::IN_PROGRESS, /*is_paused=*/false,
                     DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);

  // Simulate showing the warning in the UI and waiting past the expiry time.
  DownloadItemModel(&GetDownloadItem(1)).SetEphemeralWarningUiShownTime(now);
  task_environment_.FastForwardBy(
      DownloadItemModel::kEphemeralWarningLifetimeOnBubble * 2);

  update_service_->OnEphemeralWarningExpired(GetDownloadItem(1).GetGuid());

  // The ephemeral warning download should no longer be observable.
  // Check GetDisplayInfo first, because GetAllModelsToDisplay will prune it.
  info = update_service_->GetDisplayInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(info.all_models_size, 1u);
  EXPECT_TRUE(
      update_service_->GetAllModelsToDisplay(models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "normal_download");
}

// Tests that a download with an ephemeral warning from the original profile is
// properly handled when it expires.
TEST_F(DownloadBubbleUpdateServiceIncognitoTest,
       OnEphemeralWarningExpiredFromOriginalProfile) {
  base::Time now = base::Time::Now();
  InitDownloadItem(DownloadState::COMPLETE, "regular_profile_normal_download",
                   /*is_paused=*/false, now - base::Hours(1),
                   /*web_app_id=*/nullptr, /*is_crx=*/false,
                   /*observe=*/false);
  InitDownloadItem(DownloadState::COMPLETE,
                   "regular_profile_ephemeral_warning_download",
                   /*is_paused=*/false, now - base::Hours(1),
                   /*web_app_id=*/nullptr, /*is_crx=*/false,
                   /*observe=*/false);
  InitDownloadItem(*incognito_download_manager_, *incognito_update_service_,
                   incognito_download_items_, incognito_profile_,
                   DownloadState::COMPLETE,
                   "incognito_profile_ephemeral_warning_download",
                   /*is_paused=*/false, now, /*web_app_id=*/nullptr,
                   /*is_crx=*/false,
                   /*observe=*/false);
  incognito_update_service_->Initialize(incognito_download_manager_);
  // Run the offline content provider initialization.
  task_environment_.RunUntilIdle();

  DownloadUIModelPtrVector models;
  EXPECT_TRUE(incognito_update_service_->GetAllModelsToDisplay(
      models, /*web_app_id=*/nullptr));
  EXPECT_EQ(models.size(), 3u);
  DownloadBubbleDisplayInfo info =
      incognito_update_service_->GetDisplayInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(info.all_models_size, 3u);

  // Mark the regular profile ephemeral download with an ephemeral warning.
  UpdateDownloadItem(1, DownloadState::IN_PROGRESS, /*is_paused=*/false,
                     DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  // Mark the incognito profile ephemeral download with an ephemeral warning.
  UpdateDownloadItem(*incognito_download_items_[0], DownloadState::IN_PROGRESS,
                     /*is_paused=*/false,
                     DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);

  // Simulate showing the warning in the UI and waiting past the expiry time.
  DownloadItemModel(&GetDownloadItem(1)).SetEphemeralWarningUiShownTime(now);
  DownloadItemModel(incognito_download_items_[0].get())
      .SetEphemeralWarningUiShownTime(now);
  task_environment_.FastForwardBy(
      DownloadItemModel::kEphemeralWarningLifetimeOnBubble * 2);

  incognito_update_service_->OnEphemeralWarningExpired(
      GetDownloadItem(1).GetGuid());
  incognito_update_service_->OnEphemeralWarningExpired(
      incognito_download_items_[0]->GetGuid());

  // The ephemeral warning downloads should no longer be observable.
  // Check GetDisplayInfo first, because GetAllModelsToDisplay will prune
  // them.
  info = incognito_update_service_->GetDisplayInfo(/*web_app_id=*/nullptr);
  EXPECT_EQ(info.all_models_size, 1u);
  EXPECT_TRUE(incognito_update_service_->GetAllModelsToDisplay(
      models, /*web_app_id=*/nullptr));
  ASSERT_EQ(models.size(), 1u);
  EXPECT_EQ(models[0]->GetContentId().id, "regular_profile_normal_download");
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
