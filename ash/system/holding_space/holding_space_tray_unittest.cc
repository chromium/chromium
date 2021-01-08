// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray.h"

#include <deque>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kTestUser[] = "user@test";

// Helpers ---------------------------------------------------------------------

// A wrapper around `views::View::GetVisible()` with a null check for `view`.
bool IsViewVisible(views::View* view) {
  return view && view->GetVisible();
}

void Click(views::View* view, int flags) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.set_flags(flags);
  event_generator.ClickLeftButton();
}

void PressKey(views::View* view, ui::KeyboardCode key_code, int flags) {
  auto* root_window = view->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.PressKey(key_code, flags);
}

std::unique_ptr<HoldingSpaceImage> CreateStubHoldingSpaceImage(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      HoldingSpaceImage::GetMaxSizeForType(type), file_path,
      /*async_bitmap_resolver=*/base::DoNothing());
}

// Mocks -----------------------------------------------------------------------

class MockHoldingSpaceClient : public HoldingSpaceClient {
 public:
  // HoldingSpaceClient:
  MOCK_METHOD(void,
              AddScreenshot,
              (const base::FilePath& file_path),
              (override));
  MOCK_METHOD(void,
              AddScreenRecording,
              (const base::FilePath& file_path),
              (override));
  MOCK_METHOD(void,
              CopyImageToClipboard,
              (const HoldingSpaceItem& item, SuccessCallback callback),
              (override));
  MOCK_METHOD(void, OpenDownloads, (SuccessCallback callback), (override));
  MOCK_METHOD(void, OpenMyFiles, (SuccessCallback callback), (override));
  MOCK_METHOD(void,
              OpenItems,
              (const std::vector<const HoldingSpaceItem*>& items,
               SuccessCallback callback),
              (override));
  MOCK_METHOD(void,
              ShowItemInFolder,
              (const HoldingSpaceItem& item, SuccessCallback callback),
              (override));
  MOCK_METHOD(void,
              PinItems,
              (const std::vector<const HoldingSpaceItem*>& items),
              (override));
  MOCK_METHOD(void,
              UnpinItems,
              (const std::vector<const HoldingSpaceItem*>& items),
              (override));
};

}  // namespace

// HoldingSpaceTrayTest --------------------------------------------------------

// Parameterized by whether the previews feature is enabled.
class HoldingSpaceTrayTest : public AshTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  HoldingSpaceTrayTest() {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;

    enabled_features.push_back(features::kTemporaryHoldingSpace);

