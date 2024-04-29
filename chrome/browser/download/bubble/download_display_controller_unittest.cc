// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_display_controller.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/browser/download/bubble/download_bubble_display_info.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_bubble_utils.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/download/download_display.h"
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

namespace {
using ::offline_items_collection::OfflineItem;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using ::testing::StrictMock;
using StrictMockDownloadItem = StrictMock<download::MockDownloadItem>;
using DownloadIconActive = DownloadDisplay::IconActive;
using DownloadIconState = DownloadDisplay::IconState;
using DownloadState = download::DownloadItem::DownloadState;
using DownloadUIModelPtr = DownloadUIModel::DownloadUIModelPtr;
using OfflineItemList =
    offline_items_collection::OfflineContentProvider::OfflineItemList;
using OfflineItemState = offline_items_collection::OfflineItemState;

class FakeDownloadDisplay : public DownloadDisplay {
 public:
  FakeDownloadDisplay() = default;
  FakeDownloadDisplay(const FakeDownloadDisplay&) = delete;
  FakeDownloadDisplay& operator=(const FakeDownloadDisplay&) = delete;

  void ResetState() {
    shown_ = false;
    detail_shown_ = false;
    state_ = DownloadIconState::kComplete;
    active_ = DownloadIconActive::kInactive;
    announcement_count_ = 0;
  }

  void Show() override { shown_ = true; }

  void Hide() override {
    shown_ = false;
    detail_shown_ = false;
  }

  bool IsShowing() const override { return shown_; }

  void Enable() override { enabled_ = true; }

  void Disable() override { enabled_ = false; }

  void UpdateDownloadIcon(const IconUpdateInfo& updates) override {
    if (updates.new_state) {
      state_ = *updates.new_state;
    }
    if (updates.new_active) {
      active_ = *updates.new_active;
    }
    if (updates.new_progress) {
      progress_info_ = *updates.new_progress;
    }
  }

  void ShowDetails() override { detail_shown_ = true; }
  void HideDetails() override { detail_shown_ = false; }
  bool IsShowingDetails() const override { return detail_shown_; }
  void AnnounceAccessibleAlertNow(const std::u16string& alert_text) override {
    ++announcement_count_;
  }
  bool OpenMostSpecificDialog(
      const offline_items_collection::ContentId& content_id) override {
    detail_shown_ = true;
    return true;
  }
  bool IsFullscreenWithParentViewHidden() const override {
    return is_fullscreen_;
  }
  bool ShouldShowExclusiveAccessBubble() const override {
    return IsFullscreenWithParentViewHidden() &&
           should_show_exclusive_access_bubble_;
  }

  DownloadIconState GetIconState() const override { return state_; }
  DownloadIconActive GetIconActive() const { return active_; }
  ProgressInfo GetIconProgress() const { return progress_info_; }
  void SetIsFullscreen(bool is_fullscreen) { is_fullscreen_ = is_fullscreen; }
  void SetShouldShowExclusiveAccessBubble(bool show) {
    should_show_exclusive_access_bubble_ = show;
  }
  int GetAnnouncementCount() const { return announcement_count_; }
  void OpenSecuritySubpage(
      const offline_items_collection::ContentId&) override {}

 private:
  bool shown_ = false;
  bool enabled_ = false;
  DownloadIconState state_ = DownloadIconState::kComplete;
  DownloadIconActive active_ = DownloadIconActive::kInactive;
  ProgressInfo progress_info_;
  bool detail_shown_ = false;
  bool is_fullscreen_ = false;
  bool should_show_exclusive_access_bubble_ = true;
  int announcement_count_;
};

// TODO(chlily): Pull this and the very similar class in
// DownloadBubbleUIControllerTest out into a test utils file.
class MockDownloadBubbleUpdateService : public DownloadBubbleUpdateService {
 public:
  enum class ModelType {
    kDownloadItem,
    kOfflineItem,
  };

  MockDownloadBubbleUpdateService(
      Profile* profile,
      const std::vector<std::unique_ptr<StrictMockDownloadItem>>&
          download_items,
      const OfflineItemList& offline_items)
      : DownloadBubbleUpdateService(profile),
        profile_(profile),
        download_items_(download_items),
        offline_items_(offline_items) {}
  MockDownloadBubbleUpdateService(const MockDownloadBubbleUpdateService&) =
      delete;
  MockDownloadBubbleUpdateService& operator=(
      const MockDownloadBubbleUpdateService&) = delete;

