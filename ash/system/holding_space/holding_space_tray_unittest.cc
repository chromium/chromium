// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kTestUser[] = "user@test";

std::unique_ptr<HoldingSpaceImage> CreateStubHoldingSpaceImage() {
  return std::make_unique<HoldingSpaceImage>(
      gfx::ImageSkia(), /*async_bitmap_resolver=*/base::DoNothing());
}

}  // namespace

class HoldingSpaceTrayTest : public AshTestBase {
 public:
  HoldingSpaceTrayTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kTemporaryHoldingSpace);
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    test_api_ = std::make_unique<HoldingSpaceTestApi>();
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(
        user_account, /*client=*/nullptr, model());
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
    GURL file_system_url(
        base::StrCat({"filesystem:", path.BaseName().value()}));
    std::unique_ptr<HoldingSpaceItem> item =
        HoldingSpaceItem::CreateFileBackedItem(type, path, file_system_url,
                                               CreateStubHoldingSpaceImage());
    HoldingSpaceItem* item_ptr = item.get();
    model()->AddItem(std::move(item));
    return item_ptr;
  }

  HoldingSpaceItem* AddPartiallyInitializedItem(HoldingSpaceItem::Type type,
                                                const base::FilePath& path) {
    // Create a holding space item, and use it to create a serialized item
    // dictionary.
    std::unique_ptr<HoldingSpaceItem> item =
        HoldingSpaceItem::CreateFileBackedItem(type, path,
                                               GURL("filesystem:ignored"),
                                               CreateStubHoldingSpaceImage());
    const base::DictionaryValue serialized_holding_space_item =
        item->Serialize();
    std::unique_ptr<HoldingSpaceItem> deserialized_item =
        HoldingSpaceItem::Deserialize(
            serialized_holding_space_item,
            /*image_resolver=*/
            base::BindLambdaForTesting([&](HoldingSpaceItem::Type type,
                                           const base::FilePath& file_path) {
              return CreateStubHoldingSpaceImage();
            }));

    HoldingSpaceItem* deserialized_item_ptr = deserialized_item.get();
    model()->AddItem(std::move(deserialized_item));
    return deserialized_item_ptr;
  }

  void StartSession() {
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    GetSessionControllerClient()->SwitchActiveUser(user_account);
  }

  void MarkTimeOfFirstPin() {
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    holding_space_prefs::MarkTimeOfFirstPin(
        GetSessionControllerClient()->GetUserPrefService(user_account));
  }

  HoldingSpaceTestApi* test_api() { return test_api_.get(); }

  HoldingSpaceModel* model() { return &holding_space_model_; }

 private:
  std::unique_ptr<HoldingSpaceTestApi> test_api_;
  HoldingSpaceModel holding_space_model_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HoldingSpaceTrayTest, ShowTrayButtonOnFirstUse) {
  StartSession();

  // Tray item should be shown for users that have never added anything to the
  // holding space.
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Show the bubble - only pinned container should be shown.
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesContainerShown());
  EXPECT_FALSE(test_api()->RecentFilesContainerShown());

  // Add and remove a download item.
  HoldingSpaceItem* item =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake"));
  EXPECT_TRUE(test_api()->RecentFilesContainerShown());
  model()->RemoveItem(item->id());

  // Verify the pinned files container, and the tray button are still shown.
  EXPECT_TRUE(test_api()->PinnedFilesContainerShown());
  EXPECT_FALSE(test_api()->RecentFilesContainerShown());

  test_api()->Close();
  EXPECT_TRUE(test_api()->IsShowingInShelf());

  test_api()->Show();

  // Add and remove a pinned item.
  HoldingSpaceItem* pinned_item =
      AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/pin"));
  MarkTimeOfFirstPin();
  model()->RemoveItem(pinned_item->id());

  // Verify that the pinned files container, and the tray button get hidden.
  EXPECT_FALSE(test_api()->PinnedFilesContainerShown());
  test_api()->Close();
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

