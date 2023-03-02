// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_controller.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/bubble/download_display.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/bubble/download_icon_state.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::ReturnRefOfCopy;
using testing::SetArgPointee;

namespace {
using StrictMockDownloadItem = testing::StrictMock<download::MockDownloadItem>;
using DownloadDangerType = download::DownloadDangerType;
using DownloadIconState = download::DownloadIconState;
using DownloadState = download::DownloadItem::DownloadState;
const char kProviderNamespace[] = "mock_namespace";

class MockDownloadDisplayController : public DownloadDisplayController {
 public:
  MockDownloadDisplayController(Browser* browser,
                                DownloadBubbleUIController* bubble_controller)
      : DownloadDisplayController(nullptr, browser, bubble_controller) {}
  void MaybeShowButtonWhenCreated() override {}
  MOCK_METHOD1(OnNewItem, void(bool));
  MOCK_METHOD3(OnUpdatedItem, void(bool, bool, bool));
  MOCK_METHOD1(OnRemovedItem, void(const ContentId&));
};

struct DownloadSortingState {
  std::string id;
  base::TimeDelta offset;
  DownloadState state;
  bool is_paused;
  DownloadSortingState(const std::string& id,
                       base::TimeDelta offset,
                       DownloadState state,
                       bool is_paused) {
    this->id = id;
    this->offset = offset;
    this->state = state;
    this->is_paused = is_paused;
  }
};

}  // namespace

class DownloadBubbleUIControllerTest : public testing::Test {
 public:
  DownloadBubbleUIControllerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  DownloadBubbleUIControllerTest(const DownloadBubbleUIControllerTest&) =
      delete;
  DownloadBubbleUIControllerTest& operator=(
      const DownloadBubbleUIControllerTest&) = delete;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");
    auto manager = std::make_unique<NiceMock<content::MockDownloadManager>>();
    manager_ = manager.get();
    EXPECT_CALL(*manager, GetBrowserContext())
        .WillRepeatedly(Return(profile_.get()));
    EXPECT_CALL(*manager, RemoveObserver(_)).WillRepeatedly(Return());
    profile_->SetDownloadManagerForTesting(std::move(manager));

    // Set test delegate to get the corresponding download prefs.
    auto delegate = std::make_unique<ChromeDownloadManagerDelegate>(profile_);
    DownloadCoreServiceFactory::GetForBrowserContext(profile_)
        ->SetDownloadManagerDelegateForTesting(std::move(delegate));