    if (IsPreviewsFeatureEnabled())
      enabled_features.push_back(features::kTemporaryHoldingSpacePreviews);
    else
      disabled_features.push_back(features::kTemporaryHoldingSpacePreviews);

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    test_api_ = std::make_unique<HoldingSpaceTestApi>();
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(
        user_account, client(), model());
    GetSessionControllerClient()->AddUserSession(kTestUser);
    holding_space_prefs::MarkTimeOfFirstAvailability(
        GetSessionControllerClient()->GetUserPrefService(user_account));
  }

  void TearDown() override {
    test_api_.reset();
    AshTestBase::TearDown();
  }

  HoldingSpaceItem* AddItem(HoldingSpaceItem::Type type,
                            const base::FilePath& path) {
    return AddItemToModel(model(), type, path);
  }

  HoldingSpaceItem* AddItemToModel(HoldingSpaceModel* target_model,
                                   HoldingSpaceItem::Type type,
                                   const base::FilePath& path) {
    GURL file_system_url(
        base::StrCat({"filesystem:", path.BaseName().value()}));
    std::unique_ptr<HoldingSpaceItem> item =
        HoldingSpaceItem::CreateFileBackedItem(
            type, path, file_system_url,
            base::BindOnce(&CreateStubHoldingSpaceImage));
    HoldingSpaceItem* item_ptr = item.get();
    target_model->AddItem(std::move(item));
    return item_ptr;
  }
  HoldingSpaceItem* AddPartiallyInitializedItem(HoldingSpaceItem::Type type,
                                                const base::FilePath& path) {
    // Create a holding space item, and use it to create a serialized item
    // dictionary.
    std::unique_ptr<HoldingSpaceItem> item =
        HoldingSpaceItem::CreateFileBackedItem(
            type, path, GURL("filesystem:ignored"),
            base::BindOnce(&CreateStubHoldingSpaceImage));
    const base::DictionaryValue serialized_holding_space_item =
        item->Serialize();
    std::unique_ptr<HoldingSpaceItem> deserialized_item =
        HoldingSpaceItem::Deserialize(
            serialized_holding_space_item,
            /*image_resolver=*/
            base::BindOnce(&CreateStubHoldingSpaceImage));

    HoldingSpaceItem* deserialized_item_ptr = deserialized_item.get();
    model()->AddItem(std::move(deserialized_item));
    return deserialized_item_ptr;
  }

  // The holding space tray is only visible in the shelf after the first holding
  // space item has been added. Most tests do not care about this so, as a
  // convenience, the time of first add will be marked prior to starting the
  // session when `pre_mark_time_of_first_add` is true.
  void StartSession(bool pre_mark_time_of_first_add = true) {
    if (pre_mark_time_of_first_add)
      MarkTimeOfFirstAdd();

    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    GetSessionControllerClient()->SwitchActiveUser(user_account);
  }

  void MarkTimeOfFirstAdd() {
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    holding_space_prefs::MarkTimeOfFirstAdd(
        GetSessionControllerClient()->GetUserPrefService(user_account));
  }

  void MarkTimeOfFirstPin() {
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    holding_space_prefs::MarkTimeOfFirstPin(
        GetSessionControllerClient()->GetUserPrefService(user_account));
  }

  void SwitchToSecondaryUser(const std::string& user_id,
                             HoldingSpaceClient* client,
                             HoldingSpaceModel* model) {
    AccountId user_account = AccountId::FromUserEmail(user_id);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(user_account,
                                                                 client, model);
    GetSessionControllerClient()->AddUserSession(user_id);

    holding_space_prefs::MarkTimeOfFirstAvailability(
        GetSessionControllerClient()->GetUserPrefService(user_account));
    holding_space_prefs::MarkTimeOfFirstAdd(
        GetSessionControllerClient()->GetUserPrefService(user_account));
    holding_space_prefs::MarkTimeOfFirstPin(
        GetSessionControllerClient()->GetUserPrefService(user_account));

    GetSessionControllerClient()->SwitchActiveUser(user_account);
  }

  void UnregisterModelForUser(const std::string& user_id) {
    AccountId user_account = AccountId::FromUserEmail(user_id);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(
        user_account, nullptr, nullptr);
  }

  bool IsPreviewsFeatureEnabled() const { return GetParam(); }

  HoldingSpaceTestApi* test_api() { return test_api_.get(); }

  testing::NiceMock<MockHoldingSpaceClient>* client() {
    return &holding_space_client_;
  }

  HoldingSpaceModel* model() { return &holding_space_model_; }

  HoldingSpaceTray* GetTray() {
    return Shelf::ForWindow(Shell::GetRootWindowForNewWindows())
        ->shelf_widget()
        ->status_area_widget()
        ->holding_space_tray();
  }

 private:
  std::unique_ptr<HoldingSpaceTestApi> test_api_;
  testing::NiceMock<MockHoldingSpaceClient> holding_space_client_;
  HoldingSpaceModel holding_space_model_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

