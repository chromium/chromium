// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_display_controller.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/download/bubble/download_display.h"
#include "chrome/browser/download/bubble/download_icon_state.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/offline_items_collection/core/offline_item.h"
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
using DownloadIconState = download::DownloadIconState;
using DownloadState = download::DownloadItem::DownloadState;
using OfflineItemState = offline_items_collection::OfflineItemState;

class FakeDownloadDisplay : public DownloadDisplay {
 public:
  FakeDownloadDisplay() = default;
  FakeDownloadDisplay(const FakeDownloadDisplay&) = delete;
  FakeDownloadDisplay& operator=(const FakeDownloadDisplay&) = delete;

  void SetController(DownloadDisplayController* controller) {
    controller_ = controller;
  }

  void ResetState() {
    shown_ = false;
    detail_shown_ = false;
    icon_state_ = DownloadIconState::kComplete;
    is_active_ = false;
  }

  void Show() override { shown_ = true; }

  void Hide() override {
    shown_ = false;
    detail_shown_ = false;
  }

  bool IsShowing() override { return shown_; }

  void Enable() override { enabled_ = true; }

  void Disable() override { enabled_ = false; }

  void UpdateDownloadIcon(bool show_animation) override {
    icon_state_ = controller_->GetIconInfo().icon_state;
    is_active_ = controller_->GetIconInfo().is_active;
  }

  void ShowDetails() override { detail_shown_ = true; }
  void HideDetails() override { detail_shown_ = false; }
  bool IsShowingDetails() override { return detail_shown_; }
  bool IsFullscreenWithParentViewHidden() override { return is_fullscreen_; }

  DownloadIconState GetDownloadIconState() { return icon_state_; }
  bool IsActive() { return is_active_; }
  void SetIsFullscreen(bool is_fullscreen) { is_fullscreen_ = is_fullscreen; }

 private:
  bool shown_ = false;
  bool enabled_ = false;
  DownloadIconState icon_state_ = DownloadIconState::kComplete;
  bool is_active_ = false;
  bool detail_shown_ = false;
  bool is_fullscreen_ = false;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #constexpr-ctor-field-initializer
  RAW_PTR_EXCLUSION DownloadDisplayController* controller_ = nullptr;
};

class FakeDownloadBubbleUIController : public DownloadBubbleUIController {
 public:
  explicit FakeDownloadBubbleUIController(Browser* browser)
      : DownloadBubbleUIController(browser) {}
  ~FakeDownloadBubbleUIController() override = default;
  const OfflineItemList& GetOfflineItems() override { return offline_items_; }
  void InitOfflineItems(DownloadDisplayController* display_controller,
                        base::OnceCallback<void()> callback) override {
    std::move(callback).Run();
  }
  void AddOfflineItem(OfflineItem& item) { offline_items_.push_back(item); }
  void UpdateOfflineItem(int index, OfflineItemState state) {
    offline_items_[index].state = state;
  }

 protected:
  OfflineItemList offline_items_;
};

class MockDownloadCoreService : public DownloadCoreService {
 public:
  MOCK_METHOD(ChromeDownloadManagerDelegate*, GetDownloadManagerDelegate, ());
  MOCK_METHOD(DownloadUIController*, GetDownloadUIController, ());
  MOCK_METHOD(DownloadHistory*, GetDownloadHistory, ());
  MOCK_METHOD(extensions::ExtensionDownloadsEventRouter*,
              GetExtensionEventRouter,
              ());
  MOCK_METHOD(bool, HasCreatedDownloadManager, ());
  MOCK_METHOD(int, NonMaliciousDownloadCount, (), (const));
  MOCK_METHOD(void, CancelDownloads, ());
  MOCK_METHOD(void,
              SetDownloadManagerDelegateForTesting,
              (std::unique_ptr<ChromeDownloadManagerDelegate> delegate));
  MOCK_METHOD(bool, IsDownloadUiEnabled, ());
  MOCK_METHOD(bool, IsDownloadObservedByExtension, ());
};

std::unique_ptr<KeyedService> BuildMockDownloadCoreService(
    content::BrowserContext* browser_context) {
  return std::make_unique<MockDownloadCoreService>();
}

}  // namespace

