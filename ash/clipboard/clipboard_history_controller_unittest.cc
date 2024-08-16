// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/color_util.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"

namespace ash {
using crosapi::mojom::ClipboardHistoryControllerShowSource;

namespace {

// Matchers --------------------------------------------------------------------

MATCHER_P2(MenuItemsMatch, labels, icons, "") {
  if (arg.size() != labels.size() || arg.size() != icons.size()) {
    return false;
  }

  for (size_t index = 0; index < labels.size(); ++index) {
    if (arg[index].label != labels[index] ||
        !gfx::test::AreImagesEqual(arg[index].icon, icons[index])) {
      return false;
    }
  }

  return true;
}

// Helper classes --------------------------------------------------------------

// A mocked clipboard history controller observer.
class MockObserver : public ClipboardHistoryController::Observer {
 public:
  MockObserver() {
    scoped_observation_.Observe(ClipboardHistoryController::Get());
  }

  // ClipboardHistoryController::Observer:
  MOCK_METHOD(void, OnClipboardHistoryItemsUpdated, (), (override));

 private:
  base::ScopedObservation<ClipboardHistoryController,
                          ClipboardHistoryController::Observer>
      scoped_observation_{this};
};

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
    // Return a dummy image as the render result.
    std::move(callback).Run(
        ui::ImageModel::FromImageSkia(gfx::ImageSkia::CreateFrom1xBitmap(
            gfx::test::CreateBitmap(/*width=*/2, /*height=*/2))));
  }

  void CancelRequest(const base::UnguessableToken& request_id) override {}

  void Activate() override {}

  void Deactivate() override {}

  void RenderCurrentPendingRequests() override {}

  void OnShutdown() override {}
};

// Describes a menu item consisting of a label and an icon.
struct MenuItemDescriptor {
  MenuItemDescriptor(const std::u16string& input_label,
                     const gfx::Image& input_icon)
      : label(input_label), icon(input_icon) {}
  const std::u16string label;
  const gfx::Image icon;
};

// Helper functions ------------------------------------------------------------

void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

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
        operation_confirmed_future_.GetRepeatingCallback());
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
  base::test::TestFuture<bool> operation_confirmed_future_;

 private:
  std::unique_ptr<MockClipboardImageModelFactory> mock_image_factory_;
};

// Tests that Search + V with no history fails to show a menu.
TEST_F(ClipboardHistoryControllerTest, NoHistoryNoMenu) {
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

// Tests that Search + V shows a menu when there is something to show.
TEST_F(ClipboardHistoryControllerTest, ShowMenu) {
  base::HistogramTester histogram_tester;

  // Copy something to enable the clipboard history menu.
  WriteTextToClipboardAndConfirm(u"test");

  ShowMenu();

  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  // No user journey time should be recorded as the menu is still showing.
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 0);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", 1);

  // Hide the menu.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", 1);

  // Reshow the menu.
  ShowMenu();

  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());

  // No new UserJourneyTime histogram should be recorded as the menu is
  // still showing.
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", 1, 2);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", 2);

  // Hide the menu.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 2);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", 1, 2);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", 2);
}