TEST_P(HoldingSpaceTrayTest, ShowTrayButtonOnFirstUse) {
  StartSession(/*pre_mark_time_of_first_add=*/false);
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  // The tray button should *not* be shown for users that have never added
  // anything to the holding space.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item. This should cause the tray button to show.
  HoldingSpaceItem* item =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake"));
  MarkTimeOfFirstAdd();
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Show the bubble - both the pinned files and recent files child bubbles
  // should be shown.
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  // Remove the download item and verify the pinned files bubble, and the
  // tray button are still shown.
  model()->RemoveItem(item->id());
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  test_api()->Close();
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  EXPECT_TRUE(IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_FALSE(IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  test_api()->Show();

  // Add and remove a pinned item.
  HoldingSpaceItem* pinned_item =
      AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/pin"));
  MarkTimeOfFirstPin();
  model()->RemoveItem(pinned_item->id());

  // Verify that the pinned files bubble, and the tray button get hidden.
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());
  test_api()->Close();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

TEST_P(HoldingSpaceTrayTest, HideButtonWhenModelDetached) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item - the button should be shown.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  SwitchToSecondaryUser("user@secondary", /*client=*/nullptr,
                        /*model=*/nullptr);
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();

  EXPECT_FALSE(test_api()->IsShowingInShelf());
  UnregisterModelForUser("user@secondary");
}

TEST_P(HoldingSpaceTrayTest, HideButtonOnChangeToEmptyModel) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item - the button should be shown.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  HoldingSpaceModel secondary_holding_space_model;
  SwitchToSecondaryUser("user@secondary", /*client=*/nullptr,
                        /*model=*/&secondary_holding_space_model);
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  AddItemToModel(&secondary_holding_space_model,
                 HoldingSpaceItem::Type::kDownload,
                 base::FilePath("/tmp/fake_2"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  UnregisterModelForUser("user@secondary");
}

TEST_P(HoldingSpaceTrayTest, HideButtonOnChangeToNonEmptyModel) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  HoldingSpaceModel secondary_holding_space_model;
  AddItemToModel(&secondary_holding_space_model,
                 HoldingSpaceItem::Type::kDownload,
                 base::FilePath("/tmp/fake_2"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  SwitchToSecondaryUser("user@secondary", /*client=*/nullptr,
                        /*model=*/&secondary_holding_space_model);
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  UnregisterModelForUser("user@secondary");
}

TEST_P(HoldingSpaceTrayTest, AddingItemShowsTrayBubble) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item - the button should be shown.
  HoldingSpaceItem* item_1 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Remove the only item - the button should be hidden.
  model()->RemoveItem(item_1->id());
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a screen capture item - the button should be shown.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Remove the only item - the button should be hidden.
  model()->RemoveItem(item_2->id());
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a pinned item - the button should be shown.
  HoldingSpaceItem* item_3 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_3"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Remove the only item - the button should be hidden.
  model()->RemoveItem(item_3->id());
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

TEST_P(HoldingSpaceTrayTest, TrayButtonNotShownForPartialItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add few partial items - the tray button should remain hidden.
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kDownload,
                              base::FilePath("/tmp/fake_1"));
  EXPECT_FALSE(test_api()->IsShowingInShelf());
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  EXPECT_FALSE(test_api()->IsShowingInShelf());
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kScreenshot,
                              base::FilePath("/tmp/fake_3"));
  EXPECT_FALSE(test_api()->IsShowingInShelf());
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kPinnedFile,
                              base::FilePath("/tmp/fake_4"));
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Finalize one item, and verify the tray button gets shown.
  model()->FinalizeOrRemoveItem(item_2->id(), GURL("filesystem:fake_2"));

  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  EXPECT_EQ(!IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetDefaultTrayIcon()));
  EXPECT_EQ(IsPreviewsFeatureEnabled(),
            IsViewVisible(test_api()->GetPreviewsTrayIcon()));

  // Remove the finalized item - the shelf button should get hidden.
  model()->RemoveItem(item_2->id());
  GetTray()->FirePreviewsUpdateTimerIfRunningForTesting();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

// Tests how download chips are updated during item addition, removal and
// finalization.
TEST_P(HoldingSpaceTrayTest, DownloadsSection) {
  StartSession();

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());

  // Add a download item and verify recent file bubble gets shown.
  HoldingSpaceItem* item_1 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));

  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  ASSERT_EQ(1u, test_api()->GetDownloadChips().size());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());

  // Add another download, and verify it's shown in the UI.
  HoldingSpaceItem* item_3 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_3"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Finalize partially initialized item, and verify it gets added to the
  // section, in the order of addition, replacing the oldest item.
  model()->FinalizeOrRemoveItem(item_2->id(), GURL("filesystem:fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove the newest item, and verify the section gets updated.
  model()->RemoveItem(item_3->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove other items, and verify the recent files bubble gets hidden.
  model()->RemoveItem(item_2->id());

  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());

  model()->RemoveItem(item_1->id());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // Pinned bubble is showing "educational" info, and it should remain shown.
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
}

// Verifies the downloads section is shown and orders items as expected when the
// model contains a number of finalized items prior to showing UI.
TEST_P(HoldingSpaceTrayTest, DownloadsSectionWithFinalizedItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  // Add a number of finalized download items.
  std::deque<HoldingSpaceItem*> items;
  for (size_t i = 0; i < kMaxDownloads; ++i) {
    items.push_back(
        AddItem(HoldingSpaceItem::Type::kDownload,
                base::FilePath("/tmp/fake_" + base::NumberToString(i))));
  }

  test_api()->Show();
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  std::vector<views::View*> download_files = test_api()->GetDownloadChips();
  ASSERT_EQ(items.size(), download_files.size());

  while (!items.empty()) {
    // View order is expected to be reverse of item order.
    auto* download_file = HoldingSpaceItemView::Cast(download_files.back());
    EXPECT_EQ(download_file->item()->id(), items.front()->id());

    items.pop_front();
    download_files.pop_back();
  }

  test_api()->Close();
}

TEST_P(HoldingSpaceTrayTest, FinalizingDownloadItemThatShouldBeInvisible) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* item_1 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));

  // Add two download items.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_3"));
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Finalize partially initialized item, and verify it's not added to the
  // section.
  model()->FinalizeOrRemoveItem(item_1->id(), GURL("filesystem:fake_1"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove the oldest item, and verify the section doesn't get updated.
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());
}