class DownloadDisplayControllerTest : public testing::Test {
 public:
  DownloadDisplayControllerTest()
      : manager_(std::make_unique<NiceMock<content::MockDownloadManager>>()),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);
  }
  DownloadDisplayControllerTest(const DownloadDisplayControllerTest&) = delete;
  DownloadDisplayControllerTest& operator=(
      const DownloadDisplayControllerTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");
    EXPECT_CALL(*manager_.get(), GetBrowserContext())
        .WillRepeatedly(Return(profile_.get()));

    DownloadCoreServiceFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&BuildMockDownloadCoreService));
    mock_download_core_service_ = static_cast<MockDownloadCoreService*>(
        DownloadCoreServiceFactory::GetForBrowserContext(profile_));
    EXPECT_CALL(*mock_download_core_service(), IsDownloadUiEnabled())
        .WillRepeatedly(Return(true));
    delegate_ = std::make_unique<ChromeDownloadManagerDelegate>(profile_);
    EXPECT_CALL(*mock_download_core_service(), GetDownloadManagerDelegate())
        .WillRepeatedly(Return(delegate_.get()));

    display_ = std::make_unique<FakeDownloadDisplay>();
    window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile_, true);
    params.type = Browser::TYPE_NORMAL;
    params.window = window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));
    bubble_controller_ =
        std::make_unique<FakeDownloadBubbleUIController>(browser_.get());
    bubble_controller_->set_manager_for_testing(manager_.get());
    controller_ = std::make_unique<DownloadDisplayController>(
        display_.get(), browser_.get(), bubble_controller_.get());
    controller_->set_manager_for_testing(manager_.get());
    display_->SetController(controller_.get());
  }

  void TearDown() override {
    for (auto& item : items_) {
      item->RemoveObserver(&controller_->get_download_notifier_for_testing());
    }
    // The controller needs to be reset before download manager, because the
    // download_notifier_ will unregister itself from the manager.
    controller_.reset();
  }

  Browser* browser() { return browser_.get(); }

 protected:
  NiceMock<content::MockDownloadManager>& manager() { return *manager_.get(); }
  download::MockDownloadItem& item(size_t index) { return *items_[index]; }
  FakeDownloadDisplay& display() { return *display_; }
  DownloadDisplayController& controller() { return *controller_; }
  FakeDownloadBubbleUIController& bubble_controller() {
    return *bubble_controller_;
  }
  Profile* profile() { return profile_; }
  MockDownloadCoreService* mock_download_core_service() {
    return mock_download_core_service_;
  }

  void InitDownloadItem(const base::FilePath::CharType* path,
                        DownloadState state,
                        base::FilePath target_file_path =
                            base::FilePath(FILE_PATH_LITERAL("foo"))) {
    size_t index = items_.size();
    items_.push_back(std::make_unique<StrictMockDownloadItem>());
    EXPECT_CALL(item(index), GetId())
        .WillRepeatedly(Return(static_cast<uint32_t>(items_.size() + 1)));
    EXPECT_CALL(item(index), GetState()).WillRepeatedly(Return(state));
    EXPECT_CALL(item(index), IsPaused()).WillRepeatedly(Return(false));
    EXPECT_CALL(item(index), GetStartTime())
        .WillRepeatedly(Return(base::Time::Now()));
    EXPECT_CALL(item(index), GetDangerType())
        .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    EXPECT_CALL(item(index), IsDangerous()).WillRepeatedly(Return(false));
    int received_bytes =
        state == download::DownloadItem::IN_PROGRESS ? 50 : 100;
    EXPECT_CALL(item(index), GetReceivedBytes())
        .WillRepeatedly(Return(received_bytes));
    EXPECT_CALL(item(index), GetTotalBytes()).WillRepeatedly(Return(100));
    EXPECT_CALL(item(index), AllDataSaved())
        .WillRepeatedly(Return(
            state == download::DownloadItem::IN_PROGRESS ? false : true));
    EXPECT_CALL(item(index), IsDone()).WillRepeatedly(Return(false));
    EXPECT_CALL(item(index), IsTransient()).WillRepeatedly(Return(false));
    EXPECT_CALL(item(index), GetTargetFilePath())
        .WillRepeatedly(ReturnRefOfCopy(target_file_path));
    EXPECT_CALL(item(index), GetLastReason())
        .WillRepeatedly(Return(download::DOWNLOAD_INTERRUPT_REASON_NONE));
    EXPECT_CALL(item(index), GetInsecureDownloadStatus())
        .WillRepeatedly(
            Return(download::DownloadItem::InsecureDownloadStatus::SAFE));
    if (state == DownloadState::IN_PROGRESS) {
      in_progress_count_++;
    }
    EXPECT_CALL(manager(), InProgressCount())
        .WillRepeatedly(Return(in_progress_count_));
    // Set actioned_on to false (it defaults to true) because the controller
    // will generally set this to false in OnNewItem().
    DownloadItemModel(&item(index)).SetActionedOn(false);

    std::vector<download::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    EXPECT_CALL(*manager_.get(), GetAllDownloads(_))
        .WillRepeatedly(SetArgPointee<0>(items));
    item(index).AddObserver(&controller().get_download_notifier_for_testing());
    content::DownloadItemUtils::AttachInfoForTesting(&(item(index)), profile_,
                                                     nullptr);
    controller().OnNewItem(
        /*show_animation=*/false);
  }

  void InitOfflineItem(OfflineItemState state) {
    OfflineItem item;
    item.state = state;
    bubble_controller().AddOfflineItem(item);
    controller().OnNewItem(/*show_animation=*/false);
  }

  void UpdateOfflineItem(int item_index,
                         OfflineItemState state,
                         bool is_pending_deep_scanning) {
    if (state == OfflineItemState::COMPLETE) {
      bubble_controller().UpdateOfflineItem(item_index, state);
    }
    controller().OnUpdatedItem(state == OfflineItemState::COMPLETE,
                               is_pending_deep_scanning,
                               /*may_show_details=*/true);
  }

  void UpdateDownloadItem(int item_index,
                          DownloadState state,
                          download::DownloadDangerType danger_type =
                              download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                          bool may_show_details = true) {
    DCHECK_GT(items_.size(), static_cast<size_t>(item_index));

    EXPECT_CALL(item(item_index), GetState()).WillRepeatedly(Return(state));
    EXPECT_CALL(item(item_index), GetDangerType())
        .WillRepeatedly(Return(danger_type));
    if (state == DownloadState::COMPLETE) {
      EXPECT_CALL(item(item_index), IsDone()).WillRepeatedly(Return(true));
      in_progress_count_--;
      EXPECT_CALL(manager(), InProgressCount())
          .WillRepeatedly(Return(in_progress_count_));
      DownloadPrefs::FromDownloadManager(&manager())
          ->SetLastCompleteTime(base::Time::Now());
    } else {
      EXPECT_CALL(item(item_index), IsDone()).WillRepeatedly(Return(false));
    }
    controller().OnUpdatedItem(
        state == DownloadState::COMPLETE,
        danger_type == download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING,
        may_show_details);
  }

  void OnRemovedItem(const ContentId& id) { controller().OnRemovedItem(id); }

  void RemoveLastDownload() {
    items_.pop_back();
    std::vector<download::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    EXPECT_CALL(*manager_.get(), GetAllDownloads(_))
        .WillRepeatedly(SetArgPointee<0>(items));
  }

  bool VerifyDisplayState(bool shown,
                          bool detail_shown,
                          DownloadIconState icon_state,
                          bool is_active) {
    bool success = true;
    if (shown != display().IsShowing()) {
      success = false;
      ADD_FAILURE() << "Display should have shown state " << shown
                    << ", but found " << display().IsShowing();
    }
    if (detail_shown != display().IsShowingDetails()) {
      success = false;
      ADD_FAILURE() << "Display should have detailed shown state "
                    << detail_shown << ", but found "
                    << display().IsShowingDetails();
    }
    if (icon_state != display().GetDownloadIconState()) {
      success = false;
      ADD_FAILURE() << "Display should have detailed icon state "
                    << static_cast<int>(icon_state) << ", but found "
                    << static_cast<int>(display().GetDownloadIconState());
    }
    if (is_active != display().IsActive()) {
      success = false;
      ADD_FAILURE() << "Display should have is_active set to " << is_active
                    << ", but found " << display().IsActive();
    }
    return success;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  int in_progress_count_ = 0;

  std::unique_ptr<DownloadDisplayController> controller_;
  std::unique_ptr<FakeDownloadDisplay> display_;
  std::vector<std::unique_ptr<StrictMockDownloadItem>> items_;

  std::unique_ptr<NiceMock<content::MockDownloadManager>> manager_;
  std::unique_ptr<FakeDownloadBubbleUIController> bubble_controller_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<Profile> profile_;
  std::unique_ptr<TestBrowserWindow> window_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<MockDownloadCoreService> mock_download_core_service_;
  std::unique_ptr<ChromeDownloadManagerDelegate> delegate_;
};

