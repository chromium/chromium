// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_controller_impl.h"

#include <memory>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {
using crosapi::mojom::ClipboardHistoryControllerShowSource;

namespace {

void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

class MockClipboardImageModelFactory : public ClipboardImageModelFactory {
 public:
  MockClipboardImageModelFactory() = default;
  MockClipboardImageModelFactory(const MockClipboardImageModelFactory&) =
      delete;
  MockClipboardImageModelFactory& operator=(
      const MockClipboardImageModelFactory&) = delete;
  ~MockClipboardImageModelFactory() override = default;

  // ClipboardImageModelFactory:
  void Render(const base::UnguessableToken& clipboard_history_item_id,
              const std::string& markup,
              const gfx::Size& bounding_box_size,
              ImageModelCallback callback) override {
    std::move(callback).Run(ui::ImageModel());
  }

  void CancelRequest(const base::UnguessableToken& request_id) override {}

  void Activate() override {}

  void Deactivate() override {}

  void RenderCurrentPendingRequests() override {}

  void OnShutdown() override {}
};

void ExpectHistoryItemImageMatchesBitmap(const ClipboardHistoryItem& item,
                                         const SkBitmap& expected_bitmap) {
  EXPECT_EQ(item.display_format(),
            crosapi::mojom::ClipboardHistoryDisplayFormat::kPng);

  const auto& image = item.display_image();
  ASSERT_TRUE(image.has_value());
  ASSERT_TRUE(image.value().IsImage());
  ASSERT_FALSE(image.value().IsEmpty());
  EXPECT_TRUE(gfx::BitmapsAreEqual(*image.value().GetImage().ToSkBitmap(),
                                   expected_bitmap));
}

std::vector<ClipboardHistoryControllerShowSource>
GetClipboardHistoryShowSources() {
  std::vector<ClipboardHistoryControllerShowSource> sources;
  for (int i =
           static_cast<int>(ClipboardHistoryControllerShowSource::kMinValue);
       i <= static_cast<int>(ClipboardHistoryControllerShowSource::kMaxValue);
       ++i) {
    sources.push_back(static_cast<ClipboardHistoryControllerShowSource>(i));
  }
  return sources;
}

}  // namespace

class ClipboardHistoryControllerTest : public AshTestBase {
 public:
  ClipboardHistoryControllerTest() = default;
  ClipboardHistoryControllerTest(const ClipboardHistoryControllerTest&) =
      delete;
  ClipboardHistoryControllerTest& operator=(
      const ClipboardHistoryControllerTest&) = delete;
  ~ClipboardHistoryControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    mock_image_factory_ = std::make_unique<MockClipboardImageModelFactory>();
    GetClipboardHistoryController()->set_confirmed_operation_callback_for_test(
        operation_confirmed_future_.GetCallback());
  }

  ClipboardHistoryControllerImpl* GetClipboardHistoryController() {
    return Shell::Get()->clipboard_history_controller();
  }

  void ShowMenu() { PressAndReleaseKey(ui::VKEY_V, ui::EF_COMMAND_DOWN); }

  void WaitForOperationConfirmed() {
    EXPECT_TRUE(operation_confirmed_future_.Take());
  }

  void WriteImageToClipboardAndConfirm(const SkBitmap& bitmap) {
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteImage(bitmap);
    }
    WaitForOperationConfirmed();
  }

  void WriteTextToClipboardAndConfirm(const std::u16string& str) {
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteText(str);
    }
    WaitForOperationConfirmed();
  }

  std::vector<ClipboardHistoryItem> GetHistoryValues() {
    base::test::TestFuture<std::vector<ClipboardHistoryItem>> future;
    GetClipboardHistoryController()->GetHistoryValues(future.GetCallback());
    return future.Take();
  }

  void TestEnteringLockScreen() {
    // Querying clipboard history should return nothing if the screen is locked
    // while the request is in progress.
    GetClipboardHistoryController()->BlockGetHistoryValuesForTest();
    base::test::TestFuture<std::vector<ClipboardHistoryItem>> future;
    GetClipboardHistoryController()->GetHistoryValues(future.GetCallback());
    EXPECT_FALSE(future.IsReady());

    auto* session_controller = Shell::Get()->session_controller();
    session_controller->LockScreen();
    GetSessionControllerClient()->FlushForTest();  // `LockScreen()` is async.
    EXPECT_TRUE(session_controller->IsScreenLocked());

    GetClipboardHistoryController()->ResumeGetHistoryValuesForTest();
    auto locked_during_query_result = future.Take();
    EXPECT_TRUE(locked_during_query_result.empty());

    // Querying clipboard history should return nothing if the screen is locked
    // before the request is made.
    auto locked_before_query_result = GetHistoryValues();
    EXPECT_TRUE(locked_before_query_result.empty());
  }

 protected:
  base::test::RepeatingTestFuture<bool> operation_confirmed_future_;

 private:
  std::unique_ptr<MockClipboardImageModelFactory> mock_image_factory_;
};