// Verifies that the clipboard history is disabled in some user modes, which
// means that the clipboard history should not be recorded and meanwhile the
// menu view should not show (https://crbug.com/1100739).
TEST_F(ClipboardHistoryControllerTest, VerifyAvailabilityInUserModes) {
  // Write one item into the clipboard history.
  WriteTextToClipboardAndConfirm(u"text");

  constexpr struct {
    user_manager::UserType user_type;
    bool is_enabled;
  } kTestCases[] = {{user_manager::UserType::kRegular, true},
                    {user_manager::UserType::kGuest, true},
                    {user_manager::UserType::kPublicAccount, false},
                    {user_manager::UserType::kKioskApp, false},
                    {user_manager::UserType::kChild, true},
                    {user_manager::UserType::kWebKioskApp, false}};

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
      EXPECT_FALSE(operation_confirmed_future_.IsReady());
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
TEST_F(ClipboardHistoryControllerTest, DisableInUserAddingScreen) {
  WriteTextToClipboardAndConfirm(u"text");

  // Emulate that the user adding screen displays.
  Shell::Get()->session_controller()->ShowMultiProfileLogin();

  // Try to show the clipboard history menu; verify that the menu does not show.
  ShowMenu();
  EXPECT_FALSE(Shell::Get()->clipboard_history_controller()->IsMenuShowing());
}

// Tests that pressing Search while holding the V key does not show the
// Launcher.
TEST_F(ClipboardHistoryControllerTest, VThenSearchDoesNotShowLauncher) {
  GetEventGenerator()->PressKey(ui::VKEY_V, ui::EF_NONE);
  GetEventGenerator()->PressKey(ui::VKEY_COMMAND, ui::EF_NONE);

  // Release V, which could trigger a key released accelerator.
  GetEventGenerator()->ReleaseKey(ui::VKEY_V, ui::EF_COMMAND_DOWN);

  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible(
      /*display_id=*/std::nullopt));

  // Release Search, which could trigger the Launcher.
  GetEventGenerator()->ReleaseKey(ui::VKEY_COMMAND, ui::EF_NONE);

  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible(
      /*display_id=*/std::nullopt));
}

// Tests that clearing a single item from the clipboard clears clipboard
// history.
TEST_F(ClipboardHistoryControllerTest, ClearClipboardClearsHistory) {
  // Write a single item to the clipboard.
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

// Tests that clearing the clipboard closes the clipboard history menu.
TEST_F(ClipboardHistoryControllerTest,
       ClearingClipboardClosesClipboardHistory) {
  // Write a single item to the clipboard.
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

TEST_F(ClipboardHistoryControllerTest, EncodeImage) {
  // Write a bitmap to the clipboard.
  SkBitmap test_bitmap = gfx::test::CreateBitmap(3, 2);
  WriteImageToClipboardAndConfirm(test_bitmap);

  // The bitmap should be encoded to a PNG. Manually pry into the contents of
  // the result to confirm that the newly-encoded PNG is included.
  auto result = GetHistoryValues();
  ASSERT_EQ(result.size(), 1u);

  ExpectHistoryItemImageMatchesBitmap(result[0], test_bitmap);
}

TEST_F(ClipboardHistoryControllerTest, EncodeMultipleImages) {
  // Write a bunch of bitmaps to the clipboard.
  const std::vector<SkBitmap> test_bitmaps{
      gfx::test::CreateBitmap(2, 1),
      gfx::test::CreateBitmap(3, 2),
      gfx::test::CreateBitmap(4, 3),
  };
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
  // Write a bitmap to the clipboard.
  const std::vector<SkBitmap> test_bitmaps{
      gfx::test::CreateBitmap(3, 2),
      gfx::test::CreateBitmap(4, 3),
  };
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
  // Write text to the clipboard and verify that it can be retrieved.
  WriteTextToClipboardAndConfirm(u"test");
  auto history_list = GetHistoryValues();
  ASSERT_EQ(history_list.size(), 1u);
  ASSERT_EQ(history_list[0].display_text(), u"test");

  TestEnteringLockScreen();
}

TEST_F(ClipboardHistoryControllerTest, LockedScreenImage) {
  // Write a bitmap to the clipboard and verify that it can be returned.
  SkBitmap test_bitmap = gfx::test::CreateBitmap(3, 2);
  WriteImageToClipboardAndConfirm(test_bitmap);
  auto result = GetHistoryValues();
  ASSERT_EQ(result.size(), 1u);
  ExpectHistoryItemImageMatchesBitmap(result[0], test_bitmap);

  TestEnteringLockScreen();
}

using ClipboardHistoryControllerObserverTest = ClipboardHistoryControllerTest;

// Verifies that clipboard history controller notifies observers of clipboard
// history item updates as expected when adding or removing items.
TEST_F(ClipboardHistoryControllerObserverTest, AddAndRemoveItem) {
  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnClipboardHistoryItemsUpdated).Times(3);
  WriteTextToClipboardAndConfirm(u"A");
  WriteTextToClipboardAndConfirm(u"B");
  ClipboardHistoryController::Get()->DeleteClipboardItemById(
      GetHistoryValues()[0].id().ToString());
  GetClipboardHistoryController()->FireItemUpdateNotificationTimerForTest();
}

// Verifies that when the clipboard history is cleared, the controller notifies
// observers of clipboard history item updates as expected when removing items.
TEST_F(ClipboardHistoryControllerObserverTest, ClearHistory) {
  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnClipboardHistoryItemsUpdated).Times(3);
  WriteTextToClipboardAndConfirm(u"A");
  WriteTextToClipboardAndConfirm(u"B");

  // Clear the system clipboard, which causes clipboard history to clear.
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  GetClipboardHistoryController()->FireItemUpdateNotificationTimerForTest();
}

