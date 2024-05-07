// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_update_service.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/download/download_display.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/offline_item_state.h"
#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using ::offline_items_collection::OfflineItemState;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using StrictMockDownloadItem = testing::StrictMock<download::MockDownloadItem>;
using DownloadDangerType = download::DownloadDangerType;
using DownloadState = download::DownloadItem::DownloadState;
using DownloadUIModelPtr = DownloadUIModel::DownloadUIModelPtr;
using OfflineItemList =
    offline_items_collection::OfflineContentProvider::OfflineItemList;

const char kProviderNamespace[] = "mock_namespace";

class MockDownloadDisplayController : public DownloadDisplayController {
 public:
  MockDownloadDisplayController(Browser* browser,
                                DownloadBubbleUIController* bubble_controller)
      : DownloadDisplayController(nullptr, browser, bubble_controller) {
    bubble_controller->SetDownloadDisplayController(this);
  }
  void MaybeShowButtonWhenCreated() override {}
  MOCK_METHOD1(OnNewItem, void(bool));
  MOCK_METHOD2(OnUpdatedItem, void(bool, bool));
  MOCK_METHOD1(OnRemovedItem, void(const ContentId&));
};

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

  bool GetAllModelsToDisplay(
      std::vector<DownloadUIModelPtr>& models,
      const webapps::AppId* web_app_id,
      bool force_backfill_download_items = true) override {
    models.clear();
    int download_item_index = 0, offline_item_index = 0;
    // Compose a list of models from the items stored in the test fixture.
    for (ModelType type : model_types_) {
      if (type == ModelType::kDownloadItem) {
        auto model = DownloadItemModel::Wrap(
            download_items_->at(download_item_index++).get());
        if (model->ShouldShowInBubble()) {
          models.push_back(std::move(model));
        }
      } else {
        auto model = OfflineItemModel::Wrap(
            OfflineItemModelManagerFactory::GetForBrowserContext(profile_),
            offline_items_->at(offline_item_index++));
        if (model->ShouldShowInBubble()) {
          models.push_back(std::move(model));
        }
      }
    }
    return true;
  }

  void AddModel(ModelType type) { model_types_.push_back(type); }

  bool IsInitialized() const override { return true; }

  MOCK_METHOD(DownloadDisplay::ProgressInfo,
              GetProgressInfo,
              (const webapps::AppId*),
              (const override));

 private:
  raw_ptr<Profile> profile_;
  std::vector<ModelType> model_types_;
  const raw_ref<const std::vector<std::unique_ptr<StrictMockDownloadItem>>>
      download_items_;
  const raw_ref<const OfflineItemList> offline_items_;
};

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

    mock_update_service_ =
        std::make_unique<StrictMock<MockDownloadBubbleUpdateService>>(
            profile_, items_, offline_items_);
    window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile_, true);
    params.type = Browser::TYPE_NORMAL;
    params.window = window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));
    controller_ = std::make_unique<DownloadBubbleUIController>(
        browser_.get(), mock_update_service_.get());
    display_controller_ =
        std::make_unique<NiceMock<MockDownloadDisplayController>>(
            browser_.get(), controller_.get());
    second_controller_ = std::make_unique<DownloadBubbleUIController>(
        browser_.get(), mock_update_service_.get());
    second_display_controller_ =
        std::make_unique<NiceMock<MockDownloadDisplayController>>(
            browser_.get(), second_controller_.get());
  }

  void TearDown() override {
    DownloadCoreServiceFactory::GetForBrowserContext(profile_)
        ->SetDownloadManagerDelegateForTesting(nullptr);
    // The controller needs to be reset before download manager, because the
    // download_notifier_ will unregister itself from the manager.
    controller_.reset();
    second_controller_.reset();
    display_controller_.reset();
    second_display_controller_.reset();
    mock_update_service_.reset();
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
  MockDownloadBubbleUpdateService* mock_update_service() {
    return mock_update_service_.get();
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
    EXPECT_CALL(item(index), GetGuid()).WillRepeatedly(ReturnRefOfCopy(id));
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
    EXPECT_CALL(item(index), IsInsecure()).WillRepeatedly(Return(false));
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
    EXPECT_CALL(item(index), GetDangerType())
        .WillRepeatedly(
            Return(DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    EXPECT_CALL(*manager_, GetAllDownloads(_))
        .WillRepeatedly(SetArgPointee<0>(items));
    EXPECT_CALL(*manager_, GetDownloadByGuid(id))
        .WillRepeatedly(Return(&(item(index))));
    content::DownloadItemUtils::AttachInfoForTesting(&(item(index)), profile(),
                                                     nullptr);
    mock_update_service_->AddModel(
        MockDownloadBubbleUpdateService::ModelType::kDownloadItem);
    controller().OnDownloadItemAdded(&item(index), may_show_animation);
  }

  void UpdateDownloadItem(
      int item_index,
      DownloadState state,
      bool is_paused = false,
      DownloadDangerType danger_type =
          DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    DCHECK_GT(items_.size(), static_cast<size_t>(item_index));
    EXPECT_CALL(item(item_index), GetState()).WillRepeatedly(Return(state));
    EXPECT_CALL(item(item_index), IsDone())
        .WillRepeatedly(Return(state == DownloadState::COMPLETE));
    EXPECT_CALL(item(item_index), IsDangerous())
        .WillRepeatedly(
            Return(danger_type !=
                   DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    EXPECT_CALL(item(item_index), GetDangerType())
        .WillRepeatedly(Return(danger_type));
    EXPECT_CALL(item(item_index), IsPaused()).WillRepeatedly(Return(is_paused));
    controller().OnDownloadItemUpdated(&item(item_index));
  }

  void InitOfflineItem(OfflineItemState state, std::string id) {
    OfflineItem item;
    item.state = state;
    item.id.id = id;
    offline_items_.push_back(item);
    mock_update_service_->AddModel(
        MockDownloadBubbleUpdateService::ModelType::kOfflineItem);
    controller().OnOfflineItemsAdded({item});
  }

  void UpdateOfflineItem(int item_index, OfflineItemState state) {
    offline_items_[item_index].state = state;
    controller().OnOfflineItemUpdated(offline_items_[item_index]);
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
  raw_ptr<NiceMock<content::MockDownloadManager>, DanglingUntriaged> manager_;
  TestingProfileManager testing_profile_manager_;
  std::unique_ptr<
      NiceMock<offline_items_collection::MockOfflineContentProvider>>
      content_provider_;
  std::unique_ptr<TestBrowserWindow> window_;
  std::unique_ptr<StrictMock<MockDownloadBubbleUpdateService>>
      mock_update_service_;
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
  EXPECT_CALL(display_controller(), OnUpdatedItem(false, true)).Times(1);
  UpdateDownloadItem(/*item_index=*/0, DownloadState::IN_PROGRESS);
  EXPECT_CALL(display_controller(), OnUpdatedItem(true, true)).Times(1);
  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);

  EXPECT_CALL(display_controller(), OnNewItem(false)).Times(1);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[1]);
  EXPECT_CALL(display_controller(), OnUpdatedItem(true, true)).Times(1);
  UpdateOfflineItem(/*item_index=*/0, OfflineItemState::COMPLETE);
}

TEST_F(DownloadBubbleUIControllerTest, UpdatedItemIsPendingDeepScanning) {
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, "Download 1");
  EXPECT_CALL(display_controller(), OnUpdatedItem(true, true)).Times(1);
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
  EXPECT_FALSE(controller().last_primary_view_was_partial());
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
  if (download::IsDownloadBubblePartialViewEnabled(profile())) {
    ASSERT_EQ(partial_view.size(), 1u);
    EXPECT_EQ(partial_view[0]->GetContentId().id, ids[1]);
    EXPECT_TRUE(controller().last_primary_view_was_partial());
  } else {
    EXPECT_EQ(partial_view.size(), 0u);
  }
  std::vector<DownloadUIModelPtr> main_view = controller().GetMainView();
  EXPECT_EQ(main_view.size(), 2u);
  EXPECT_FALSE(controller().last_primary_view_was_partial());
}

TEST_F(DownloadBubbleUIControllerTest,
       OpeningMainViewRemovesCompletedEntryFromPartialView) {
  std::vector<std::string> ids = {"Download 1", "Offline 1"};
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0]);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[1]);

  if (download::IsDownloadBubblePartialViewEnabled(profile())) {
    EXPECT_EQ(controller().GetPartialView().size(), 2ul);
    EXPECT_TRUE(controller().last_primary_view_was_partial());
    EXPECT_EQ(second_controller().GetPartialView().size(), 2ul);
    EXPECT_TRUE(second_controller().last_primary_view_was_partial());
  } else {
    EXPECT_EQ(controller().GetPartialView().size(), 0ul);
    EXPECT_EQ(second_controller().GetPartialView().size(), 0ul);
  }

  UpdateDownloadItem(/*item_index=*/0, DownloadState::COMPLETE);
  UpdateOfflineItem(/*item_index=*/0, OfflineItemState::COMPLETE);
  EXPECT_EQ(controller().GetMainView().size(), 2ul);
  EXPECT_FALSE(controller().last_primary_view_was_partial());
  // Download was removed from partial view because it is completed.
  EXPECT_EQ(controller().GetPartialView().size(), 0ul);
  // The partial view wasn't actually shown, so this bit is not updated.
  EXPECT_FALSE(controller().last_primary_view_was_partial());
  EXPECT_EQ(second_controller().GetPartialView().size(), 0ul);
  EXPECT_EQ(second_controller().last_primary_view_was_partial(),
            download::IsDownloadBubblePartialViewEnabled(profile()));
}