    content_provider_ = std::make_unique<
        NiceMock<offline_items_collection::MockOfflineContentProvider>>();
    OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey())
        ->RegisterProvider(kProviderNamespace, content_provider_.get());

    window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile_, true);
    params.type = Browser::TYPE_NORMAL;
    params.window = window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));
    controller_ = std::make_unique<DownloadBubbleUIController>(browser_.get());
    display_controller_ =
        std::make_unique<NiceMock<MockDownloadDisplayController>>(
            browser_.get(), controller_.get());
    second_controller_ =
        std::make_unique<DownloadBubbleUIController>(browser_.get());
    second_display_controller_ =
        std::make_unique<NiceMock<MockDownloadDisplayController>>(
            browser_.get(), second_controller_.get());
    controller_->set_manager_for_testing(manager_);
    second_controller_->set_manager_for_testing(manager_);
  }

  void TearDown() override {
    DownloadCoreServiceFactory::GetForBrowserContext(profile_)
        ->SetDownloadManagerDelegateForTesting(nullptr);
    for (auto& item : items_) {
      item->RemoveObserver(&controller_->get_download_notifier_for_testing());
      item->RemoveObserver(
          &second_controller_->get_download_notifier_for_testing());
    }
    // The controller needs to be reset before download manager, because the
    // download_notifier_ will unregister itself from the manager.
    controller_.reset();
    second_controller_.reset();
    display_controller_.reset();
    second_display_controller_.reset();
  }

 protected:
  NiceMock<content::MockDownloadManager>& manager() { return *manager_; }
  download::MockDownloadItem& item(size_t index) { return *items_[index]; }
  std::vector<std::unique_ptr<StrictMockDownloadItem>>& items() {
    return items_;
  }
  NiceMock<MockDownloadDisplayController>& display_controller() {
    return *display_controller_;
  }
  DownloadBubbleUIController& controller() { return *controller_; }
  DownloadBubbleUIController& second_controller() {
    return *second_controller_;
  }
  TestingProfile* profile() { return profile_; }
  NiceMock<offline_items_collection::MockOfflineContentProvider>&
  content_provider() {
    return *content_provider_;
  }

  void InitDownloadItem(
      const base::FilePath::CharType* path,
      DownloadState state,
      const std::string& id,
      bool is_transient = false,
      base::Time start_time = base::Time::Now(),
      bool may_show_animation = true,
      download::DownloadItem::TargetDisposition target_disposition =
          download::DownloadItem::TARGET_DISPOSITION_PROMPT,
      const std::string& mime_type = "",
      download::DownloadItem::DownloadCreationType creation_type =
          download::DownloadItem::DownloadCreationType::TYPE_ACTIVE_DOWNLOAD) {
    size_t index = items_.size();
    items_.push_back(std::make_unique<StrictMockDownloadItem>());
    EXPECT_CALL(item(index), GetId())
        .WillRepeatedly(Return(static_cast<uint32_t>(items_.size() + 1)));
    EXPECT_CALL(item(index), GetGuid()).WillRepeatedly(ReturnRef(id));
    EXPECT_CALL(item(index), GetState()).WillRepeatedly(Return(state));
    EXPECT_CALL(item(index), GetStartTime()).WillRepeatedly(Return(start_time));
    EXPECT_CALL(item(index), GetTargetFilePath())
        .WillRepeatedly(
            ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
    EXPECT_CALL(item(index), GetLastReason())
        .WillRepeatedly(Return(download::DOWNLOAD_INTERRUPT_REASON_NONE));
    EXPECT_CALL(item(index), GetInsecureDownloadStatus())
        .WillRepeatedly(
            Return(download::DownloadItem::InsecureDownloadStatus::SAFE));
    int received_bytes =
        state == download::DownloadItem::IN_PROGRESS ? 50 : 100;
    EXPECT_CALL(item(index), GetReceivedBytes())
        .WillRepeatedly(Return(received_bytes));
    EXPECT_CALL(item(index), GetTotalBytes()).WillRepeatedly(Return(100));
    EXPECT_CALL(item(index), IsDone()).WillRepeatedly(Return(false));
    EXPECT_CALL(item(index), IsTransient())
        .WillRepeatedly(Return(is_transient));
    EXPECT_CALL(item(index), GetDownloadCreationType())
        .WillRepeatedly(Return(creation_type));
    EXPECT_CALL(item(index), IsPaused()).WillRepeatedly(Return(false));
    EXPECT_CALL(item(index), IsDangerous()).WillRepeatedly(Return(false));
    // Functions called when checking ShouldShowDownloadStartedAnimation().
    EXPECT_CALL(item(index), IsSavePackageDownload())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(item(index), GetTargetDisposition())
        .WillRepeatedly(Return(target_disposition));
    EXPECT_CALL(item(index), GetMimeType()).WillRepeatedly(Return(mime_type));
    EXPECT_CALL(item(index), GetURL())
        .WillRepeatedly(ReturnRef(GURL::EmptyGURL()));
    EXPECT_CALL(item(index), GetReferrerUrl())
        .WillRepeatedly(ReturnRef(GURL::EmptyGURL()));
    std::vector<download::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    EXPECT_CALL(*manager_, GetAllDownloads(_))
        .WillRepeatedly(SetArgPointee<0>(items));
    item(index).AddObserver(&controller().get_download_notifier_for_testing());
    content::DownloadItemUtils::AttachInfoForTesting(&(item(index)), profile_,
                                                     nullptr);
    controller().OnNewItem(&item(index), may_show_animation);
  }

  void UpdateDownloadItem(
      int item_index,
      DownloadState state,
      bool is_paused = false,
      DownloadDangerType danger_type =
          DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    DCHECK_GT(items_.size(), static_cast<size_t>(item_index));
    EXPECT_CALL(item(item_index), GetState()).WillRepeatedly(Return(state));
    if (state == DownloadState::COMPLETE) {
      EXPECT_CALL(item(item_index), IsDone()).WillRepeatedly(Return(true));
      DownloadPrefs::FromDownloadManager(&manager())
          ->SetLastCompleteTime(base::Time::Now());
    } else {
      EXPECT_CALL(item(item_index), IsDone()).WillRepeatedly(Return(false));
    }
    EXPECT_CALL(item(item_index), IsDangerous())
        .WillRepeatedly(
            Return(danger_type !=
                   DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    EXPECT_CALL(item(item_index), GetDangerType())
        .WillRepeatedly(Return(danger_type));
    EXPECT_CALL(item(item_index), IsPaused()).WillRepeatedly(Return(is_paused));
    item(item_index).NotifyObserversDownloadUpdated();
  }

  void InitOfflineItem(OfflineItemState state, std::string id) {
    OfflineItem item;
    item.state = state;
    item.id.id = id;
    offline_items_.push_back(item);
    content_provider().NotifyOnItemsAdded({item});
  }

  void UpdateOfflineItem(int item_index, OfflineItemState state) {
    offline_items_[item_index].state = state;
    UpdateDelta delta;
    delta.state_changed = true;
    content_provider().NotifyOnItemUpdated(offline_items_[item_index], delta);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<DownloadBubbleUIController> controller_;
  std::unique_ptr<DownloadBubbleUIController> second_controller_;
  std::unique_ptr<NiceMock<MockDownloadDisplayController>> display_controller_;
  std::unique_ptr<NiceMock<MockDownloadDisplayController>>
      second_display_controller_;
  std::vector<std::unique_ptr<StrictMockDownloadItem>> items_;
  OfflineItemList offline_items_;
  raw_ptr<NiceMock<content::MockDownloadManager>> manager_;
  TestingProfileManager testing_profile_manager_;
  std::unique_ptr<
      NiceMock<offline_items_collection::MockOfflineContentProvider>>
      content_provider_;
  std::unique_ptr<TestBrowserWindow> window_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<TestingProfile> profile_;
};

TEST_F(DownloadBubbleUIControllerTest, ProcessesNewItems) {
  std::vector<std::string> ids = {"Download 1", "Download 2", "Offline 1",
                                  "Download 3", "Offline 2"};
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(2);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0]);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE, ids[1]);
  EXPECT_CALL(display_controller(), OnNewItem(false)).Times(1);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[2]);
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[3],
                   /*is_transient=*/false, base::Time::Now());
  EXPECT_CALL(display_controller(), OnNewItem(false)).Times(1);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[4]);
}