TEST_F(ClipboardHistoryControllerTest, EncodeImage) {
  // Write a bitmap to ClipboardHistory.
  SkBitmap test_bitmap = gfx::test::CreateBitmap(3, 2);
  WriteImageToClipboardAndConfirm(test_bitmap);

  // The bitmap should be encoded to a PNG. Manually pry into the contents of
  // the result to confirm that the newly-encoded PNG is included.
  auto result = GetHistoryValues();
  ASSERT_EQ(result.size(), 1u);

  ExpectHistoryItemImageMatchesBitmap(result[0], test_bitmap);
}

TEST_F(ClipboardHistoryControllerTest, EncodeMultipleImages) {
  // Write a bunch of bitmaps to ClipboardHistory.
  std::vector<const SkBitmap> test_bitmaps;
  test_bitmaps.emplace_back(gfx::test::CreateBitmap(2, 1));
  test_bitmaps.emplace_back(gfx::test::CreateBitmap(3, 2));
  test_bitmaps.emplace_back(gfx::test::CreateBitmap(4, 3));
  for (const auto& test_bitmap : test_bitmaps) {
    WriteImageToClipboardAndConfirm(test_bitmap);
  }

  auto result = GetHistoryValues();
  auto num_results = result.size();
  ASSERT_EQ(num_results, test_bitmaps.size());

  // The bitmaps should be encoded to PNGs. Manually pry into the contents of
  // the result to confirm that the newly-encoded PNGs are included. History
  // values should be sorted by recency.
  for (uint i = 0; i < num_results; ++i) {
    ExpectHistoryItemImageMatchesBitmap(result[i],
                                        test_bitmaps[num_results - 1 - i]);
  }
}