  ~MockDownloadBubbleUpdateService() override = default;

  void UpdateInfoForModel(const DownloadUIModel& model,
                          DownloadBubbleDisplayInfo& info) {
    ++info.all_models_size;
    info.last_completed_time =
        std::max(info.last_completed_time, model.GetEndTime());
    if (model.GetDangerType() ==
            download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING &&
        model.GetState() != download::DownloadItem::CANCELLED) {
      info.has_deep_scanning = true;
    }
    if (!model.WasActionedOn()) {
      info.has_unactioned = true;
    }
    if (IsModelInProgress(&model)) {
      ++info.in_progress_count;
      if (model.IsPaused()) {
        ++info.paused_count;
      }
    }
  }

  const DownloadBubbleDisplayInfo& GetDisplayInfo(
      const webapps::AppId* web_app_id) override {
    info_ = DownloadBubbleDisplayInfo{};
    int download_item_index = 0, offline_item_index = 0;
    // Compose a list of models from the items stored in the test fixture.
    for (ModelType type : model_types_) {
      if (type == ModelType::kDownloadItem) {
        auto model = DownloadItemModel::Wrap(
            download_items_->at(download_item_index++).get());
        if (!model->ShouldShowInBubble()) {
          continue;
        }
        UpdateInfoForModel(*model, info_);
      } else {
        auto model = OfflineItemModel::Wrap(
            OfflineItemModelManagerFactory::GetForBrowserContext(profile_),
            offline_items_->at(offline_item_index++));
        if (!model->ShouldShowInBubble()) {
          continue;
        }
        UpdateInfoForModel(*model, info_);
      }
    }
    return info_;
  }

  std::vector<std::u16string> TakeAccessibleAlertsForAnnouncement(
      const webapps::AppId* web_app_id) override {
    std::vector<std::u16string> alerts;
    alerts.swap(accessible_alerts_);
    return alerts;
  }

  void AddModel(ModelType type) {
    model_types_.push_back(type);
    AddAlert();
  }

  void AddAlert() {
    // Add an arbitrary accessible alert. These tests do not care about the
    // alert content.
    accessible_alerts_.push_back(u"alert");
  }

  void RemoveLastDownload() {
    for (auto reverse_it = model_types_.rbegin();
         reverse_it != model_types_.rend(); ++reverse_it) {
      if (*reverse_it == ModelType::kDownloadItem) {
        model_types_.erase(std::next(reverse_it).base());
        break;
      }
    }
  }

  bool IsInitialized() const override { return true; }

  MOCK_METHOD(DownloadDisplay::ProgressInfo,
              GetProgressInfo,
              (const webapps::AppId*),
              (const override));