// Verifies that clipboard history controller notifies observers once when
// clipboard history item addition causes overflow.
TEST_F(ClipboardHistoryControllerObserverTest, Overflow) {
  // Add five items to reach the clipboard history size limit.
  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnClipboardHistoryItemsUpdated).Times(5);
  WriteTextToClipboardAndConfirm(u"A");
  WriteTextToClipboardAndConfirm(u"B");
  WriteTextToClipboardAndConfirm(u"C");
  WriteTextToClipboardAndConfirm(u"D");
  WriteTextToClipboardAndConfirm(u"E");
  EXPECT_EQ(GetHistoryValues().size(),
            static_cast<size_t>(clipboard_history_util::kMaxClipboardItems));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Notify `mock_observer` once when item addition causes overflow.
  EXPECT_CALL(mock_observer, OnClipboardHistoryItemsUpdated);
  WriteTextToClipboardAndConfirm(u"F");
  EXPECT_EQ(GetHistoryValues().size(),
            static_cast<size_t>(clipboard_history_util::kMaxClipboardItems));
}

TEST_F(ClipboardHistoryControllerObserverTest,
       ChangeSessionStateWithEmptyHistory) {
  // Clipboard history is empty. Therefore, the clipboard history controller
  // should not notify observers when the session state changes.
  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnClipboardHistoryItemsUpdated).Times(0);
  TestSessionControllerClient* test_session_client =
      GetSessionControllerClient();
  test_session_client->SetSessionState(session_manager::SessionState::LOCKED);
  test_session_client->FlushForTest();
  test_session_client->SetSessionState(session_manager::SessionState::ACTIVE);
  test_session_client->FlushForTest();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Notify `mock_observer` when a new clipboard history item arrives.
  EXPECT_CALL(mock_observer, OnClipboardHistoryItemsUpdated);
  WriteTextToClipboardAndConfirm(u"A");
}

// TODO(crbug.com/40274291): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ChangeSessionStateWithNonEmptyHistory \
  DISABLED_ChangeSessionStateWithNonEmptyHistory
#else
#define MAYBE_ChangeSessionStateWithNonEmptyHistory \
  ChangeSessionStateWithNonEmptyHistory
#endif
TEST_F(ClipboardHistoryControllerObserverTest,
       MAYBE_ChangeSessionStateWithNonEmptyHistory) {
  // Notify `mock_observer` once when adding a clipboard history item.
  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnClipboardHistoryItemsUpdated);
  WriteTextToClipboardAndConfirm(u"A");
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Notify `mock_observer` once when clipboard history becomes disabled.
  EXPECT_CALL(mock_observer, OnClipboardHistoryItemsUpdated);
  TestSessionControllerClient* test_session_client =
      GetSessionControllerClient();
  test_session_client->SetSessionState(session_manager::SessionState::LOCKED);
  test_session_client->FlushForTest();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Do not notify `mock_observer` when switching to another session state where
  // clipboard history is still disabled.
  EXPECT_CALL(mock_observer, OnClipboardHistoryItemsUpdated).Times(0);
  test_session_client->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  test_session_client->FlushForTest();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Notify `mock_observer` once when clipboard history becomes enabled.
  EXPECT_CALL(mock_observer, OnClipboardHistoryItemsUpdated);
  test_session_client->SetSessionState(session_manager::SessionState::ACTIVE);
  test_session_client->FlushForTest();
}