TEST_F(DownloadBubbleUIControllerTest,
       OpeningMainViewDoesNotRemoveInProgressEntryFromPartialView) {
  std::vector<std::string> ids = {"Download 1", "Offline 1"};
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[0]);
  InitOfflineItem(OfflineItemState::IN_PROGRESS, ids[1]);

  if (download::IsDownloadBubblePartialViewEnabled(profile())) {
    EXPECT_EQ(controller().GetPartialView().size(), 2ul);
    EXPECT_TRUE(controller().last_primary_view_was_partial());
  } else {
    EXPECT_EQ(controller().GetPartialView().size(), 0ul);
  }

  // This does not remove the entries from the partial view because the items
  // are in progress.
  EXPECT_EQ(controller().GetMainView().size(), 2ul);
  EXPECT_FALSE(controller().last_primary_view_was_partial());
  if (download::IsDownloadBubblePartialViewEnabled(profile())) {
    EXPECT_EQ(controller().GetPartialView().size(), 2ul);
    EXPECT_TRUE(controller().last_primary_view_was_partial());
  } else {
    EXPECT_EQ(controller().GetPartialView().size(), 0ul);
  }
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
  if (download::IsDownloadBubblePartialViewEnabled(profile())) {
    EXPECT_EQ(controller().GetPartialView().size(), 1u);
    EXPECT_TRUE(controller().last_primary_view_was_partial());
  } else {
    EXPECT_EQ(controller().GetPartialView().size(), 0u);
  }

  // No items are returned for a partial view because it is too soon.
  task_environment_.FastForwardBy(base::Seconds(14));
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE, ids[1]);
  EXPECT_EQ(controller().GetPartialView().size(), 0u);
  // The partial view wasn't actually shown, so this bit is not updated.
  EXPECT_EQ(controller().last_primary_view_was_partial(),
            download::IsDownloadBubblePartialViewEnabled(profile()));

  // Partial view can now be shown, and contains all the items.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar3.pdf"),
                   download::DownloadItem::COMPLETE, ids[1]);
  if (download::IsDownloadBubblePartialViewEnabled(profile())) {
    EXPECT_EQ(controller().GetPartialView().size(), 3u);
    EXPECT_TRUE(controller().last_primary_view_was_partial());
  } else {
    EXPECT_EQ(controller().GetPartialView().size(), 0u);
  }

  // Showing the main view even before time is up should still work.
  task_environment_.FastForwardBy(base::Seconds(14));
  EXPECT_EQ(controller().GetPartialView().size(), 0u);
  // The partial view wasn't actually shown, so this bit is not updated.
  EXPECT_EQ(controller().last_primary_view_was_partial(),
            download::IsDownloadBubblePartialViewEnabled(profile()));
  EXPECT_EQ(controller().GetMainView().size(), 3u);
  EXPECT_FALSE(controller().last_primary_view_was_partial());

  // Main view resets the partial view time, so the partial view can now be
  // shown.
  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar4.pdf"),
                   download::DownloadItem::IN_PROGRESS, ids[3]);
  if (download::IsDownloadBubblePartialViewEnabled(profile())) {
    EXPECT_EQ(controller().GetPartialView().size(), 1u);
    EXPECT_TRUE(controller().last_primary_view_was_partial());
  } else {
    EXPECT_EQ(controller().GetPartialView().size(), 0u);
  }
}