 private:
  raw_ptr<Profile> profile_;
  DownloadBubbleDisplayInfo info_;
  std::vector<ModelType> model_types_;
  const raw_ref<const std::vector<std::unique_ptr<StrictMockDownloadItem>>>
      download_items_;
  const raw_ref<const OfflineItemList> offline_items_;
  std::vector<std::u16string> accessible_alerts_;
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
  MOCK_METHOD(int, BlockingShutdownCount, (), (const));
  MOCK_METHOD(void,
              CancelDownloads,
              (DownloadCoreService::CancelDownloadsTrigger));
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

class DownloadDisplayControllerTest : public testing::Test {
 public:
  DownloadDisplayControllerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);
  }
  DownloadDisplayControllerTest(const DownloadDisplayControllerTest&) = delete;
  DownloadDisplayControllerTest& operator=(
      const DownloadDisplayControllerTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");

    DownloadCoreServiceFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&BuildMockDownloadCoreService));
    mock_download_core_service_ = static_cast<MockDownloadCoreService*>(
        DownloadCoreServiceFactory::GetForBrowserContext(profile_));
    EXPECT_CALL(*mock_download_core_service(), IsDownloadUiEnabled())
        .WillRepeatedly(Return(true));
    delegate_ = std::make_unique<ChromeDownloadManagerDelegate>(profile_);
    EXPECT_CALL(*mock_download_core_service(), GetDownloadManagerDelegate())
        .WillRepeatedly(Return(delegate_.get()));

    mock_update_service_ =
        std::make_unique<StrictMock<MockDownloadBubbleUpdateService>>(
            profile_, items_, offline_items_);
    // Will be called when the DownloadDisplayController is constructed.
    EXPECT_CALL(*mock_update_service_, GetProgressInfo(_))
        .WillRepeatedly(Return(DownloadDisplay::ProgressInfo()));
    display_ = std::make_unique<FakeDownloadDisplay>();
    window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile_, true);
    params.type = Browser::TYPE_NORMAL;
    params.window = window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));
    bubble_controller_ = std::make_unique<DownloadBubbleUIController>(
        browser_.get(), mock_update_service_.get());
    controller_ = std::make_unique<DownloadDisplayController>(
        display_.get(), browser_.get(), bubble_controller_.get());
  }

  void TearDown() override {
    // The controller needs to be reset before download manager, because the
    // download_notifier_ will unregister itself from the manager.
    controller_.reset();
    mock_update_service_.reset();
  }

  Browser* browser() { return browser_.get(); }

 protected:
  download::MockDownloadItem& item(size_t index) { return *items_[index]; }
  FakeDownloadDisplay& display() { return *display_; }
  DownloadDisplayController& controller() { return *controller_; }
  DownloadBubbleUIController& bubble_controller() {
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
    EXPECT_CALL(item(index), GetEndTime()).WillRepeatedly(Return(base::Time()));
    EXPECT_CALL(item(index), GetDangerType())
        .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    EXPECT_CALL(item(index), IsDangerous()).WillRepeatedly(Return(false));
    EXPECT_CALL(item(index), IsInsecure()).WillRepeatedly(Return(false));
    int received_bytes =
        state == download::DownloadItem::IN_PROGRESS ? 50 : 100;
    EXPECT_CALL(item(index), GetReceivedBytes())
        .WillRepeatedly(Return(received_bytes));
    EXPECT_CALL(item(index), GetTotalBytes()).WillRepeatedly(Return(100));
    EXPECT_CALL(item(index), AllDataSaved())
        .WillRepeatedly(Return(state != download::DownloadItem::IN_PROGRESS));
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
    // Set actioned_on to false (it defaults to true) because the controller
    // will generally set this to false in OnNewItem().
    DownloadItemModel(&item(index)).SetActionedOn(false);

    std::vector<download::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    content::DownloadItemUtils::AttachInfoForTesting(&(item(index)), profile_,
                                                     nullptr);
    mock_update_service_->AddModel(
        MockDownloadBubbleUpdateService::ModelType::kDownloadItem);
    DownloadDisplay::ProgressInfo progress_info;
    progress_info.download_count = in_progress_count_;
    progress_info.progress_percentage = in_progress_count_ > 0 ? 50 : 0;
    EXPECT_CALL(*mock_update_service_, GetProgressInfo(_))
        .WillRepeatedly(Return(progress_info));
    controller().OnNewItem(/*show_animation=*/false);
  }

  void InitOfflineItem(OfflineItemState state) {
    OfflineItem item;
    item.state = state;
    offline_items_.push_back(item);
    if (state == OfflineItemState::IN_PROGRESS) {
      ++in_progress_count_;
    }
    DownloadDisplay::ProgressInfo progress_info;
    progress_info.download_count = in_progress_count_;
    progress_info.progress_percentage = in_progress_count_ > 0 ? 50 : 0;
    progress_info.progress_certain = false;
    EXPECT_CALL(*mock_update_service_, GetProgressInfo(_))
        .WillRepeatedly(Return(progress_info));
    mock_update_service_->AddModel(
        MockDownloadBubbleUpdateService::ModelType::kOfflineItem);
    controller().OnNewItem(/*show_animation=*/false);
  }

  void UpdateOfflineItem(int item_index, OfflineItemState state) {
    offline_items_[item_index].state = state;
    mock_update_service_->AddAlert();
    controller().OnUpdatedItem(state == OfflineItemState::COMPLETE,
                               /*may_show_details=*/true);
  }

  void UpdateDownloadItem(int item_index,
                          DownloadState state,
                          download::DownloadDangerType danger_type =
                              download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                          bool may_show_details = true,
                          bool is_insecure = false) {
    DCHECK_GT(items_.size(), static_cast<size_t>(item_index));

    // In-progress but dangerous downloads are considered complete.
    // TODO(crbug.com/40264271): Don't duplicate this logic.
    bool in_progress_dangerous =
        (state == DownloadState::IN_PROGRESS &&
         danger_type != download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
    EXPECT_CALL(item(item_index), GetState()).WillRepeatedly(Return(state));
    EXPECT_CALL(item(item_index), GetDangerType())
        .WillRepeatedly(Return(danger_type));
    EXPECT_CALL(item(item_index), IsInsecure())
        .WillRepeatedly(Return(is_insecure));
    if (state == DownloadState::COMPLETE) {
      EXPECT_CALL(item(item_index), IsDone()).WillRepeatedly(Return(true));
      EXPECT_CALL(item(item_index), GetEndTime())
          .WillRepeatedly(Return(base::Time::Now()));
    } else {
      EXPECT_CALL(item(item_index), IsDone()).WillRepeatedly(Return(false));
    }
    if (state == DownloadState::COMPLETE || in_progress_dangerous) {
      in_progress_count_--;
    }
    mock_update_service_->AddAlert();
    controller().OnUpdatedItem(
        state == DownloadState::COMPLETE || in_progress_dangerous,
        may_show_details);
  }

  void OnRemovedItem(const std::string& id) {
    controller().OnRemovedItem(ContentId{"LEGACY_DOWNLOAD", id});
  }

  void RemoveLastDownload() {
    items_.pop_back();
    std::vector<download::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    mock_update_service_->RemoveLastDownload();
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
    if (icon_state != display().GetIconState()) {
      success = false;
      ADD_FAILURE() << "Display should have detailed icon state "
                    << static_cast<int>(icon_state) << ", but found "
                    << static_cast<int>(display().GetIconState());
    }
    if (is_active !=
        (display().GetIconActive() == DownloadIconActive::kActive)) {
      success = false;
      ADD_FAILURE() << "Display should have is_active set to " << is_active
                    << ", but found "
                    << (display().GetIconActive() ==
                        DownloadIconActive::kActive);
    }
    return success;
  }

  // Simulates an update to make the download display update.
  void TriggerIconUpdate() {
    controller().OnUpdatedItem(/*is_done=*/true, /*may_show_details=*/true);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  int in_progress_count_ = 0;

  std::unique_ptr<DownloadDisplayController> controller_;
  std::unique_ptr<FakeDownloadDisplay> display_;
  std::vector<std::unique_ptr<StrictMockDownloadItem>> items_;
  std::vector<OfflineItem> offline_items_;
  std::unique_ptr<StrictMock<MockDownloadBubbleUpdateService>>
      mock_update_service_;
  std::unique_ptr<DownloadBubbleUIController> bubble_controller_;
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
  TriggerIconUpdate();

  EXPECT_EQ(display().GetAnnouncementCount(), 3);
  EXPECT_EQ(display().GetIconProgress().download_count, 2);
  EXPECT_EQ(display().GetIconProgress().progress_percentage, 50);
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
  TriggerIconUpdate();

  EXPECT_EQ(display().GetAnnouncementCount(), 4);
  EXPECT_EQ(display().GetIconProgress().download_count, 3);
  EXPECT_EQ(display().GetIconProgress().progress_percentage, 50);
  EXPECT_FALSE(display().GetIconProgress().progress_certain);
}

TEST_F(DownloadDisplayControllerTest, GetProgressItemsAllComplete) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::COMPLETE);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE);
  TriggerIconUpdate();

  EXPECT_EQ(display().GetAnnouncementCount(), 2);
  EXPECT_EQ(display().GetIconProgress().download_count, 0);
  EXPECT_EQ(display().GetIconProgress().progress_percentage, 0);
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

  task_environment_.FastForwardBy(base::Minutes(58));
  // The display is still showing because the last download is less than 1
  // hour ago.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  task_environment_.FastForwardBy(base::Minutes(2));
  // The display should stop showing once the last download is more than 1
  // hour ago.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  EXPECT_EQ(display().GetAnnouncementCount(), 7);
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

  UpdateOfflineItem(/*item_index=*/0, OfflineItemState::COMPLETE);
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

  EXPECT_EQ(display().GetAnnouncementCount(), 8);
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

  EXPECT_EQ(display().GetAnnouncementCount(), 1);
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
  // Details are shown because the pending deep scan download is considered
  // complete.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

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

  EXPECT_EQ(display().GetAnnouncementCount(), 4);
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
  // Add an alert manually because updating the EXPECT_CALL doesn't by itself
  // add an alert, even though in production such an update would.
  mock_update_service_->AddAlert();
  controller().OnNewItem(/*show_animation=*/false);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  EXPECT_EQ(display().GetAnnouncementCount(), 2);
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
  // Dangerous downloads should be considered completed and
  // should display details if there are no other in-progress downloads.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS,
                     download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  EXPECT_EQ(display().GetAnnouncementCount(), 3);
}