class ClipboardHistoryControllerWithTextfieldTest
    : public ClipboardHistoryControllerTest {
 public:
  // ClipboardHistoryControllerTest:
  void SetUp() override {
    ClipboardHistoryControllerTest::SetUp();

    textfield_widget_ = CreateFramelessTestWidget();
    textfield_widget_->SetBounds(gfx::Rect(0, 0, 100, 100));
    textfield_ = textfield_widget_->SetContentsView(
        std::make_unique<views::Textfield>());
    textfield_->GetViewAccessibility().SetName(u"Textfield");
    textfield_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

    // Focus the textfield and confirm initial state.
    textfield_->RequestFocus();
    ASSERT_TRUE(textfield_->HasFocus());
    ASSERT_TRUE(textfield_->GetText().empty());
  }

  void ShowTextfieldContextMenu(const views::View& textfield) {
    GetEventGenerator()->MoveMouseTo(
        textfield.GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickRightButton();
  }

  std::unique_ptr<views::Widget> textfield_widget_;
  raw_ptr<views::Textfield> textfield_;
};

TEST_F(ClipboardHistoryControllerWithTextfieldTest, PasteClipboardItemById) {
  // Write four items to the clipboard.
  WriteTextToClipboardAndConfirm(u"A");
  WriteTextToClipboardAndConfirm(u"B");
  WriteTextToClipboardAndConfirm(u"C");
  WriteTextToClipboardAndConfirm(u"D");
  WriteTextToClipboardAndConfirm(u"E");
  const std::vector<ClipboardHistoryItem> items = GetHistoryValues();
  ASSERT_EQ(items.size(), 5u);

  // Set a zero duration to make test code simpler.
  GetClipboardHistoryController()->set_buffer_restoration_delay_for_test(
      base::TimeDelta());

  struct {
    size_t paste_data_index;
    crosapi::mojom::ClipboardHistoryControllerShowSource paste_source;
    int event_flags;
    ClipboardHistoryControllerImpl::ClipboardHistoryPasteType paste_type;
  } test_cases[] = {
      {/*paste_data_index=*/0,
       /*paste_source=*/
       crosapi::mojom::ClipboardHistoryControllerShowSource::kVirtualKeyboard,
       /*event_flags=*/ui::EF_NONE,
       /*paste_type=*/
       ClipboardHistoryControllerImpl::ClipboardHistoryPasteType::
           kRichTextVirtualKeyboard},
      {/*paste_data_index=*/1,
       /*paste_source=*/
       crosapi::mojom::ClipboardHistoryControllerShowSource::
           kTextfieldContextMenu,
       /*event_flags=*/ui::EF_MOUSE_BUTTON,
       /*paste_type=*/
       ClipboardHistoryControllerImpl::ClipboardHistoryPasteType::
           kRichTextMouse},
      {/*paste_data_index=*/2,
       /*paste_source=*/
       crosapi::mojom::ClipboardHistoryControllerShowSource::
           kRenderViewContextMenu,
       /*event_flags=*/ui::EF_SHIFT_DOWN | ui::EF_FROM_TOUCH,
       /*paste_type=*/
       ClipboardHistoryControllerImpl::ClipboardHistoryPasteType::
           kPlainTextTouch},
      {/*paste_data_index=*/3,
       /*paste_source=*/
       crosapi::mojom::ClipboardHistoryControllerShowSource::
           kRenderViewContextSubmenu,
       /*event_flags=*/ui::EF_MOUSE_BUTTON,
       /*paste_type=*/
       ClipboardHistoryControllerImpl::ClipboardHistoryPasteType::
           kRichTextMouse},
      {/*paste_data_index=*/4,
       /*paste_source=*/
       crosapi::mojom::ClipboardHistoryControllerShowSource::
           kTextfieldContextSubmenu,
       /*event_flags=*/ui::EF_MOUSE_BUTTON,
       /*paste_type=*/
       ClipboardHistoryControllerImpl::ClipboardHistoryPasteType::
           kRichTextMouse}};

  for (auto& [paste_data_index, paste_source, event_flags, paste_type] :
       test_cases) {
    base::HistogramTester histogram_tester;
    textfield_->SetText(std::u16string());
    ClipboardHistoryController::Get()->PasteClipboardItemById(
        items[paste_data_index].id().ToString(), event_flags, paste_source);
    base::RunLoop().RunUntilIdle();

    // Verify the contents of `textfield_` and histograms.
    EXPECT_EQ(textfield_->GetText(), items[paste_data_index].display_text());
    histogram_tester.ExpectBucketCount("Ash.ClipboardHistory.PasteType",
                                       paste_type,
                                       /*expected_count=*/1);
    histogram_tester.ExpectBucketCount("Ash.ClipboardHistory.PasteSource",
                                       paste_source,
                                       /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Ash.ClipboardHistory.ContextMenu.MenuOptionSelected",
        paste_data_index + 1, /*expected_count=*/1);
  }
}

// Base class for tests that exercise clipboard history controller behavior with
// every possible way of showing the clipboard history menu. The
// `kClipboardHistoryLongpress` feature is enabled iff the menu is shown via
// Ctrl+V longpress.
class ClipboardHistoryControllerShowSourceTest
    : public ClipboardHistoryControllerTest,
      public testing::WithParamInterface<ClipboardHistoryControllerShowSource> {
 public:
  ClipboardHistoryControllerShowSourceTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kClipboardHistoryLongpress,
        GetSource() ==
            ClipboardHistoryControllerShowSource::kControlVLongpress);
  }

  ClipboardHistoryControllerShowSource GetSource() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardHistoryControllerShowSourceTest,
                         testing::ValuesIn(GetClipboardHistoryShowSources()));

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
  base::test::TestFuture<bool> on_menu_closing_future;
  base::HistogramTester histogram_tester;

  // Copy something to enable the clipboard history menu.
  WriteTextToClipboardAndConfirm(u"test");

  gfx::Rect test_window_rect(100, 100, 100, 100);
  std::unique_ptr<aura::Window> window(CreateTestWindow(test_window_rect));

  // Show the menu with an `OnMenuClosingCallback`.
  GetClipboardHistoryController()->ShowMenu(
      test_window_rect, ui::MenuSourceType::MENU_SOURCE_NONE, GetSource(),
      on_menu_closing_future.GetRepeatingCallback());
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_FALSE(on_menu_closing_future.IsReady());

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
  EXPECT_FALSE(on_menu_closing_future.IsReady());

  // Toggle the menu closed. The callback should indicate a pending paste.
  PressAndReleaseKey(ui::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  EXPECT_TRUE(on_menu_closing_future.Take());

  FlushMessageLoop();
  histogram_tester.ExpectUniqueSample("Ash.ClipboardHistory.PasteSource",
                                      GetSource(),
                                      /*expected_bucket_count=*/1);
}