TEST_F(DownloadBubbleUIControllerTest, ProcessesUpdatedItems) {
  std::vector<std::string> ids = {"Download 1", "Offline 1"};
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0]);
  EXPECT_CALL(display_controller(), OnUpdatedItem(false, false, true)).Times(1);
  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS);
  EXPECT_CALL(display_controller(), OnUpdatedItem(true, false, true)).Times(1);
  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);

  EXPECT_CALL(display_controller(), OnNewItem(false)).Times(1);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[1]);
  EXPECT_CALL(display_controller(), OnUpdatedItem(true, false, true)).Times(1);
  UpdateOfflineItem(/*item_index=*/0, OfflineItemState::COMPLETE);
}

TEST_F(DownloadBubbleUIControllerTest, UpdatedItemIsPendingDeepScanning) {
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, "Download 1");
  EXPECT_CALL(display_controller(), OnUpdatedItem(false, true, true)).Times(1);
  UpdateDownloadItem(
      /*item_index=*/0, DownloadState::IN_PROGRESS, false,
      DownloadDangerType::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING);
}

TEST_F(DownloadBubbleUIControllerTest, TransientDownloadShouldNotShow) {
  std::vector<std::string> ids = {"Download 1", "Download 2"};
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0],
                   /*is_transient=*/true);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[1],
                   /*is_transient=*/false);
  std::vector<DownloadUIModelPtr> models = controller().GetMainView();
  EXPECT_EQ(models.size(), 1ul);
  EXPECT_EQ(models[0]->GetContentId().id, ids[1]);
}