TEST_F(ClipboardHistoryControllerTest, WriteBitmapWhileEncodingImage) {
  // Write a bitmap to ClipboardHistory.
  std::vector<const SkBitmap> test_bitmaps;
  test_bitmaps.emplace_back(gfx::test::CreateBitmap(3, 2));
  test_bitmaps.emplace_back(gfx::test::CreateBitmap(4, 3));
  WriteImageToClipboardAndConfirm(test_bitmaps[0]);

  // Write another bitmap to the clipboard while encoding the first bitmap.
  GetClipboardHistoryController()
      ->set_new_bitmap_to_write_while_encoding_for_test(test_bitmaps[1]);

  // Make sure the second bitmap is written to the clipboard before history
  // values are returned.
  GetClipboardHistoryController()->BlockGetHistoryValuesForTest();
  base::test::TestFuture<std::vector<ClipboardHistoryItem>> future;
  GetClipboardHistoryController()->GetHistoryValues(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  WaitForOperationConfirmed();

  GetClipboardHistoryController()->ResumeGetHistoryValuesForTest();
  auto result = future.Take();
  auto num_results = result.size();
  ASSERT_EQ(num_results, test_bitmaps.size());

  // Both bitmaps should be encoded to PNGs. Manually pry into the contents of
  // the result to confirm that the newly-encoded PNGs are included. History
  // values should be sorted by recency.
  for (uint i = 0; i < num_results; ++i) {
    ExpectHistoryItemImageMatchesBitmap(result[i],
                                        test_bitmaps[num_results - 1 - i]);
  }
}

TEST_F(ClipboardHistoryControllerTest, LockedScreenText) {
  // Write text to ClipboardHistory and verify that it can be retrieved.
  WriteTextToClipboardAndConfirm(u"test");
  auto history_list = GetHistoryValues();
  ASSERT_EQ(history_list.size(), 1u);
  ASSERT_EQ(history_list[0].display_text(), u"test");

  TestEnteringLockScreen();
}

TEST_F(ClipboardHistoryControllerTest, LockedScreenImage) {
  // Write a bitmap to ClipboardHistory and verify that it can be returned.
  SkBitmap test_bitmap = gfx::test::CreateBitmap(3, 2);
  WriteImageToClipboardAndConfirm(test_bitmap);
  auto result = GetHistoryValues();
  ASSERT_EQ(result.size(), 1u);
  ExpectHistoryItemImageMatchesBitmap(result[0], test_bitmap);

  TestEnteringLockScreen();
}

// Base class for tests of Clipboard History parameterized by whether the
// `kClipboardHistoryRefresh` feature flag is enabled.
class ClipboardHistoryControllerRefreshTest
    : public ClipboardHistoryControllerTest,
      public testing::WithParamInterface</*refresh_enabled=*/bool> {
 public:
  ClipboardHistoryControllerRefreshTest() {
    std::vector<base::test::FeatureRef> refresh_features = {
        features::kClipboardHistoryRefresh, chromeos::features::kJelly};
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    (IsClipboardHistoryRefreshEnabled() ? enabled_features : disabled_features)
        .swap(refresh_features);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool IsClipboardHistoryRefreshEnabled() const { return GetParam(); }

  // Some toasts can display on multiple root windows, so the caller can use
  // `root_window` to target a toast on a specific root window.
  ToastOverlay* GetCurrentOverlay(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    return Shell::Get()->toast_manager()->GetCurrentOverlayForTesting(
        root_window);
  }

  views::LabelButton* GetDismissButton(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    ToastOverlay* overlay = GetCurrentOverlay(root_window);
    DCHECK(overlay);
    return overlay->dismiss_button_for_testing();
  }

  void ClickDismissButton(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    views::LabelButton* dismiss_button = GetDismissButton(root_window);
    const gfx::Point button_center =
        dismiss_button->GetBoundsInScreen().CenterPoint();
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(button_center);
    event_generator->ClickLeftButton();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardHistoryControllerRefreshTest,
                         /*refresh_enabled=*/testing::Bool());

// Tests that search + v with no history fails to show a menu.
TEST_P(ClipboardHistoryControllerRefreshTest, NoHistoryNoMenu) {
  base::HistogramTester histogram_tester;
  ShowMenu();

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", 0);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 0);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", 0);
}

// Tests that search + v shows a menu when there is something to show.
TEST_P(ClipboardHistoryControllerRefreshTest, ShowMenu) {
  base::HistogramTester histogram_tester;

  // Copy something to enable the clipboard history menu.
  WriteTextToClipboardAndConfirm(u"test");

  // TODO(b/267694484): Do not fork these values once the menu can show items
  // with the refresh feature enabled.
  const int num_items_shown = IsClipboardHistoryRefreshEnabled() ? 0 : 1;
  int num_items_shown_samples = 0;
  ShowMenu();

  if (!IsClipboardHistoryRefreshEnabled()) {
    ++num_items_shown_samples;
  }

  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  // No user journey time should be recorded as the menu is still showing.
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 0);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", num_items_shown,
      num_items_shown_samples);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown",
      num_items_shown_samples);

  // Hide the menu.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", num_items_shown,
      num_items_shown_samples);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown",
      num_items_shown_samples);

  // Reshow the menu.
  ShowMenu();

  if (!IsClipboardHistoryRefreshEnabled()) {
    ++num_items_shown_samples;
  }

  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  // No new UserJourneyTime histogram should be recorded as the menu is
  // still showing.
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", num_items_shown,
      num_items_shown_samples);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown",
      num_items_shown_samples);

  // Hide the menu.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 2);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", num_items_shown,
      num_items_shown_samples);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown",
      num_items_shown_samples);
}