TEST_F(DownloadDisplayControllerTest, GetProgressItemsInProgress) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar4.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  DownloadDisplayController::ProgressInfo progress = controller().GetProgress();

  EXPECT_EQ(progress.download_count, 2);
  EXPECT_EQ(progress.progress_percentage, 50);
}

TEST_F(DownloadDisplayControllerTest, OfflineItemsUncertainProgress) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar4.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  // This offline item has uncertain progress
  InitOfflineItem(OfflineItemState::IN_PROGRESS);
  DownloadDisplayController::ProgressInfo progress = controller().GetProgress();

  EXPECT_EQ(progress.download_count, 3);
  EXPECT_EQ(progress.progress_percentage, 50);
  EXPECT_FALSE(progress.progress_certain);
}

TEST_F(DownloadDisplayControllerTest, GetProgressItemsAllComplete) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::COMPLETE);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE);
  DownloadDisplayController::ProgressInfo progress = controller().GetProgress();

  EXPECT_EQ(progress.download_count, 0);
  EXPECT_EQ(progress.progress_percentage, 0);
}

TEST_F(DownloadDisplayControllerTest, UpdateToolbarButtonState) {
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  // No details are shown on download initiation.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  // No details are shown on download initiation.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  // Pause download 0.
  EXPECT_CALL(item(0), IsPaused()).WillRepeatedly(Return(true));
  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));
  // Pause download 1.
  EXPECT_CALL(item(1), IsPaused()).WillRepeatedly(Return(true));
  UpdateDownloadItem(/*item_index=*/1, DownloadState::IN_PROGRESS);
  // The download display is not active anymore, because all in progress
  // downloads are paused. Details are not shown because the updated download
  // is not done.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/false));
  // Unpause download 0.
  EXPECT_CALL(item(0), IsPaused()).WillRepeatedly(Return(false));
  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));
  // Complete download 0.
  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
  // Details are shown because the only in-progress download is still paused.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/false));

  // Complete download 1.
  UpdateDownloadItem(/*item_index=*/1, DownloadState::COMPLETE);
  // Now details are shown because all downloads are complete.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/true));

  task_environment_.FastForwardBy(base::Minutes(1));
  // The display is still showing but the state has changed to inactive.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  task_environment_.FastForwardBy(base::Hours(23));
  // The display is still showing because the last download is less than 1
  // day ago.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  task_environment_.FastForwardBy(base::Hours(1));
  // The display should stop showing once the last download is more than 1
  // day ago.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));
}