// Tests that the partial view timer doesn't start if the partial view was
// empty and thus not shown.
TEST_F(DownloadBubbleUIControllerTest, EmptyPartialViewDoesNotPreventOpening) {
  EXPECT_EQ(controller().GetPartialView().size(), 0u);
  EXPECT_FALSE(controller().last_primary_view_was_partial());

  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE, "Download");
  // Partial view is returned despite previous call to GetPartialView less than
  // 15 seconds ago.
  if (download::IsDownloadBubblePartialViewEnabled(profile())) {
    EXPECT_EQ(controller().GetPartialView().size(), 1u);
    EXPECT_TRUE(controller().last_primary_view_was_partial());
  } else {
    EXPECT_EQ(controller().GetPartialView().size(), 0u);
  }
}

// Test that the preference suppresses the partial view.
TEST_F(DownloadBubbleUIControllerTest, PrefSuppressesPartialView) {
  download::SetDownloadBubblePartialViewEnabled(profile(), false);

  EXPECT_CALL(display_controller(), OnNewItem(true)).Times(1);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE, "Download");

  EXPECT_EQ(controller().GetPartialView().size(), 0u);
  EXPECT_FALSE(controller().last_primary_view_was_partial());

  download::SetDownloadBubblePartialViewEnabled(profile(), true);
  if (download::IsDownloadBubblePartialViewEnabled(profile())) {
    EXPECT_EQ(controller().GetPartialView().size(), 1u);
    EXPECT_TRUE(controller().last_primary_view_was_partial());
  } else {
    EXPECT_EQ(controller().GetPartialView().size(), 0u);
  }
}

}  // namespace
