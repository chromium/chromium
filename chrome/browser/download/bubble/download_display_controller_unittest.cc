// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/bubble/download_display.h"
#include "chrome/browser/download/bubble/download_icon_state.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

namespace {
using StrictMockDownloadItem = testing::StrictMock<download::MockDownloadItem>;
using DownloadIconState = download::DownloadIconState;
using DownloadState = download::DownloadItem::DownloadState;

class FakeDownloadDisplay : public DownloadDisplay {
 public:
  FakeDownloadDisplay() = default;
  FakeDownloadDisplay(const FakeDownloadDisplay&) = delete;
  FakeDownloadDisplay& operator=(const FakeDownloadDisplay&) = delete;

  void Show() override { shown_ = true; }

  void Hide() override {
    shown_ = false;
    detail_shown_ = false;
  }

  bool IsShowing() override { return shown_; }

  void Enable() override { enabled_ = true; }

  void Disable() override { enabled_ = false; }

  void UpdateDownloadIcon(DownloadIconState state) override {
    icon_state_ = state;
  }

  void ShowDetails() override { detail_shown_ = true; }

  bool IsDetailsShown() { return detail_shown_; }
  void SetDetailsShown(bool detail_shown) { detail_shown_ = detail_shown; }

  DownloadIconState GetDownloadIconState() { return icon_state_; }

 private:
  bool shown_ = false;
  bool enabled_ = false;
  DownloadIconState icon_state_ = DownloadIconState::kComplete;
  bool detail_shown_ = false;
};

}  // namespace

class DownloadDisplayControllerTest : public testing::Test {
 public:
  DownloadDisplayControllerTest()
      : manager_(std::make_unique<NiceMock<content::MockDownloadManager>>()),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  DownloadDisplayControllerTest(const DownloadDisplayControllerTest&) = delete;
  DownloadDisplayControllerTest& operator=(
      const DownloadDisplayControllerTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    Profile* profile =
        testing_profile_manager_.CreateTestingProfile("testing_profile");
    EXPECT_CALL(*manager_.get(), GetBrowserContext())
        .WillRepeatedly(Return(profile));

    // Set test delegate to get the corresponding download prefs.
    auto delegate = std::make_unique<ChromeDownloadManagerDelegate>(profile);
    DownloadCoreServiceFactory::GetForBrowserContext(profile)
        ->SetDownloadManagerDelegateForTesting(std::move(delegate));

    display_ = std::make_unique<FakeDownloadDisplay>();
    controller_ = std::make_unique<DownloadDisplayController>(display_.get(),
                                                              manager_.get());
  }

  void TearDown() override {
    for (auto& item : items_) {
      item->RemoveObserver(&controller_->get_download_notifier_for_testing());
    }
    // The controller needs to be reset before download manager, because the
    // download_notifier_ will unregister itself from the manager.
    controller_.reset();
  }

 protected:
  NiceMock<content::MockDownloadManager>& manager() { return *manager_.get(); }
  download::MockDownloadItem& item(size_t index) { return *items_[index]; }
  FakeDownloadDisplay& display() { return *display_; }
  DownloadDisplayController& controller() { return *controller_; }

  void InitDownloadItem(const base::FilePath::CharType* path,
                        DownloadState state) {
    size_t index = items_.size();
    items_.push_back(std::make_unique<StrictMockDownloadItem>());
    EXPECT_CALL(item(index), GetId())
        .WillRepeatedly(Return(static_cast<uint32_t>(items_.size() + 1)));
    EXPECT_CALL(item(index), GetState()).WillRepeatedly(Return(state));
    int received_bytes =
        state == download::DownloadItem::IN_PROGRESS ? 50 : 100;
    EXPECT_CALL(item(index), GetReceivedBytes())
        .WillRepeatedly(Return(received_bytes));
    EXPECT_CALL(item(index), GetTotalBytes()).WillRepeatedly(Return(100));
    EXPECT_CALL(item(index), IsDone()).WillRepeatedly(Return(false));
    if (state == DownloadState::IN_PROGRESS) {
      in_progress_count_++;
    }
    EXPECT_CALL(manager(), InProgressCount())
        .WillRepeatedly(Return(in_progress_count_));

    std::vector<download::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    EXPECT_CALL(*manager_.get(), GetAllDownloads(_))
        .WillRepeatedly(SetArgPointee<0>(items));
    item(index).AddObserver(&controller().get_download_notifier_for_testing());
    controller().OnDownloadCreated(&manager(), &item(index));
  }