TEST_F(DownloadDisplayControllerTest,
       UpdateToolbarButtonState_MultipleDownloads) {
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
  // The download icon state is still kProgress because not all downloads are
  // completed. details_shown is still false, because the details are only
  // popped up when all in-progress downloads are complete.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  UpdateDownloadItem(/*item_index=*/1, DownloadState::COMPLETE);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/true));

  // Reset details_shown while the downloads are in progress. This can happen if
  // the user clicks somewhere else to dismiss the download bubble.
  display().HideDetails();

  InitOfflineItem(OfflineItemState::IN_PROGRESS);
  // Do not show details because the offline item is not complete.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  UpdateOfflineItem(/*item_index=*/0, OfflineItemState::COMPLETE,
                    /*is_pending_deep_scanning=*/false);
  // Details are shown because all items are complete.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/true));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar3.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  UpdateDownloadItem(/*item_index=*/2, DownloadState::COMPLETE,
                     download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  // Pop up the partial view because all downloads are complete.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/true));
}

TEST_F(DownloadDisplayControllerTest,
       UpdateToolbarButtonState_OnCompleteItemCreated) {
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::COMPLETE);
  // Don't show the button if the new download is already completed.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));
}

TEST_F(DownloadDisplayControllerTest, UpdateToolbarButtonState_DeepScanning) {
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS,
                     download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING);
  // Details are shown because the scan is pending.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  // Reset details_shown while the downloads are in progress. This can happen if
  // the user clicks somewhere else to dismiss the download bubble.
  display().HideDetails();

  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS,
                     download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING);
  // Details are not shown because the scan is ongoing.
  EXPECT_TRUE(
      VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                         /*icon_state=*/DownloadIconState::kDeepScanning,
                         /*is_active=*/true));

  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
  // Details are shown because all downloads are now complete.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/true));
}