// TODO(http://b/278109818): Move clipboard history refresh tests into a
// separate test file.

// A parameterized test base to verify the clipboard history refresh feature on
// every display format.
// Each test param is such a tuple:
// 1. The first value is a boolean indicating whether the clipboard history
// refresh feature is enabled;
// 2. The second value is the display format under test.
class ClipboardHistoryRefreshDisplayFormatTest
    : public ClipboardHistoryControllerWithTextfieldTest,
      public testing::WithParamInterface<
          std::tuple</*enable_clipboard_history_refresh=*/bool,
                     /*display_format_under_test=*/crosapi::mojom::
                         ClipboardHistoryDisplayFormat>> {
 public:
  ClipboardHistoryRefreshDisplayFormatTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{chromeos::features::kClipboardHistoryRefresh,
          IsClipboardHistoryRefreshEnabled()},
         {chromeos::features::kJelly, IsClipboardHistoryRefreshEnabled()}});
  }

  bool IsClipboardHistoryRefreshEnabled() const {
    return std::get<0>(GetParam());
  }

  // Writes clipboard data. Returns the the descriptors of the expected
  // clipboard history submenu items. The returned arrays follow the reverse
  // clipboard data writing order. Returns an empty array if the clipboard
  // history refresh feature is disabled.
  std::vector<MenuItemDescriptor> WriteClipboardDataBasedOnParam() {
    const ui::ColorProvider* color_provider = GetPrimaryWindowColorProvider();
    CHECK(color_provider);
    auto get_icon = [color_provider](const gfx::VectorIcon& icon) {
      return gfx::Image(ui::ImageModel::FromVectorIcon(icon,
                                                       ui::kColorSysSecondary,
                                                       /*icon_size=*/20)
                            .Rasterize(color_provider));
    };

    const bool refresh_feature_enabled =
        chromeos::features::IsClipboardHistoryRefreshEnabled();
    const std::u16string show_clipboard_menu_label =
        l10n_util::GetStringUTF16(IDS_APP_SHOW_CLIPBOARD_HISTORY);
    switch (GetDisplayFormat()) {
      case crosapi::mojom::ClipboardHistoryDisplayFormat::kText:
        WriteTextToClipboardAndConfirm(u"A");
        WriteTextToClipboardAndConfirm(u"B");
        WriteTextToClipboardAndConfirm(u"https://google.com/");
        if (refresh_feature_enabled) {
          return {{u"https://google.com/", get_icon(vector_icons::kLinkIcon)},
                  {u"B", get_icon(chromeos::kTextIcon)},
                  {u"A", get_icon(chromeos::kTextIcon)},
                  {show_clipboard_menu_label, gfx::Image()}};
        }
        break;
      case crosapi::mojom::ClipboardHistoryDisplayFormat::kPng:
        WriteImageToClipboardAndConfirm(
            gfx::test::CreateBitmap(/*width=*/3, /*height=*/3));
        WriteImageToClipboardAndConfirm(
            gfx::test::CreateBitmap(/*width=*/2, /*height=*/2));
        if (refresh_feature_enabled) {
          return {{u"Image", get_icon(chromeos::kFiletypeImageIcon)},
                  {u"Image", get_icon(chromeos::kFiletypeImageIcon)},
                  {show_clipboard_menu_label, gfx::Image()}};
        }
        break;
      case crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml:
        WriteHtmlAndConfirm("<table>A</table>");
        WriteHtmlAndConfirm("<table>B></table>");
        if (refresh_feature_enabled) {
          return {{u"HTML Content", get_icon(vector_icons::kCodeIcon)},
                  {u"HTML Content", get_icon(vector_icons::kCodeIcon)},
                  {show_clipboard_menu_label, gfx::Image()}};
        }
        break;
      case crosapi::mojom::ClipboardHistoryDisplayFormat::kFile:
        // Use dummy file paths. The corresponding files do not have to exist
        // because only file extensions are required to calculate icons.

        // Copy a single file.
        WriteFilePathsAndConfirm({u"dummy_file.webm"});

        // Copy multiple files at the same time.
        WriteFilePathsAndConfirm({u"dummy_child1.jpg", u"dummy_child2.png"});

        if (refresh_feature_enabled) {
          return {{u"2 files", get_icon(vector_icons::kContentCopyIcon)},
                  {u"dummy_file.webm", get_icon(chromeos::kFiletypeVideoIcon)},
                  {show_clipboard_menu_label, gfx::Image()}};
        }
        break;
      case crosapi::mojom::ClipboardHistoryDisplayFormat::kUnknown:
        NOTREACHED();
    }

    return {};
  }

  void WriteFilePathsAndConfirm(const std::vector<std::u16string>& file_paths) {
    {
      base::Pickle pickle;
      ui::WriteCustomDataToPickle(
          std::unordered_map<std::u16string, std::u16string>(
              {{u"fs/sources", base::JoinString(file_paths, u"\n")}}),
          &pickle);
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WritePickledData(pickle,
                           ui::ClipboardFormatType::DataTransferCustomType());
    }

    WaitForOperationConfirmed();
  }

  void WriteHtmlAndConfirm(const std::string& html) {
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteHTML(base::UTF8ToUTF16(html), /*source_url=*/"");
    }

    WaitForOperationConfirmed();
  }

  crosapi::mojom::ClipboardHistoryDisplayFormat GetDisplayFormat() const {
    return std::get<1>(GetParam());
  }

  const ui::ColorProvider* GetPrimaryWindowColorProvider() {
    auto* color_provider_source = ColorUtil::GetColorProviderSourceForWindow(
        Shell::GetPrimaryRootWindow());
    auto* color_provider = color_provider_source->GetColorProvider();
    return color_provider;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ClipboardHistoryRefreshDisplayFormatTest,
    testing::Combine(
        /*enable_clipboard_history_refresh=*/testing::Bool(),
        /*display_format_under_test=*/testing::Values(
            crosapi::mojom::ClipboardHistoryDisplayFormat::kText,
            crosapi::mojom::ClipboardHistoryDisplayFormat::kPng,
            crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml,
            crosapi::mojom::ClipboardHistoryDisplayFormat::kFile)));