TEST_F(DownloadDisplayControllerTest,
       UpdateToolbarButtonState_InsecureDownload) {
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS,
                     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST,
                     /*may_show_details=*/true, /*is_insecure=*/true);
  // Insecure downloads should be considered completed and
  // should display details if there are no other in-progress downloads.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  EXPECT_EQ(display().GetAnnouncementCount(), 2);
}

TEST_F(DownloadDisplayControllerTest, UpdateToolbarButtonState_OnRemovedItem) {
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
  RemoveLastDownload();
  OnRemovedItem(ids[1]);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  // Display is hidden because the last download was deleted.
  RemoveLastDownload();
  OnRemovedItem(ids[0]);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  EXPECT_EQ(display().GetAnnouncementCount(), 2);
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

  EXPECT_EQ(display().GetAnnouncementCount(), 3);
}

TEST_F(DownloadDisplayControllerTest, UpdateToolbarButtonState_OnResume) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  EXPECT_CALL(item(0), IsPaused()).WillRepeatedly(Return(true));
  // Add an alert manually because updating the EXPECT_CALL doesn't by itself
  // add an alert, even though in production such an update would.
  mock_update_service_->AddAlert();
  controller().OnResume();
  // is_active state should be updated after OnResume is called.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/false));

  EXPECT_EQ(display().GetAnnouncementCount(), 2);
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

  EXPECT_EQ(display().GetAnnouncementCount(), 2);
}