TEST_F(DownloadDisplayControllerTest, UpdateToolbarButtonState_EmptyFilePath) {
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS,
                   /*target_file_path=*/base::FilePath(FILE_PATH_LITERAL("")));
  // Empty file path should not be reflected in the UI.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  EXPECT_CALL(item(0), GetTargetFilePath())
      .WillRepeatedly(
          ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("bar.pdf"))));
  controller().OnNewItem(/*show_animation=*/false);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));
}

TEST_F(DownloadDisplayControllerTest,
       UpdateToolbarButtonState_DangerousDownload) {
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  EXPECT_CALL(item(0), IsDangerous()).WillRepeatedly(Return(true));
  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS,
                     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST);
  // Details are not shown for most dangerous reasons.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  // Downloads prompted for deep scanning should be considered in progress and
  // should display details.
  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS,
                     download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));
}

TEST_F(DownloadDisplayControllerTest, UpdateToolbarButtonState_OnRemovedItem) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  std::string same_id = "Download 1";
  std::string different_id = "Download 2";
  EXPECT_CALL(item(0), GetGuid()).WillRepeatedly(ReturnRef(same_id));

  OnRemovedItem(ContentId("LEGACY_DOWNLOAD", different_id));
  // The download display is still shown, because the removed download is
  // different. Details are not shown because there is still a download in
  // progress.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  OnRemovedItem(ContentId("LEGACY_DOWNLOAD", same_id));
  // The download display is hided, because the only item in the download list
  // is about to be removed.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));
}

TEST_F(DownloadDisplayControllerTest,
       UpdateToolbarButtonState_OnRemovedItemMultipleDownloads) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar1.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  std::vector<std::string> ids = {"Download 1", "Download 2"};
  EXPECT_CALL(item(0), GetGuid()).WillRepeatedly(ReturnRef(ids[0]));
  EXPECT_CALL(item(1), GetGuid()).WillRepeatedly(ReturnRef(ids[1]));

  // The download display is still shown, because there are multiple downloads
  // in the list. Details are not shown because there is still a download in
  // progress.
  OnRemovedItem(ContentId("LEGACY_DOWNLOAD", ids[0]));
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  RemoveLastDownload();
  OnRemovedItem(ContentId("LEGACY_DOWNLOAD", ids[0]));
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));
}

TEST_F(DownloadDisplayControllerTest,
       UpdateToolbarButtonState_DownloadWasActionedOn) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
  // Details are shown because the last in-progress download has completed.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/true));

  DownloadItemModel(&item(0)).SetActionedOn(true);
  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));
}

TEST_F(DownloadDisplayControllerTest, UpdateToolbarButtonState_OnResume) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  EXPECT_CALL(item(0), IsPaused()).WillRepeatedly(Return(true));
  controller().OnResume();
  // is_active state should be updated after OnResume is called.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/false));
}

