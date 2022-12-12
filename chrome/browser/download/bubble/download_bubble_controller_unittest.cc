// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_controller.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
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
using testing::ReturnRefOfCopy;
using testing::SetArgPointee;

namespace {
using StrictMockDownloadItem = testing::StrictMock<download::MockDownloadItem>;
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
  MOCK_METHOD2(OnUpdatedItem, void(bool, bool));
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

  void InitDownloadItem(const base::FilePath::CharType* path,
                        DownloadState state,
                        std::string& id,
                        bool is_transient = false,
                        base::Time start_time = base::Time::Now(),
                        bool show_details = true) {
    size_t index = items_.size();
    items_.push_back(std::make_unique<StrictMockDownloadItem>());
    EXPECT_CALL(item(index), GetId())
        .WillRepeatedly(Return(static_cast<uint32_t>(items_.size() + 1)));
    EXPECT_CALL(item(index), GetGuid()).WillRepeatedly(testing::ReturnRef(id));
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
        .WillRepeatedly(Return(download::DownloadItem::DownloadCreationType::
                                   TYPE_ACTIVE_DOWNLOAD));
    EXPECT_CALL(item(index), IsPaused()).WillRepeatedly(Return(false));
    std::vector<download::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    EXPECT_CALL(*manager_, GetAllDownloads(_))
        .WillRepeatedly(SetArgPointee<0>(items));
    item(index).AddObserver(&controller().get_download_notifier_for_testing());
    content::DownloadItemUtils::AttachInfoForTesting(&(item(index)), profile_,
                                                     nullptr);
    controller().OnNewItem(&item(index), show_details);
  }

  void UpdateDownloadItem(int item_index,
                          DownloadState state,
                          bool is_paused = false) {
    DCHECK_GT(items_.size(), static_cast<size_t>(item_index));
    EXPECT_CALL(item(item_index), GetState()).WillRepeatedly(Return(state));
    if (state == DownloadState::COMPLETE) {
      EXPECT_CALL(item(item_index), IsDone()).WillRepeatedly(Return(true));
      DownloadPrefs::FromDownloadManager(&manager())
          ->SetLastCompleteTime(base::Time::Now());
    } else {
      EXPECT_CALL(item(item_index), IsDone()).WillRepeatedly(Return(false));
    }
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

 private:
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
                                  "Download 3"};
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0]);
  EXPECT_CALL(display_controller(), OnNewItem(false)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE, ids[1]);
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[2]);
  EXPECT_CALL(display_controller(), OnNewItem(false)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[3],
                   /*is_transient=*/false, base::Time::Now(),
                   /*show_details=*/false);
}

TEST_F(DownloadBubbleUIControllerTest, ProcessesUpdatedItems) {
  std::vector<std::string> ids = {"Download 1", "Offline 1"};
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0]);
  EXPECT_CALL(display_controller(), OnUpdatedItem(false, true)).Times(1);
  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS);
  EXPECT_CALL(display_controller(), OnUpdatedItem(true, true)).Times(1);
  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);

  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[1]);
  EXPECT_CALL(display_controller(), OnUpdatedItem(true, true)).Times(1);
  UpdateOfflineItem(/*item_index=*/0, OfflineItemState::COMPLETE);
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

TEST_F(DownloadBubbleUIControllerTest,
       OpeningMainViewRemovesEntryFromPartialView) {
  std::vector<std::string> ids = {"Download 1", "Offline 1"};
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0]);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[1]);

  EXPECT_EQ(controller().GetPartialView().size(), 2ul);
  EXPECT_EQ(second_controller().GetPartialView().size(), 2ul);

  EXPECT_EQ(controller().GetMainView().size(), 2ul);
  EXPECT_EQ(controller().GetPartialView().size(), 0ul);
  EXPECT_EQ(second_controller().GetPartialView().size(), 0ul);
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
  // `show_details_if_done` is false because the download is initiated from the
  // main profile.
  EXPECT_CALL(*incognito_display_controller_,
              OnUpdatedItem(/*is_done=*/true, /*show_details_if_done=*/false))
      .Times(1);
  item(0).NotifyObserversDownloadUpdated();
}