// Verifies that the clipboard history is disabled in some user modes, which
// means that the clipboard history should not be recorded and meanwhile the
// menu view should not show (https://crbug.com/1100739).
TEST_P(ClipboardHistoryControllerRefreshTest, VerifyAvailabilityInUserModes) {
  // Write one item into the clipboard history.
  WriteTextToClipboardAndConfirm(u"text");

  constexpr struct {
    user_manager::UserType user_type;
    bool is_enabled;
  } kTestCases[] = {{user_manager::USER_TYPE_REGULAR, true},
                    {user_manager::USER_TYPE_GUEST, true},
                    {user_manager::USER_TYPE_PUBLIC_ACCOUNT, false},
                    {user_manager::USER_TYPE_KIOSK_APP, false},
                    {user_manager::USER_TYPE_CHILD, true},
                    {user_manager::USER_TYPE_ARC_KIOSK_APP, false},
                    {user_manager::USER_TYPE_WEB_KIOSK_APP, false}};

  UserSession session;
  session.session_id = 1u;
  session.user_info.account_id = AccountId::FromUserEmail("user1@test.com");
  session.user_info.display_name = "User 1";
  session.user_info.display_email = "user1@test.com";

  for (const auto& test_case : kTestCases) {
    // Switch to the target user mode.
    session.user_info.type = test_case.user_type;
    Shell::Get()->session_controller()->UpdateUserSession(session);

    // Write a new item into the clipboard buffer.
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteText(u"test");
    }

    if (test_case.is_enabled) {
      WaitForOperationConfirmed();
    } else {
      FlushMessageLoop();
      // Note: This check might not catch a scenario where a mode expected to be
      // disabled actually allows writes to go through, because the operation
      // might not have finished yet in that case. The history verification
      // below mitigates the chance that such a bug would not be caught.
      EXPECT_TRUE(operation_confirmed_future_.IsEmpty());
    }

    const std::list<ClipboardHistoryItem>& items =
        Shell::Get()->clipboard_history_controller()->history()->GetItems();
    if (test_case.is_enabled) {
      // Verify that the new item should be included in the clipboard history
      // and the menu should be able to show.
      EXPECT_EQ(items.size(), 2u);

      ShowMenu();

      EXPECT_TRUE(
          Shell::Get()->clipboard_history_controller()->IsMenuShowing());

      PressAndReleaseKey(ui::VKEY_ESCAPE);

      EXPECT_FALSE(
          Shell::Get()->clipboard_history_controller()->IsMenuShowing());

      if (IsClipboardHistoryRefreshEnabled()) {
        // Wait for the clipboard manager to be fully destroyed so that the
        // presence of an open modal window does not block the next test case's
        // accelerator action from being performed.
        FlushMessageLoop();
      }

      // Restore the clipboard history by removing the new item.
      ClipboardHistory* clipboard_history = const_cast<ClipboardHistory*>(
          Shell::Get()->clipboard_history_controller()->history());
      clipboard_history->RemoveItemForId(items.begin()->id());
    } else {
      // Verify that the new item should not be written into the clipboard
      // history. The menu cannot show although the clipboard history is
      // non-empty.
      EXPECT_EQ(items.size(), 1u);

      ShowMenu();

      EXPECT_FALSE(
          Shell::Get()->clipboard_history_controller()->IsMenuShowing());
    }
  }
}

// Verifies that the clipboard history menu is disabled when the screen for
// user adding shows.
TEST_P(ClipboardHistoryControllerRefreshTest, DisableInUserAddingScreen) {
  WriteTextToClipboardAndConfirm(u"text");

  // Emulate that the user adding screen displays.
  Shell::Get()->session_controller()->ShowMultiProfileLogin();

  // Try to show the clipboard history menu; verify that the menu does not show.
  ShowMenu();
  EXPECT_FALSE(Shell::Get()->clipboard_history_controller()->IsMenuShowing());
}