// Tests that a partially initialized download item does not get shown if a full
// download item gets removed from the holding space.
TEST_P(HoldingSpaceTrayTest, PartialItemNowShownOnRemovingADownloadItem) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kDownload,
                              base::FilePath("/tmp/fake_1"));

  // Add two download items.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_3"));
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove one of the fully initialized items, and verify the partially
  // initialized item is no shown.
  model()->RemoveItem(item_2->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
}

// Tests how screen captures section is updated during item addition, removal
// and finalization.
TEST_P(HoldingSpaceTrayTest, ScreenCapturesSection) {
  StartSession();
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // Add a screenshot item and verify recent file bubble gets shown.
  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_1"));

  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  ASSERT_EQ(1u, test_api()->GetScreenCaptureViews().size());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_captures =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(1u, screen_captures.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());

  // Add more items to fill up the section.
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* item_4 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_4"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Finalize partially initialized item, and verify it gets added to the
  // section, in the order of addition, replacing the oldest item.
  model()->FinalizeOrRemoveItem(item_2->id(), GURL("filesystem:fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Remove the newest item, and verify the section gets updated.
  model()->RemoveItem(item_4->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Remove other items, and verify the recent files bubble gets hidden.
  model()->RemoveItem(item_2->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(2u, screen_captures.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());

  model()->RemoveItem(item_3->id());
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // Pinned bubble is showing "educational" info, and it should remain shown.
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
}

// Verifies the screen captures section is shown and orders items as expected
// when the model contains a number of finalized items prior to showing UI.
TEST_P(HoldingSpaceTrayTest, ScreenCapturesSectionWithFinalizedItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  // Add a number of finalized screen capture items.
  std::deque<HoldingSpaceItem*> items;
  for (size_t i = 0; i < kMaxScreenCaptures; ++i) {
    items.push_back(
        AddItem(HoldingSpaceItem::Type::kScreenshot,
                base::FilePath("/tmp/fake_" + base::NumberToString(i))));
  }

  test_api()->Show();
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  std::vector<views::View*> screenshots = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(items.size(), screenshots.size());

  while (!items.empty()) {
    // View order is expected to be reverse of item order.
    auto* screenshot = HoldingSpaceItemView::Cast(screenshots.back());
    EXPECT_EQ(screenshot->item()->id(), items.front()->id());

    items.pop_front();
    screenshots.pop_back();
  }

  test_api()->Close();
}

TEST_P(HoldingSpaceTrayTest, FinalizingScreenCaptureItemThatShouldBeInvisible) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* item_1 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake_1"));

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add enough screenshot items to fill up the section.
  HoldingSpaceItem* item_2 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* item_4 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_4"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_captures =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Finalize partially initialized item, and verify it's not added to the
  // section.
  model()->FinalizeOrRemoveItem(item_1->id(), GURL("filesystem:fake_1"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Remove the oldest item, and verify the section doesn't get updated.
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  test_api()->Close();
}

// Tests that a partially initialized screenshot item does not get shown if a
// fully initialized screenshot item gets removed from the holding space.
TEST_P(HoldingSpaceTrayTest, PartialItemNowShownOnRemovingAScreenCapture) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized item - verify it doesn't get shown in the UI yet.
  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kScreenshot,
                              base::FilePath("/tmp/fake_1"));

  HoldingSpaceItem* item_2 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* item_4 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_4"));
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_captures =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_captures[2])->item()->id());

  // Remove one of the fully initialized items, and verify the partially
  // initialized item is no shown.
  model()->RemoveItem(item_2->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_captures = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(2u, screen_captures.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(screen_captures[0])->item()->id());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(screen_captures[1])->item()->id());

  test_api()->Close();
}

// Tests how the pinned item section is updated during item addition, removal
// and finalization.
TEST_P(HoldingSpaceTrayTest, PinnedFilesSection) {
  MarkTimeOfFirstPin();
  StartSession();

  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kPinnedFile,
                                     base::FilePath("/tmp/fake_1"));

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());

  // Add a partially initialized item - verify it doesn't get shown in the UI
  // yet.
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());

  // Add more items to the section.
  HoldingSpaceItem* item_3 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* item_4 = AddItem(HoldingSpaceItem::Type::kPinnedFile,
                                     base::FilePath("/tmp/fake_4"));

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(2u, pinned_files.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());

  // Finalize partially initialized item, and verify it gets shown.
  model()->FinalizeOrRemoveItem(item_2->id(), GURL("filesystem:fake_2"));

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(3u, pinned_files.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[2])->item()->id());

  // Remove a partial item.
  model()->RemoveItem(item_3->id());

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(3u, pinned_files.size());
  EXPECT_EQ(item_4->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[2])->item()->id());

  // Remove the newest item, and verify the section gets updated.
  model()->RemoveItem(item_4->id());

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(2u, pinned_files.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());

  // Remove other items, and verify the files section gets hidden.
  model()->RemoveItem(item_2->id());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());

  model()->RemoveItem(item_1->id());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());
}