TEST_F(DownloadBubbleUIControllerTest,
       CompleteHistoryImportShouldNotShowInPartialView) {
  std::vector<std::string> ids = {"history_import1", "history_import2"};
  // Complete history import item.
  InitDownloadItem(
      FILE_PATH_LITERAL("/foo/bar.pdf"), download::DownloadItem::COMPLETE,
      ids[0],
      /*is_transient=*/false, /*start_time=*/base::Time::Now(),
      /*may_show_animation=*/true,
      download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      /*mime_type=*/"",
      download::DownloadItem::DownloadCreationType::TYPE_HISTORY_IMPORT);
  // In-progress history import item.
  InitDownloadItem(
      FILE_PATH_LITERAL("/foo/bar2.pdf"), download::DownloadItem::IN_PROGRESS,
      ids[1],
      /*is_transient=*/false, /*start_time=*/base::Time::Now(),
      /*may_show_animation=*/true,
      download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      /*mime_type=*/"",
      download::DownloadItem::DownloadCreationType::TYPE_HISTORY_IMPORT);
  std::vector<DownloadUIModelPtr> partial_view = controller().GetPartialView();
  ASSERT_EQ(partial_view.size(), 1u);
  EXPECT_EQ(partial_view[0]->GetContentId().id, ids[1]);
  std::vector<DownloadUIModelPtr> main_view = controller().GetMainView();
  EXPECT_EQ(main_view.size(), 2u);
}

TEST_F(DownloadBubbleUIControllerTest, FastCrxDownloadShowsNoUI) {
  std::string id = "fast_crx";
  EXPECT_CALL(display_controller(), OnNewItem(_)).Times(0);
  EXPECT_CALL(display_controller(), OnUpdatedItem(_, _, _)).Times(0);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::IN_PROGRESS, id,
                   /*is_transient=*/false, /*start_time=*/base::Time::Now(),
                   /*may_show_animation=*/true,
                   download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                   "application/x-chrome-extension");
  EXPECT_CALL(*manager_, GetDownloadByGuid(id))
      .WillRepeatedly(Return(items_[0].get()));
  task_environment_.FastForwardBy(base::Seconds(1));
  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
}

TEST_F(DownloadBubbleUIControllerTest, SlowCrxDownloadShowsDelayedUI) {
  std::string id = "slow_crx";
  EXPECT_CALL(display_controller(), OnNewItem(_)).Times(0);
  EXPECT_CALL(display_controller(), OnUpdatedItem(_, _, _)).Times(0);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::IN_PROGRESS, id,
                   /*is_transient=*/false, /*start_time=*/base::Time::Now(),
                   /*may_show_animation=*/true,
                   download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                   "application/x-chrome-extension");
  EXPECT_CALL(*manager_, GetDownloadByGuid(id))
      .WillRepeatedly(Return(items_[0].get()));
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  EXPECT_CALL(display_controller(), OnUpdatedItem(false, false, true)).Times(1);
  task_environment_.FastForwardBy(base::Seconds(2));
  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS);
  EXPECT_CALL(display_controller(), OnUpdatedItem(true, false, true)).Times(1);
  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
}

TEST_F(DownloadBubbleUIControllerTest, ListIsSorted) {
  std::vector<DownloadSortingState> sort_states = {
      DownloadSortingState("Download 1", base::Hours(2),
                           DownloadState::IN_PROGRESS, /*is_paused=*/false),
      DownloadSortingState("Download 2", base::Hours(4),
                           DownloadState::IN_PROGRESS, /*is_paused=*/true),
      DownloadSortingState("Download 3", base::Hours(3),
                           DownloadState::COMPLETE, /*is_paused=*/false),
      DownloadSortingState("Download 4", base::Hours(0),
                           DownloadState::IN_PROGRESS, /*is_paused=*/false),
      DownloadSortingState("Download 5", base::Hours(1),
                           DownloadState::COMPLETE, /*is_paused=*/false)};

  // Offline item will be in-progress. Non in-progress offline items do not
  // surface.
  std::string offline_item = "Offline 1";
  // First non-paused in-progress, then paused in-progress, then completed,
  // sub-sorted by starting times.
  std::vector<std::string> sorted_ids = {"Download 4", "Download 1",
                                         "Offline 1",  "Download 2",
                                         "Download 5", "Download 3"};
  base::Time now = base::Time::Now();
  for (unsigned long i = 0; i < sort_states.size(); i++) {
    InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                     DownloadState::IN_PROGRESS, sort_states[i].id,
                     /*is_transient=*/false, now - sort_states[i].offset);
    UpdateDownloadItem(/*item_index=*/i, sort_states[i].state,
                       sort_states[i].is_paused);
  }
  InitOfflineItem(OfflineItemState::IN_PROGRESS, offline_item);

  std::vector<DownloadUIModelPtr> models = controller().GetMainView();
  EXPECT_EQ(models.size(), sorted_ids.size());
  for (unsigned long i = 0; i < models.size(); i++) {
    EXPECT_EQ(models[i]->GetContentId().id, sorted_ids[i]);
  }
}