  void UpdateDownloadItem(int item_index, DownloadState state) {
    DCHECK_GT(items_.size(), static_cast<size_t>(item_index));

    EXPECT_CALL(item(item_index), GetState()).WillRepeatedly(Return(state));
    if (state == DownloadState::COMPLETE) {
      EXPECT_CALL(item(item_index), IsDone()).WillRepeatedly(Return(true));
      in_progress_count_--;
      EXPECT_CALL(manager(), InProgressCount())
          .WillRepeatedly(Return(in_progress_count_));
    } else {
      EXPECT_CALL(item(item_index), IsDone()).WillRepeatedly(Return(false));
    }
    item(item_index).NotifyObserversDownloadUpdated();
  }

  bool VerifyDisplayState(bool shown,
                          bool detail_shown,
                          DownloadIconState icon_state) {
    bool success = true;
    if (shown != display().IsShowing()) {
      success = false;
      ADD_FAILURE() << "Display should have shown state " << shown
                    << ", but found " << display().IsShowing();
    }
    if (detail_shown != display().IsDetailsShown()) {
      success = false;
      ADD_FAILURE() << "Display should have detailed shown state "
                    << detail_shown << ", but found "
                    << display().IsDetailsShown();
    }
    if (icon_state != display().GetDownloadIconState()) {
      success = false;
      ADD_FAILURE() << "Display should have detailed icon state "
                    << static_cast<int>(icon_state) << ", but found "
                    << static_cast<int>(display().GetDownloadIconState());
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
  TestingProfileManager testing_profile_manager_;
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
                                 /*icon_state=*/DownloadIconState::kComplete));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kProgress));

  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete));

  task_environment_.FastForwardBy(base::Hours(23));
  // The display is still showing because the last download is less than 1
  // day ago.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kComplete));

  task_environment_.FastForwardBy(base::Hours(1));
  // The display should stop showing once the last download is more than 1
  // day ago.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete));
}

TEST_F(DownloadDisplayControllerTest,
       UpdateToolbarButtonState_MultipleDownloads) {
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kProgress));

  // Reset details_shown before the second download starts. This can happen if
  // the user clicks somewhere else to dismiss the download bubble.
  display().SetDetailsShown(false);

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/true,
                                 /*icon_state=*/DownloadIconState::kProgress));

  // Reset details_shown while the downloads are in progress. This can happen if
  // the user clicks somewhere else to dismiss the download bubble.
  display().SetDetailsShown(false);

  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
  // The download icon state is still kProgress because not all downloads are
  // completed. details_shown is still false, because the details are only
  // popped up when the download is created.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kProgress));

  UpdateDownloadItem(/*item_index=*/1, DownloadState::COMPLETE);
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete));
}

TEST_F(DownloadDisplayControllerTest,
       UpdateToolbarButtonState_OnCompleteItemCreated) {
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete));

  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::COMPLETE);
  // Don't show the button if the new download is already completed.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete));
}

TEST_F(DownloadDisplayControllerTest, InitialState_OldLastDownload) {
  base::Time current_time = base::Time::Now();
  // Set the last complete time to more than 1 day ago.
  DownloadPrefs::FromDownloadManager(&manager())
      ->SetLastCompleteTime(current_time - base::Hours(25));

  DownloadDisplayController controller(&display(), &manager());
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete));
}

TEST_F(DownloadDisplayControllerTest, InitialState_NewLastDownload) {
  base::Time current_time = base::Time::Now();
  // Set the last complete time to less than 1 day ago.
  DownloadPrefs::FromDownloadManager(&manager())
      ->SetLastCompleteTime(current_time - base::Hours(23));

  DownloadDisplayController controller(&display(), &manager());
  // The initial state should not display details.
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/true, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete));

  // The display should stop showing once the last download is more than 1 day
  // ago.
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete));
}

TEST_F(DownloadDisplayControllerTest, InitialState_NoLastDownload) {
  DownloadDisplayController controller(&display(), &manager());
  EXPECT_TRUE(VerifyDisplayState(/*shown=*/false, /*detail_shown=*/false,
                                 /*icon_state=*/DownloadIconState::kComplete));
}