TEST_F(HoldingSpaceTrayTest, AddingItemShowsTrayBubble) {
  MarkTimeOfFirstPin();
  StartSession();

  // The tray button should be hidden if the user has previously pinned an item,
  // and the holding space is empty.
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a download item - the button should be shown.
  HoldingSpaceItem* item_1 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  // Remove the only item - the button should be hidden.
  model()->RemoveItem(item_1->id());
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a screen capture item - the button should be shown.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_2"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  // Remove the only item - the button should be hidden.
  model()->RemoveItem(item_2->id());
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Add a pinned item - the button should be shown.
  HoldingSpaceItem* item_3 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_3"));
  EXPECT_TRUE(test_api()->IsShowingInShelf());
  // Remove the only item - the button should be hidden.
  model()->RemoveItem(item_3->id());
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

TEST_F(HoldingSpaceTrayTest, TrayButtonNotShownForPartialItemsOnly) {
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
  EXPECT_FALSE(test_api()->IsShowingInShelf());

  // Finalize one item, and verify the tray button gets shown.
  model()->FinalizeOrRemoveItem(item_2->id(), GURL("filesystem:fake_2"));

  EXPECT_TRUE(test_api()->IsShowingInShelf());

  // Remove the finalized item - the shelf button should get hidden.
  model()->RemoveItem(item_2->id());
  EXPECT_FALSE(test_api()->IsShowingInShelf());
}

// Tests how download chips are updated during item addition, removal and
// finalization
TEST_F(HoldingSpaceTrayTest, DownloadsContainer) {
  StartSession();

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesContainerShown());
  EXPECT_FALSE(test_api()->RecentFilesContainerShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());

  // Add a download item and verify recent file container gets shown.
  HoldingSpaceItem* item_1 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/fake_1"));

  EXPECT_TRUE(test_api()->PinnedFilesContainerShown());
  EXPECT_TRUE(test_api()->RecentFilesContainerShown());

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
  // container, in the order of addition, replacing the oldest item.
  model()->FinalizeOrRemoveItem(item_2->id(), GURL("filesystem:fake_2"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove the newest item, and verify the container gets updated.
  model()->RemoveItem(item_3->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove other items, and verify the recent files container gets hidden.
  model()->RemoveItem(item_2->id());

  EXPECT_TRUE(test_api()->RecentFilesContainerShown());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(1u, download_chips.size());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());

  model()->RemoveItem(item_1->id());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());

  EXPECT_FALSE(test_api()->RecentFilesContainerShown());

  // Pinned container is showing "educational" info, and it should remain shown.
  EXPECT_TRUE(test_api()->PinnedFilesContainerShown());

  test_api()->Close();
}

TEST_F(HoldingSpaceTrayTest, FinalizingDownloadItemThatShouldBeInvisible) {
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
  // container.
  model()->FinalizeOrRemoveItem(item_1->id(), GURL("filesystem:fake_1"));

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  // Remove the oldest item, and verify the container doesn't get updated.
  model()->RemoveItem(item_1->id());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  download_chips = test_api()->GetDownloadChips();
  ASSERT_EQ(2u, download_chips.size());
  EXPECT_EQ(item_3->id(),
            HoldingSpaceItemView::Cast(download_chips[0])->item()->id());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(download_chips[1])->item()->id());

  test_api()->Close();
}

// Tests that a partially initialized download item does not get shown if a full
// download item gets removed from the holding space.
TEST_F(HoldingSpaceTrayTest, PartialItemNowShownOnRemovingADownloadItem) {
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

  test_api()->Close();
}