// Verifies that the clipboard history submenu model of the text services
// context menu in Ash works as expected.
TEST_P(ClipboardHistoryRefreshDisplayFormatTest, TextServicesSubMenu) {
  // Show the textfield context menu before writing any clipboard data.
  ShowTextfieldContextMenu(*textfield_);

  views::TextfieldTestApi api(textfield_);
  ui::MenuModel* const root_model = api.context_menu_contents();
  ASSERT_TRUE(root_model);

  const bool is_refresh_enabled =
      chromeos::features::IsClipboardHistoryRefreshEnabled();
  const int clipboard_history_command_id = is_refresh_enabled
                                               ? IDS_APP_PASTE_FROM_CLIPBOARD
                                               : IDS_APP_SHOW_CLIPBOARD_HISTORY;

  // Search the parent model and the command index of
  // `clipboard_history_command_id`.
  ui::MenuModel* target_command_parent_model = root_model;
  size_t target_command_index = 0u;
  ui::MenuModel::GetModelAndIndexForCommandId(clipboard_history_command_id,
                                              &target_command_parent_model,
                                              &target_command_index);
  EXPECT_EQ(target_command_parent_model, root_model);
  EXPECT_GT(target_command_index, 0u);

  // The clipboard history menu item should be disabled when there is no
  // clipboard history.
  EXPECT_FALSE(target_command_parent_model->IsEnabledAt(target_command_index));

  // Write clipboard data.
  const std::vector<MenuItemDescriptor> expected_submenu_items =
      WriteClipboardDataBasedOnParam();
  ASSERT_EQ(expected_submenu_items.empty(), !is_refresh_enabled);

  // Close the textfield menu then reshow.
  GetEventGenerator()->PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  ShowTextfieldContextMenu(*textfield_);

  // Check `submenu_model` if any. Reuse `target_command_index` since the
  // context menu model structure should not change.
  target_command_parent_model = api.context_menu_contents();
  ui::MenuModel* const submenu_model =
      target_command_parent_model->GetSubmenuModelAt(target_command_index);

  // The clipboard history menu item should be enabled when there is clipboard
  // history.
  EXPECT_TRUE(target_command_parent_model->IsEnabledAt(target_command_index));

  if (is_refresh_enabled) {
    // If the refresh feature is enabled, the clipboard history menu item is a
    // submenu item.
    EXPECT_EQ(target_command_parent_model->GetTypeAt(target_command_index),
              ui::MenuModel::ItemType::TYPE_SUBMENU);
    ASSERT_TRUE(submenu_model);

    // Get the labels and icons from `submenu_model`. If a menu item does not
    // have an icon, add an empty image to `actual_icons`.
    const ui::ColorProvider* color_provider = GetPrimaryWindowColorProvider();
    std::vector<std::u16string> actual_labels;
    std::vector<gfx::Image> actual_icons;
    for (size_t index = 0; index < submenu_model->GetItemCount(); ++index) {
      actual_labels.emplace_back(submenu_model->GetLabelAt(index));
      const ui::ImageModel image_model = submenu_model->GetIconAt(index);
      actual_icons.push_back(
          image_model.IsEmpty()
              ? gfx::Image()
              : gfx::Image(image_model.Rasterize(color_provider)));
    }

    // Check the actual labels and icons.
    EXPECT_THAT(expected_submenu_items,
                MenuItemsMatch(actual_labels, actual_icons));
  } else {
    // If the refresh feature is disabled, the clipboard history menu item is a
    // command item.
    EXPECT_FALSE(submenu_model);
    EXPECT_EQ(target_command_parent_model->GetTypeAt(target_command_index),
              ui::MenuModel::ItemType::TYPE_COMMAND);
  }
}