TEST_F(DownloadBubbleUIControllerTest, ListIsRecent) {
  std::vector<std::string> ids = {"Download 1", "Download 2", "Download 3",
                                  "Offline 1"};
  std::vector<base::TimeDelta> start_time_offsets = {
      base::Hours(1), base::Hours(25), base::Hours(2)};
  std::vector<std::string> sorted_ids = {"Download 1", "Download 3",
                                         "Offline 1"};
  base::Time now = base::Time::Now();
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0],
                   /*is_transient=*/false, now - start_time_offsets[0]);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[1],
                   /*is_transient=*/false, now - start_time_offsets[1]);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar3.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[2],
                   /*is_transient=*/false, now - start_time_offsets[2]);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[3]);
  std::vector<DownloadUIModelPtr> models = controller().GetMainView();
  EXPECT_EQ(models.size(), sorted_ids.size());
  for (unsigned long i = 0; i < models.size(); i++) {
    EXPECT_EQ(models[i]->GetContentId().id, sorted_ids[i]);
  }
}

// Tests that the list is limited to kMaxDownloadsToShow items, and that they
// are the most recent kMaxDownloadsToShow items.
TEST_F(DownloadBubbleUIControllerTest, ListIsCappedAndMostRecent) {
  const size_t kMaxDownloadsToShow = 100;
  const size_t kNumDownloads = kMaxDownloadsToShow + 1;
  const base::Time kFirstStartTime =
      base::Time::Now() - base::Seconds(kNumDownloads);
  // Create 101 downloads in chronological order, such that the first 100 are
  // *not* the 100 most recent. Note that DownloadManager does not guarantee
  // any order on the items returned from GetAllDownloads(). We still want to
  // ensure that the most recent ones are returned.
  for (size_t i = 0; i < kNumDownloads; ++i) {
    std::string id = base::NumberToString(i);
    InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                     download::DownloadItem::IN_PROGRESS, id,
                     /*is_transient=*/false,
                     kFirstStartTime + base::Seconds(i));
  }

  std::vector<DownloadUIModelPtr> partial_view_models =
      controller().GetPartialView();
  EXPECT_EQ(partial_view_models.size(), kMaxDownloadsToShow);
  for (const DownloadUIModelPtr& model : partial_view_models) {
    // Expect the oldest download, which started at kFirstStartTime, to be the 1
    // excluded, despite being the first returned from GetAllDownloads().
    EXPECT_GT(model->GetStartTime(), kFirstStartTime);
  }

  std::vector<DownloadUIModelPtr> main_view_models = controller().GetMainView();
  EXPECT_EQ(main_view_models.size(), kMaxDownloadsToShow);
  for (const DownloadUIModelPtr& model : main_view_models) {
    // Expect the oldest download, which started at kFirstStartTime, to be the 1
    // excluded, despite being the first returned from GetAllDownloads().
    EXPECT_GT(model->GetStartTime(), kFirstStartTime);
  }
}

TEST_F(DownloadBubbleUIControllerTest,
       OpeningMainViewRemovesCompletedEntryFromPartialView) {
  std::vector<std::string> ids = {"Download 1", "Offline 1"};
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0]);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[1]);

  EXPECT_EQ(controller().GetPartialView().size(), 2ul);
  EXPECT_EQ(second_controller().GetPartialView().size(), 2ul);

  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
  // Completed offline item is removed.
  UpdateOfflineItem(/*item_index=*/0, OfflineItemState::COMPLETE);
  EXPECT_EQ(controller().GetMainView().size(), 1ul);
  // Download was removed from partial view because it is completed.
  EXPECT_EQ(controller().GetPartialView().size(), 0ul);
  EXPECT_EQ(second_controller().GetPartialView().size(), 0ul);
}

TEST_F(DownloadBubbleUIControllerTest,
       OpeningMainViewDoesNotRemoveInProgressEntryFromPartialView) {
  std::vector<std::string> ids = {"Download 1", "Offline 1"};
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0]);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[1]);

  EXPECT_EQ(controller().GetPartialView().size(), 2ul);

  // This does not remove the entries from the partial view because the items
  // are in progress.
  EXPECT_EQ(controller().GetMainView().size(), 2ul);
  EXPECT_EQ(controller().GetPartialView().size(), 2ul);
}