// Tests how screen capture list is updated during item addition, removal and
// finalization
TEST_F(HoldingSpaceTrayTest, ScreenCaptureContainer) {
  StartSession();
  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesContainerShown());
  EXPECT_FALSE(test_api()->RecentFilesContainerShown());

  // Add a screenshot item and verify recent file container gets shown.
  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kScreenshot,
                                     base::FilePath("/tmp/fake_1"));

  EXPECT_TRUE(test_api()->PinnedFilesContainerShown());
  EXPECT_TRUE(test_api()->RecentFilesContainerShown());

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

  // Add more items to fill up the container.
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
  // container, in the order of addition, replacing the oldest item.
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

  // Remove the newest item, and verify the container gets updated.
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

  // Remove other items, and verify the recent files container gets hidden.
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
  EXPECT_FALSE(test_api()->RecentFilesContainerShown());

  // Pinned container is showing "educational" info, and it should remain shown.
  EXPECT_TRUE(test_api()->PinnedFilesContainerShown());

  test_api()->Close();
}

TEST_F(HoldingSpaceTrayTest, FinalizingScreenCaptureItemThatShouldBeInvisible) {
  StartSession();
  test_api()->Show();

  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add partially initialized download item - verify it doesn't get shown in
  // the UI yet.
  HoldingSpaceItem* item_1 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kScreenshot, base::FilePath("/tmp/fake_1"));

  EXPECT_FALSE(test_api()->RecentFilesContainerShown());

  EXPECT_TRUE(test_api()->GetPinnedFileChips().empty());
  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());

  // Add enough screenshot items to fill up the container.
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
  // container.
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

  // Remove the oldest item, and verify the container doesn't get updated.
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
TEST_F(HoldingSpaceTrayTest, PartialItemNowShownOnRemovingAScreenCapture) {
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

// Tests how the pinned item list is updated during item addition, removal and
// finalization.
TEST_F(HoldingSpaceTrayTest, PinnedFilesContainer) {
  MarkTimeOfFirstPin();
  StartSession();

  HoldingSpaceItem* item_1 = AddItem(HoldingSpaceItem::Type::kPinnedFile,
                                     base::FilePath("/tmp/fake_1"));

  test_api()->Show();
  EXPECT_TRUE(test_api()->PinnedFilesContainerShown());
  EXPECT_FALSE(test_api()->RecentFilesContainerShown());

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

  // Add more items to the container.
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

  // Remove the newest item, and verify the container gets updated.
  model()->RemoveItem(item_4->id());

  EXPECT_TRUE(test_api()->GetDownloadChips().empty());
  EXPECT_TRUE(test_api()->GetScreenCaptureViews().empty());
  pinned_files = test_api()->GetPinnedFileChips();
  ASSERT_EQ(2u, pinned_files.size());
  EXPECT_EQ(item_2->id(),
            HoldingSpaceItemView::Cast(pinned_files[0])->item()->id());
  EXPECT_EQ(item_1->id(),
            HoldingSpaceItemView::Cast(pinned_files[1])->item()->id());

  // Remove other items, and verify the files container gets hidden.
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

  EXPECT_FALSE(test_api()->RecentFilesContainerShown());
  EXPECT_FALSE(test_api()->PinnedFilesContainerShown());

  test_api()->Close();
}

// Verifies the pinned items container is not shown if it only contains
// partially initialized items.
TEST_F(HoldingSpaceTrayTest,
       PinnedFilesContainerWithPartiallyInitializedItemsOnly) {
  MarkTimeOfFirstPin();
  StartSession();

  // Add a download item to show the tray button.
  AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/download"));

  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kPinnedFile,
                              base::FilePath("/tmp/fake_1"));

  test_api()->Show();
  EXPECT_FALSE(test_api()->PinnedFilesContainerShown());

  // Add another partially initialized item.
  HoldingSpaceItem* item_2 = AddPartiallyInitializedItem(
      HoldingSpaceItem::Type::kPinnedFile, base::FilePath("/tmp/fake_2"));
  EXPECT_FALSE(test_api()->PinnedFilesContainerShown());

  // Add a fully initialized item, and verify it gets shown.
  HoldingSpaceItem* item_3 = AddItem(HoldingSpaceItem::Type::kPinnedFile,
                                     base::FilePath("/tmp/fake_3"));
  EXPECT_TRUE(test_api()->PinnedFilesContainerShown());

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

  test_api()->Close();
}

}  // namespace ash