// Verifies the pinned files bubble is not shown if it only contains partially
// initialized items.
TEST_P(HoldingSpaceTrayTest,
       PinnedFilesBubbleWithPartiallyInitializedItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  // Add a download item to show the tray button.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/download"));

  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kPinnedFile,
                              base::FilePath("/tmp/fake_1"));

  test_api()->Show();
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());

  // Add another partially initialized item.
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake_2"));
  EXPECT_FALSE(test_api()->PinnedFilesBubbleShown());

  // Add a fully initialized item, and verify it gets shown.
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kPinnedFile,
                                     base::FilePath("/tmp/fake_3"));
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());

  std::vector<views::View*> pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_TRUE(HoldingSpaceItemView::Cast(pinned_files[0])->GetVisible());

  // Finalize a partially initialized item with an empty URL - it should get
  // removed.
  model()->FinalizeOrRemoveItem(item_2->id(), GURL());

  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(1u, pinned_files.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
}

// Verifies the pinned items section is shown and orders items as expected when
// the model contains a number of finalized items prior to showing UI.
TEST_P(HoldingSpaceTrayTest, PinnedFilesSectionWithFinalizedItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  // Add a number of finalized pinned items.
  std::deque<HoldingSpaceItem*> items;
  for (int i = 0; i < 10; ++i) {
    items.push_back(
        AddItem(HoldingSpaceItem::Type::kPinnedFile,
                base::FilePath("/tmp/fake_" + base::NumberToString(i))));
  }

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());

  std::vector<views::View*> pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(items.size(), pinned_files.size());

  while (!items.empty()) {
    // View order is expected to be reverse of item order.
    auto* pinned_file = HoldingSpaceItemView::Cast(pinned_files.back());
    EXPECT_EQ(pinned_file->item()->id(), items.front()->id());

    items.pop_front();
    pinned_files.pop_back();
  }
  test_api()->Close();
}

// Tests that as nearby shared files are added to the model, they show on the
// downloads section.
TEST_P(HoldingSpaceTrayTest, DownloadsSectionWithNearbySharedFiles) {
  StartSession();

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  ASSERT_TRUE(test_api()->GetDownloadChips().empty());

  // Add a nearby share item and verify recent files bubble gets shown.
  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kNearbyShare,
                                     base::FilePath("/tmp/fake_1"));
  ASSERT_TRUE(item_1->IsFinalized());

  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  ASSERT_EQ(1u, test_api()->GetDownloadChips().size());

  // Add a download item, and verify it's also shown in the UI in the order they
  // were added.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove the first item, and verify the section gets updated.
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());

  test_api()->Close();
}