TEST_F(DownloadDisplayControllerTest,
       UpdateToolbarButtonState_DontShowDetailsIfNotAllowed) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE,
                     download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                     /*may_show_details=*/false);
  // Details are not shown because may_show_details is false.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/true));
}

TEST_F(DownloadDisplayControllerTest, InitialState_OldLastDownload) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::COMPLETE);
  base::Time current_time = base::Time::Now();
  // Set the last complete time to more than 1 day ago.
  DownloadPrefs::FromDownloadManager(&manager())
      ->SetLastCompleteTime(current_time - base::Hours(25));

  DownloadDisplayController controller(&display(), browser(),
                                       &bubble_controller());
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));
}

TEST_F(DownloadDisplayControllerTest, InitialState_NewLastDownload) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::COMPLETE);
  base::Time current_time = base::Time::Now();
  // Set the last complete time to less than 1 day ago.
  DownloadPrefs::FromDownloadManager(&manager())
      ->SetLastCompleteTime(current_time - base::Hours(23));

  DownloadDisplayController controller(&display(), browser(),
                                       &bubble_controller());
  // The initial state should not display details.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  // The display should stop showing once the last download is more than 1 day
  // ago.
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));
}

TEST_F(DownloadDisplayControllerTest, InitialState_InProgressDownload) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);

  // Simulate a new window opened.
  display().ResetState();
  DownloadDisplayController controller(&display(), browser(),
                                       &bubble_controller());
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));
}

TEST_F(DownloadDisplayControllerTest,
       InitialState_NewLastDownloadWithEmptyItem) {
  base::Time current_time = base::Time::Now();
  // Set the last complete time to less than 1 day ago.
  DownloadPrefs::FromDownloadManager(&manager())
      ->SetLastCompleteTime(current_time - base::Hours(23));

  DownloadDisplayController controller(&display(), browser(),
                                       &bubble_controller());
  // Although the last complete time is set, the download display is not shown
  // because the download item list is empty. This can happen if the download
  // history is deleted by the user.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));
}

TEST_F(DownloadDisplayControllerTest, InitialState_NoLastDownload) {
  DownloadDisplayController controller(&display(), browser(),
                                       &bubble_controller());
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));
}

TEST_F(DownloadDisplayControllerTest, OnButtonPressed_IconStateComplete) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/true));

  controller().HandleButtonPressed();

  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));
}

TEST_F(DownloadDisplayControllerTest, OnButtonPressed_IconStateInProgress) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  controller().OnButtonPressed();

  // Keep is_active to true because the download is still in progress.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));
}

TEST_F(DownloadDisplayControllerTest,
       Fullscreen_DoesNotShowDetailsForInProgressOnExitFullscreen) {
  display().SetIsFullscreen(true);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  // Do not show bubble for in-progress download in full screen mode.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  display().SetIsFullscreen(false);
  controller().OnFullscreenStateChanged();
  // Do not show bubble for in-progress download when exiting full screen mode.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));
}

TEST_F(DownloadDisplayControllerTest,
       Fullscreen_ShowsIconAndDetailsForCompletedOnExitFullscreen) {
  display().SetIsFullscreen(true);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE,
                     download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  // While the bubble does not pop up, and the toolbar not shown, the icon
  // state is still updated. So |is_active| should be true for one minute
  // after completed download.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/true));

  task_environment_.FastForwardBy(base::Minutes(1));
  // The display is still showing but the state has changed to inactive.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  display().SetIsFullscreen(false);
  controller().OnFullscreenStateChanged();
  // On exiting full screen, show download icon as active for 1 minute and show
  // details, as they were missed while in fullscreen.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/true));
  display().HideDetails();

  task_environment_.FastForwardBy(base::Minutes(1));
  // The display is still showing but the state has changed to inactive.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));
}

TEST_F(DownloadDisplayControllerTest,
       ShowsDetailsWhenExtensionObservingDownloads) {
  EXPECT_CALL(*mock_download_core_service(), IsDownloadObservedByExtension())
      .WillRepeatedly(Return(true));
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));
  UpdateDownloadItem(0, download::DownloadItem::COMPLETE);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/true));
}