TEST_P(ClipboardHistoryRefreshDisplayFormatTest,
       ShowStandaloneMenuFromSubmenu) {
  WriteClipboardDataBasedOnParam();
  ShowTextfieldContextMenu(*textfield_);

  // If the clipboard history refresh feature is enabled, show the submenu.
  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    // Expect the menu item that hosts the clipboard history submenu exists.
    const views::MenuItemView* const submenu_item = WaitForMenuItemWithLabel(
        l10n_util::GetStringUTF16(IDS_APP_PASTE_FROM_CLIPBOARD));
    ASSERT_TRUE(submenu_item);

    // Mouse hover on `submenu_item`. Wait until the submenu shows.
    base::HistogramTester submenu_histogram_tester;
    GetEventGenerator()->MoveMouseTo(
        submenu_item->GetBoundsInScreen().CenterPoint());
    views::View* const submenu_view = submenu_item->GetSubmenu();
    ViewDrawnWaiter().Wait(submenu_view);

    // Verify that the submenu source is recorded as expected when
    // `submenu_view` shows.
    submenu_histogram_tester.ExpectUniqueSample(
        "Ash.ClipboardHistory.ContextMenu.ShowMenu",
        crosapi::mojom::ClipboardHistoryControllerShowSource::
            kTextfieldContextSubmenu,
        1);
  }

  // Expect that the menu option to launch the clipboard history menu exists.
  const views::View* const menu_item = WaitForMenuItemWithLabel(
      l10n_util::GetStringUTF16(IDS_APP_SHOW_CLIPBOARD_HISTORY));
  ASSERT_TRUE(menu_item);

  // Left mouse click at `menu_item`. The standalone clipboard history menu
  // should show.
  base::HistogramTester histogram_tester;
  GetEventGenerator()->MoveMouseTo(
      menu_item->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(Shell::Get()->clipboard_history_controller()->IsMenuShowing());

  // The source of the standalone clipboard history menu should be recorded.
  histogram_tester.ExpectUniqueSample(
      "Ash.ClipboardHistory.ContextMenu.ShowMenu",
      crosapi::mojom::ClipboardHistoryControllerShowSource::
          kTextfieldContextMenu,
      1);
}

}  // namespace ash