// Tests that a partially initialized nearby share item does not get shown if a
// full download item gets removed from the holding space.
TEST_P(HoldingSpaceTrayTest, PartialNearbyShareItemWithExistingDownloadItems) {
  StartSession();
  test_api()->Show();

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  ASSERT_TRUE(test_api()->GetDownloadChips().empty());

  // Add partially initialized nearby share item - verify it doesn't get shown
  // in the UI yet.
  HoldingSpaceItem* nearby_share_item =
      AddPartiallyInitializedItem(HoldingSpaceItem::Type::kNearbyShare,
                                  base::FilePath("/tmp/nearby_share"));
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());

  // Add partially initialized screenshot item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* screenshot_item = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/screenshot"));
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  // Add two download items.
  HoldingSpaceItem* download_item_1 = AddItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/download_1"));
  HoldingSpaceItem* download_item_2 = AddItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/download_2"));
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(download_item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(download_item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Finalize the nearby share item and verify it is not shown.
  model()->FinalizeOrRemoveItem(nearby_share_item->id(),
                                GURL("filesystem:nearby_share"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(download_item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(download_item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Finalize the screenshot item and verify it is shown. Note that the
  // finalized screenshot item should not affect appearance of the downloads
  // section of holding space UI. It shows in the screen captures section.
  model()->FinalizeOrRemoveItem(screenshot_item->id(),
                                GURL("filesystem:screenshot"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_EQ(1u, test_api()->GetScreenCaptureViews().size());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(download_item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(download_item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove one of the fully initialized items, and verify the nearby share item
  // that was finalized late is shown.
  model()->RemoveItem(download_item_1->id());

  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(download_item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(nearby_share_item->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  test_api()->Close();
}

// Tests that a partially initialized download item does not get shown if a
// full download item gets removed from the holding space.
TEST_P(HoldingSpaceTrayTest, PartialDownloadItemWithExistingNearbyShareItems) {
  StartSession();
  test_api()->Show();

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  ASSERT_TRUE(test_api()->GetDownloadChips().empty());

  // Add partially initialized download item - verify it doesn't get shown
  // in the UI yet.
  HoldingSpaceItem* item_1 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());

  // Add two nearby share items.
  HoldingSpaceItem* item_2 = AddItem(HoldingSpaceItem::Type::kNearbyShare,
                                     base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kNearbyShare,
                                     base::FilePath("/tmp/fake_3"));
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Finalize the download item and verify it is not shown.
  model()->FinalizeOrRemoveItem(item_1->id(), GURL("filesystem:fake_1"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove one of the fully initialized items, and verify the partially
  // initialized item is not shown.
  model()->RemoveItem(item_2->id());

  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  test_api()->Close();
}

// Right clicking the holding space tray should show a context menu if the
// previews feature is enabled. Otherwise it should do nothing.
TEST_P(HoldingSpaceTrayTest, ShouldMaybeShowContextMenuOnRightClick) {
  StartSession();

  views::View* tray = test_api()->GetTray();
  ASSERT_TRUE(tray);

  EXPECT_FALSE(views::MenuController::GetActiveInstance());

  // Move the mouse to and perform a right click on `tray`.
  auto* root_window = tray->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(tray->GetBoundsInScreen().CenterPoint());
  event_generator.ClickRightButton();

  EXPECT_EQ(!!views::MenuController::GetActiveInstance(),
            IsPreviewsFeatureEnabled());
}

// Tests that as screen recording files are added to the model, they show in the
// screen captures section.
TEST_P(HoldingSpaceTrayTest, ScreenCapturesSectionWithScreenRecordingFiles) {
  StartSession();

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  ASSERT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add a screen recording item and verify recent files section gets shown.
  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kScreenRecording,
                                     base::FilePath("/tmp/fake_1"));
  ASSERT_TRUE(item_1->IsFinalized());

  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  ASSERT_EQ(1u, test_api()->GetScreenCaptureViews().size());

  // Add a screenshot item, and verify it's also shown in the UI in the reverse
  // order they were added.
  HoldingSpaceItem* item_2 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_capture_chips =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(2u, screen_capture_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());

  // Remove the first item, and verify the section gets updated.
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(1u, screen_capture_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());

  test_api()->Close();
}

// Tests that a partially initialized screen recording item shows in the UI in
// the reverse order from added time rather than finalization time.
TEST_P(HoldingSpaceTrayTest,
       PartialScreenRecordingItemWithExistingScreenshotItems) {
  StartSession();
  test_api()->Show();

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  ASSERT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized screen recording item - verify it doesn't get
  // shown in the UI yet.
  HoldingSpaceItem* screen_recording_item =
      AddPartiallyInitializedItem(HoldingSpaceItem::Type::kScreenRecording,
                                  base::FilePath("/tmp/screen_recording"));
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add three screenshot items to fill up the section.
  HoldingSpaceItem* screenshot_item_1 = AddItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/screenshot_1"));
  HoldingSpaceItem* screenshot_item_2 = AddItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/screenshot_2"));
  HoldingSpaceItem* screenshot_item_3 = AddItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/screenshot_3"));
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_capture_chips =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screenshot_item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Finalize the screen recording item and verify it is not shown.
  model()->FinalizeOrRemoveItem(screen_recording_item->id(),
                                GURL("filesystem:screen_recording"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screenshot_item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Remove one of the fully initialized items, and verify the screen recording
  // item that was finalized late is shown.
  model()->RemoveItem(screenshot_item_1->id());

  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screen_recording_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Add partially initialized screen recording item - verify it doesn't get
  // shown in the UI yet.
  HoldingSpaceItem* screen_recording_item_last =
      AddPartiallyInitializedItem(HoldingSpaceItem::Type::kScreenRecording,
                                  base::FilePath("/tmp/screen_recording_last"));
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screen_recording_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Finalize the screen recording item and verify it is shown first.
  model()->FinalizeOrRemoveItem(screen_recording_item_last->id(),
                                GURL("filesystem:screen_recording"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screen_recording_item_last->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screenshot_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screenshot_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  test_api()->Close();
}

// Tests that partially initialized screenshot item shows in the UI in the
// reverse order from added time rather than finalization time.
TEST_P(HoldingSpaceTrayTest,
       PartialScreenshotItemWithExistingScreenRecordingItems) {
  StartSession();
  test_api()->Show();

  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  ASSERT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized screenshot item - verify it doesn't get shown
  // in the UI yet.
  HoldingSpaceItem* screenshot_item = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake_1"));
  EXPECT_FALSE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add three screenshot recording items to fill up the section.
  HoldingSpaceItem* screen_recording_item_1 = AddItem(
      HoldingSpaceItem::Type::kScreenRecording, base::FilePath("/tmp/fake_2"));
  HoldingSpaceItem* screen_recording_item_2 = AddItem(
      HoldingSpaceItem::Type::kScreenRecording, base::FilePath("/tmp/fake_3"));
  HoldingSpaceItem* screen_recording_item_3 = AddItem(
      HoldingSpaceItem::Type::kScreenRecording, base::FilePath("/tmp/fake_4"));
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());
  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  std::vector<views::View*> screen_capture_chips =
      test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screen_recording_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screen_recording_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screen_recording_item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Finalize the screenshot item and verify it is not shown.
  model()->FinalizeOrRemoveItem(screenshot_item->id(),
                                GURL("filesystem:fake_1"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screen_recording_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screen_recording_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screen_recording_item_1->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  // Remove one of the fully initialized items, and verify the partially
  // initialized item is not shown.
  model()->RemoveItem(screen_recording_item_1->id());

  screen_capture_chips = test_api()->GetScreenCaptureViews();
  ASSERT_EQ(3u, screen_capture_chips.size());
  EXPECT_EQ(screen_recording_item_3->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_EQ(screen_recording_item_2->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_EQ(screenshot_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[2])->item()->id());

  test_api()->Close();
}

// Screen recordings should have an overlaying play icon.
TEST_P(HoldingSpaceTrayTest, PlayIconForScreenRecordings) {
  StartSession();
  test_api()->Show();

  // Add one screenshot item and one screen recording item.
  HoldingSpaceItem* screenshot_item = AddItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake_1"));
  HoldingSpaceItem* screen_recording_item = AddItem(
      HoldingSpaceItem::Type::kScreenRecording, base::FilePath("/tmp/fake_2"));
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  std::vector<views::View*> screen_capture_chips =
      test_api()->GetScreenCaptureViews();

  EXPECT_EQ(2u, screen_capture_chips.size());

  EXPECT_EQ(screenshot_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[1])->item()->id());
  EXPECT_FALSE(screen_capture_chips[1]->GetViewByID(
      kHoldingSpaceScreenCapturePlayIconId));
  EXPECT_EQ(screen_recording_item->id(),
            HoldingSpaceItemView::Cast(screen_capture_chips[0])->item()->id());
  EXPECT_TRUE(screen_capture_chips[0]->GetViewByID(
      kHoldingSpaceScreenCapturePlayIconId));
}

// Until the user has pinned an item, a placeholder should exist in the pinned
// files bubble which contains a chip to open the Files app.
TEST_P(HoldingSpaceTrayTest, PlaceholderContainsFilesAppChip) {
  StartSession(/*pre_mark_time_of_first_add=*/false);

  // The tray button should *not* be shown for users that have never added
  // anything to the holding space.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item. This should cause the tray button to show.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake"));
  MarkTimeOfFirstAdd();
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble. Both the pinned files and recent files child bubbles
  // should be shown.
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesBubbleShown());
  EXPECT_TRUE(test_api()->RecentFilesBubbleShown());

  // A chip to open the Files app should exist in the pinned files bubble.
  views::View* pinned_files_bubble = test_api()->GetPinnedFilesBubble();
  ASSERT_TRUE(pinned_files_bubble);
  views::View* files_app_chip =
      pinned_files_bubble->GetViewByID(kHoldingSpaceFilesAppChipId);
  ASSERT_TRUE(files_app_chip);

  // Prior to being acted upon by the user, there should be no events logged to
  // the Files app chip histogram.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.FilesAppChip.Action.All",
      holding_space_metrics::FilesAppChipAction::kClick, 0);

  // Click the chip and expect a call to open the Files app.
  EXPECT_CALL(*client(), OpenMyFiles);
  Click(files_app_chip, 0);

  // After having been acted upon by the user, there should be a single click
  // event logged to the Files app chip histogram.
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.FilesAppChip.Action.All",
      holding_space_metrics::FilesAppChipAction::kClick, 1);
}

// User should be able to launch selected holding space items by pressing the
// enter key.
TEST_P(HoldingSpaceTrayTest, EnterKeyOpensSelectedFiles) {
  StartSession();

  // Add two download items.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake1"));
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake2"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble.
  test_api()->Show();
  std::vector<views::View*> download_chips = test_api()->GetDownloadChips();
  HoldingSpaceItemView* holding_space_item =
      HoldingSpaceItemView::Cast(download_chips[0]);

  // Click a download item chip. The view should be selected
  Click(download_chips[0], 0);
  ASSERT_TRUE(holding_space_item->selected());

  // Press the enter key. We expect the client to open the selected item.
  EXPECT_CALL(
      *client(),
      OpenItems(testing::ElementsAre(holding_space_item->item()), testing::_));
  PressKey(download_chips[0], ui::KeyboardCode::VKEY_RETURN, 0);

  test_api()->Show();

  download_chips = test_api()->GetDownloadChips();
  holding_space_item = HoldingSpaceItemView::Cast(download_chips[0]);
  HoldingSpaceItemView* holding_space_item_2 =
      HoldingSpaceItemView::Cast(download_chips[1]);

  // Click on both items to select them both.
  Click(download_chips[0], ui::EF_SHIFT_DOWN);
  Click(download_chips[1], ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(holding_space_item->selected());
  ASSERT_TRUE(holding_space_item_2->selected());

  // Press the enter key. We expect the client to open the selected items.
  EXPECT_CALL(*client(),
              OpenItems(testing::ElementsAre(holding_space_item_2->item(),
                                             holding_space_item->item()),
                        testing::_));
  PressKey(download_chips[0], ui::KeyboardCode::VKEY_RETURN, 0);
}

INSTANTIATE_TEST_SUITE_P(All, HoldingSpaceTrayTest, testing::Bool());

}  // namespace ash
