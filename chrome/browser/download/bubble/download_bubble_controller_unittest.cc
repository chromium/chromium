// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/download/bubble/download_display.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/bubble/download_icon_state.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile_key.h"
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
using testing::SetArgPointee;

namespace {
using StrictMockDownloadItem = testing::StrictMock<download::MockDownloadItem>;
using DownloadIconState = download::DownloadIconState;
using DownloadState = download::DownloadItem::DownloadState;
const char kProviderNamespace[] = "mock_namespace";

class MockDownloadDisplayController : public DownloadDisplayController {
 public:
  MockDownloadDisplayController(Profile* profile,
                                DownloadBubbleUIController* bubble_controller)
      : DownloadDisplayController(nullptr, profile, bubble_controller) {}
  void MaybeShowButtonWhenCreated() override {}
  MOCK_METHOD1(OnNewItem, void(bool));
  MOCK_METHOD1(OnUpdatedItem, void(bool));
  MOCK_METHOD0(OnRemovedItem, void());
};

}  // namespace

class DownloadBubbleUIControllerTest : public testing::Test {
 public:
  DownloadBubbleUIControllerTest()
      : manager_(std::make_unique<NiceMock<content::MockDownloadManager>>()),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  DownloadBubbleUIControllerTest(const DownloadBubbleUIControllerTest&) =
      delete;
  DownloadBubbleUIControllerTest& operator=(
      const DownloadBubbleUIControllerTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");
    EXPECT_CALL(*manager_.get(), GetBrowserContext())
        .WillRepeatedly(Return(profile_));

    // Set test delegate to get the corresponding download prefs.
    auto delegate = std::make_unique<ChromeDownloadManagerDelegate>(profile_);
    DownloadCoreServiceFactory::GetForBrowserContext(profile_)
        ->SetDownloadManagerDelegateForTesting(std::move(delegate));

    content_provider_ = std::make_unique<
        NiceMock<offline_items_collection::MockOfflineContentProvider>>();
    OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey())
        ->RegisterProvider(kProviderNamespace, content_provider_.get());

    controller_ = std::make_unique<DownloadBubbleUIController>(profile_);
    display_controller_ =
        std::make_unique<NiceMock<MockDownloadDisplayController>>(
            profile_, controller_.get());
    controller_->set_manager_for_testing(manager_.get());
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
  NiceMock<MockDownloadDisplayController>& display_controller() {
    return *display_controller_;
  }
  DownloadBubbleUIController& controller() { return *controller_; }
  NiceMock<offline_items_collection::MockOfflineContentProvider>&
  content_provider() {
    return *content_provider_;
  }

  void InitDownloadItem(const base::FilePath::CharType* path,
                        DownloadState state,
                        std::string& id,
                        bool is_transient = false,
                        base::Time start_time = base::Time::Now()) {
    size_t index = items_.size();
    items_.push_back(std::make_unique<StrictMockDownloadItem>());
    EXPECT_CALL(item(index), GetId())
        .WillRepeatedly(Return(static_cast<uint32_t>(items_.size() + 1)));
    EXPECT_CALL(item(index), GetGuid()).WillRepeatedly(testing::ReturnRef(id));
    EXPECT_CALL(item(index), GetState()).WillRepeatedly(Return(state));
    EXPECT_CALL(item(index), GetStartTime()).WillRepeatedly(Return(start_time));
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
    std::vector<download::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    EXPECT_CALL(*manager_.get(), GetAllDownloads(_))
        .WillRepeatedly(SetArgPointee<0>(items));
    item(index).AddObserver(&controller().get_download_notifier_for_testing());
    content::DownloadItemUtils::AttachInfoForTesting(&(item(index)), profile_,
                                                     nullptr);
    controller().OnDownloadCreated(&manager(), &item(index));
  }

  void UpdateDownloadItem(int item_index, DownloadState state) {
    DCHECK_GT(items_.size(), static_cast<size_t>(item_index));

    EXPECT_CALL(item(item_index), GetState()).WillRepeatedly(Return(state));
    if (state == DownloadState::COMPLETE) {
      EXPECT_CALL(item(item_index), IsDone()).WillRepeatedly(Return(true));
      DownloadPrefs::FromDownloadManager(&manager())
          ->SetLastCompleteTime(base::Time::Now());
    } else {
      EXPECT_CALL(item(item_index), IsDone()).WillRepeatedly(Return(false));
    }
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
  std::unique_ptr<NiceMock<MockDownloadDisplayController>> display_controller_;
  std::vector<std::unique_ptr<StrictMockDownloadItem>> items_;
  OfflineItemList offline_items_;
  std::unique_ptr<NiceMock<content::MockDownloadManager>> manager_;
  TestingProfileManager testing_profile_manager_;
  std::unique_ptr<
      NiceMock<offline_items_collection::MockOfflineContentProvider>>
      content_provider_;
  Profile* profile_;
};

TEST_F(DownloadBubbleUIControllerTest, ProcessesNewItems) {
  std::vector<std::string> ids = {"Download 1", "Download 2", "Offline 1"};
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0]);
  EXPECT_CALL(display_controller(), OnNewItem(false)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE, ids[1]);
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[2]);
}

TEST_F(DownloadBubbleUIControllerTest, ProcessesUpdatedItems) {
  std::vector<std::string> ids = {"Download 1", "Offline 1"};
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0]);
  EXPECT_CALL(display_controller(), OnUpdatedItem(false)).Times(1);
  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS);
  EXPECT_CALL(display_controller(), OnUpdatedItem(true)).Times(1);
  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);

  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[1]);
  EXPECT_CALL(display_controller(), OnUpdatedItem(true)).Times(1);
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
  std::vector<std::string> ids = {"Download 1", "Download 2", "Download 3",
                                  "Offline 1"};
  std::vector<base::TimeDelta> start_time_offsets = {
      base::Hours(1), base::Hours(4), base::Hours(2)};
  std::vector<std::string> sorted_ids = {"Offline 1", "Download 1",
                                         "Download 3", "Download 2"};
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
  EXPECT_EQ(models.size(), 4ul);
  for (unsigned long i = 0; i < models.size(); i++) {
    EXPECT_EQ(models[i]->GetContentId().id, sorted_ids[i]);
  }
}

TEST_F(DownloadBubbleUIControllerTest, ListIsRecent) {
  std::vector<std::string> ids = {"Download 1", "Download 2", "Download 3",
                                  "Offline 1"};
  std::vector<base::TimeDelta> start_time_offsets = {
      base::Hours(1), base::Hours(25), base::Hours(2)};
  std::vector<std::string> sorted_ids = {"Offline 1", "Download 1",
                                         "Download 3"};
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
  EXPECT_EQ(models.size(), 3ul);
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
  std::vector<DownloadUIModelPtr> partial_view = controller().GetPartialView();
  EXPECT_EQ(partial_view.size(), 2ul);
  std::vector<DownloadUIModelPtr> main_view = controller().GetMainView();
  EXPECT_EQ(main_view.size(), 2ul);
  std::vector<DownloadUIModelPtr> partial_view_empty =
      controller().GetPartialView();
  EXPECT_EQ(partial_view_empty.size(), 0ul);
}