// Tests that pressing and holding VKEY_V, then the search key (EF_COMMAND_DOWN)
// does not show the AppList.
TEST_P(ClipboardHistoryControllerRefreshTest, VThenSearchDoesNotShowLauncher) {
  GetEventGenerator()->PressKey(ui::VKEY_V, ui::EF_NONE);
  GetEventGenerator()->PressKey(ui::VKEY_LWIN, ui::EF_NONE);

  // Release VKEY_V, which could trigger a key released accelerator.
  GetEventGenerator()->ReleaseKey(ui::VKEY_V, ui::EF_NONE);

  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible(
      /*display_id=*/absl::nullopt));

  // Release VKEY_LWIN(search/launcher), which could trigger the app list.
  GetEventGenerator()->ReleaseKey(ui::VKEY_LWIN, ui::EF_NONE);

  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible(
      /*display_id=*/absl::nullopt));
}

// Tests that clearing the clipboard clears ClipboardHistory
TEST_P(ClipboardHistoryControllerRefreshTest, ClearClipboardClearsHistory) {
  // Write a single item to ClipboardHistory.
  WriteTextToClipboardAndConfirm(u"test");

  // Clear the clipboard.
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  FlushMessageLoop();

  // History should also be cleared.
  const std::list<ClipboardHistoryItem>& items =
      Shell::Get()->clipboard_history_controller()->history()->GetItems();
  EXPECT_TRUE(items.empty());

  ShowMenu();

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
}

// Tests that clearing the clipboard closes the ClipboardHistory menu.
TEST_P(ClipboardHistoryControllerRefreshTest,
       ClearingClipboardClosesClipboardHistory) {
  // Write a single item to ClipboardHistory.
  WriteTextToClipboardAndConfirm(u"test");

  ASSERT_TRUE(Shell::Get()->cursor_manager()->IsCursorVisible());

  ShowMenu();
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  // The cursor is visible after showing the clipboard history menu through
  // the accelerator.
  EXPECT_TRUE(Shell::Get()->cursor_manager()->IsCursorVisible());

  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  FlushMessageLoop();

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
}

// Tests that a toast is shown if something was copied to clipboard history.
TEST_P(ClipboardHistoryControllerRefreshTest, ShowToast) {
  // Copy something to enable the clipboard history menu.
  WriteTextToClipboardAndConfirm(u"test");

  ToastManagerImpl* manager_ = Shell::Get()->toast_manager();
  if (IsClipboardHistoryRefreshEnabled()) {
    EXPECT_TRUE(manager_->IsRunning(kClipboardCopyToastId));
    ClickDismissButton();
    EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  } else {
    EXPECT_FALSE(manager_->IsRunning(kClipboardCopyToastId));
  }
}