TEST_F(DownloadDisplayControllerTest, InitialState_InProgressDownload) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);

  EXPECT_EQ(display().GetAnnouncementCount(), 1);

  // Simulate a new window opened.
  display().ResetState();
  DownloadDisplayController controller(&display(), browser(),
                                       &bubble_controller());
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress,
                                 /*is_active=*/true));

  EXPECT_EQ(display().GetAnnouncementCount(), 0);
}

TEST_F(DownloadDisplayControllerTest, InitialState_NoLastDownload) {
  DownloadDisplayController controller(&display(), browser(),
                                       &bubble_controller());
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  EXPECT_EQ(display().GetAnnouncementCount(), 0);
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

  EXPECT_EQ(display().GetAnnouncementCount(), 2);
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

  EXPECT_EQ(display().GetAnnouncementCount(), 1);
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

  // Nothing is announced for a fullscreen transition. There is only 1
  // announcement from InitDownloadItem.
  EXPECT_EQ(display().GetAnnouncementCount(), 1);
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

  EXPECT_EQ(display().GetAnnouncementCount(), 2);
}

// Test the path where the exclusive access bubble should not be shown (e.g. in
// kiosk mode or in immersive fullscreen).
TEST_F(DownloadDisplayControllerTest,
       Fullscreen_ShouldNotShowExclusiveAccessBubble) {
  display().SetIsFullscreen(true);
  display().SetShouldShowExclusiveAccessBubble(false);
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
  // On exiting full screen, the details were not shown.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete,
                                 /*is_active=*/false));

  EXPECT_EQ(display().GetAnnouncementCount(), 2);
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

  EXPECT_EQ(display().GetAnnouncementCount(), 2);
}

}  // namespace