// Tests that no items are returned (i.e. no partial view will be shown) if it
// is too soon since the last partial view has been shown.
TEST_F(DownloadBubbleUIControllerTest, NoItemsReturnedForPartialViewTooSoon) {
  std::vector<std::string> ids = {"Download 1", "Download 2", "Download 3",
                                  "Download 4"};

  // First time showing the partial view should work.
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar1.pdf"),
                   download::DownloadItem::COMPLETE, ids[0]);
  EXPECT_EQ(controller().GetPartialView().size(), 1u);

  // No items are returned for a partial view because it is too soon.
  task_environment_.FastForwardBy(base::Seconds(14));
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE, ids[1]);
  EXPECT_EQ(controller().GetPartialView().size(), 0u);

  // Partial view can now be shown, and contains all the items.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar3.pdf"),
                   download::DownloadItem::COMPLETE, ids[1]);
  EXPECT_EQ(controller().GetPartialView().size(), 3u);

  // Showing the main view even before time is up should still work.
  task_environment_.FastForwardBy(base::Seconds(14));
  EXPECT_EQ(controller().GetPartialView().size(), 0u);
  EXPECT_EQ(controller().GetMainView().size(), 3u);

  // Main view resets the partial view time, so the partial view can now be
  // shown.
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar4.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[3]);
  EXPECT_EQ(controller().GetPartialView().size(), 1u);
}

class DownloadBubbleUIControllerIncognitoTest
    : public DownloadBubbleUIControllerTest {
 public:
  DownloadBubbleUIControllerIncognitoTest() = default;
  DownloadBubbleUIControllerIncognitoTest(
      const DownloadBubbleUIControllerIncognitoTest&) = delete;
  DownloadBubbleUIControllerIncognitoTest& operator=(
      const DownloadBubbleUIControllerIncognitoTest&) = delete;

  void SetUp() override {
    DownloadBubbleUIControllerTest::SetUp();
    incognito_profile_ = TestingProfile::Builder().BuildIncognito(profile());
    incognito_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(incognito_profile_, true);
    params.type = Browser::TYPE_NORMAL;
    params.window = incognito_window_.get();
    incognito_browser_ = std::unique_ptr<Browser>(Browser::Create(params));
    incognito_controller_ =
        std::make_unique<DownloadBubbleUIController>(incognito_browser_.get());
    incognito_display_controller_ =
        std::make_unique<NiceMock<MockDownloadDisplayController>>(
            incognito_browser_.get(), incognito_controller_.get());
  }

  void TearDown() override {
    for (auto& item : items()) {
      item->RemoveObserver(
          incognito_controller_->get_original_notifier_for_testing());
    }
    // The controller needs to be reset before download manager, because the
    // download_notifier_ will unregister itself from the manager.
    incognito_controller_.reset();
    incognito_display_controller_.reset();
    DownloadBubbleUIControllerTest::TearDown();
  }

 protected:
  std::unique_ptr<TestBrowserWindow> incognito_window_;
  std::unique_ptr<Browser> incognito_browser_;
  raw_ptr<TestingProfile> incognito_profile_;
  std::unique_ptr<DownloadBubbleUIController> incognito_controller_;
  std::unique_ptr<NiceMock<MockDownloadDisplayController>>
      incognito_display_controller_;
};

TEST_F(DownloadBubbleUIControllerIncognitoTest,
       IncludeDownloadsFromMainProfile) {
  std::string download_id = "Download 1";
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, download_id);
  std::vector<DownloadUIModelPtr> main_view =
      incognito_controller_->GetMainView();
  // The main view should contain downloads from the main profile.
  EXPECT_EQ(main_view.size(), 1ul);
}

TEST_F(DownloadBubbleUIControllerIncognitoTest, DoesNotShowDetailsIfDone) {
  std::string download_id = "Download 1";
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, download_id);
  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
  item(0).AddObserver(
      incognito_controller_->get_original_notifier_for_testing());
  content::DownloadItemUtils::AttachInfoForTesting(&(item(0)),
                                                   incognito_profile_, nullptr);
  // `may_show_details` is false because the download is initiated from the
  // main profile.
  EXPECT_CALL(
      *incognito_display_controller_,
      OnUpdatedItem(/*is_done=*/true, /*is_pending_deep_scanning=*/false,
                    /*may_show_details=*/false))
      .Times(1);
  item(0).NotifyObserversDownloadUpdated();
}