class ClipboardHistoryControllerShowSourceTest
    : public ClipboardHistoryControllerTest,
      public testing::WithParamInterface<
          std::tuple<ClipboardHistoryControllerShowSource,
                     /*refresh_enabled=*/bool>> {
 public:
  ClipboardHistoryControllerShowSourceTest() {
    std::vector<base::test::FeatureRef> refresh_features = {
        features::kClipboardHistoryRefresh, chromeos::features::kJelly};
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    (IsClipboardHistoryRefreshEnabled() ? enabled_features : disabled_features)
        .swap(refresh_features);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ClipboardHistoryControllerShowSource GetSource() const {
    return std::get<0>(GetParam());
  }

  bool IsClipboardHistoryRefreshEnabled() const {
    return std::get<1>(GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ClipboardHistoryControllerShowSourceTest,
    testing::Combine(testing::ValuesIn(GetClipboardHistoryShowSources()),
                     /*refresh_enabled=*/testing::Bool()));

// Tests that `ShowMenu()` returns whether the menu was shown successfully.
TEST_P(ClipboardHistoryControllerShowSourceTest, ShowMenuReturnsSuccess) {
  base::HistogramTester histogram_tester;

  // Try to show the menu without populating the clipboard. The menu should not
  // show.
  EXPECT_FALSE(GetClipboardHistoryController()->ShowMenu(
      gfx::Rect(), ui::MenuSourceType::MENU_SOURCE_NONE, GetSource()));
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ContextMenu.ShowMenu",
                                    /*expected_count=*/0);

  // Copy something to enable the clipboard history menu.
  WriteTextToClipboardAndConfirm(u"test");

  // Try to show the menu with the screen locked. The menu should not show.
  auto* session_controller = Shell::Get()->session_controller();
  session_controller->LockScreen();
  GetSessionControllerClient()->FlushForTest();
  EXPECT_TRUE(session_controller->IsScreenLocked());

  EXPECT_FALSE(GetClipboardHistoryController()->ShowMenu(
      gfx::Rect(), ui::MenuSourceType::MENU_SOURCE_NONE, GetSource()));
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ContextMenu.ShowMenu",
                                    /*expected_count=*/0);

  session_controller->HideLockScreen();
  GetSessionControllerClient()->FlushForTest();
  EXPECT_FALSE(session_controller->IsScreenLocked());

  // Show the menu.
  EXPECT_TRUE(GetClipboardHistoryController()->ShowMenu(
      gfx::Rect(), ui::MenuSourceType::MENU_SOURCE_NONE, GetSource()));
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectUniqueSample(
      "Ash.ClipboardHistory.ContextMenu.ShowMenu", GetSource(),
      /*expected_bucket_count=*/1);

  // Try to show the menu again without closing the active menu. The menu should
  // still be showing, but this attempt should fail.
  EXPECT_FALSE(GetClipboardHistoryController()->ShowMenu(
      gfx::Rect(), ui::MenuSourceType::MENU_SOURCE_NONE, GetSource()));
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectUniqueSample(
      "Ash.ClipboardHistory.ContextMenu.ShowMenu", GetSource(),
      /*expected_bucket_count=*/1);
}

// Tests that the client-provided `OnMenuClosingCallback` runs before the menu
// closes.
TEST_P(ClipboardHistoryControllerShowSourceTest, OnMenuClosingCallback) {
  if (IsClipboardHistoryRefreshEnabled()) {
    // TODO(b/267694484): Do not skip this test case once the menu can paste
    // with the refresh feature enabled.
    GTEST_SKIP();
  }

  base::test::RepeatingTestFuture<bool> on_menu_closing_future;
  base::HistogramTester histogram_tester;

  // Copy something to enable the clipboard history menu.
  WriteTextToClipboardAndConfirm(u"test");

  gfx::Rect test_window_rect(100, 100, 100, 100);
  std::unique_ptr<aura::Window> window(CreateTestWindow(test_window_rect));

  // Show the menu with an `OnMenuClosingCallback`.
  GetClipboardHistoryController()->ShowMenu(
      test_window_rect, ui::MenuSourceType::MENU_SOURCE_NONE, GetSource(),
      on_menu_closing_future.GetCallback());
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_TRUE(on_menu_closing_future.IsEmpty());

  // Hide the menu. The callback should indicate that nothing will be pasted.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_FALSE(on_menu_closing_future.Take());

  FlushMessageLoop();
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.PasteSource",
                                    /*expected_count=*/0);

  // Show the menu again.
  GetClipboardHistoryController()->ShowMenu(
      test_window_rect, ui::MenuSourceType::MENU_SOURCE_NONE, GetSource(),
      on_menu_closing_future.GetCallback());
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_TRUE(on_menu_closing_future.IsEmpty());

  // Toggle the menu closed. The callback should indicate a pending paste.
  PressAndReleaseKey(ui::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_TRUE(on_menu_closing_future.Take());

  FlushMessageLoop();
  histogram_tester.ExpectUniqueSample("Ash.ClipboardHistory.PasteSource",
                                      GetSource(),
                                      /*expected_bucket_count=*/1);
}

}  // namespace ash
