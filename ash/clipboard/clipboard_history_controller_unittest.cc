// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_controller_impl.h"

#include <memory>

#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/image_model.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();
}

void WriteToClipboard(const std::string& str) {
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(base::UTF8ToUTF16(str));
  }
  FlushMessageLoop();
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
              ImageModelCallback callback) override {
    std::move(callback).Run(ui::ImageModel());
  }

  void CancelRequest(const base::UnguessableToken& request_id) override {}

  void Activate() override {}

  void Deactivate() override {}

  void OnShutdown() override {}
};

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
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kClipboardHistory);
    AshTestBase::SetUp();
    mock_image_factory_ = std::make_unique<MockClipboardImageModelFactory>();
  }

  ClipboardHistoryControllerImpl* GetClipboardHistoryController() {
    return Shell::Get()->clipboard_history_controller();
  }

  void ShowMenu() {
    GetEventGenerator()->PressKey(ui::VKEY_V, ui::EF_COMMAND_DOWN);
    GetEventGenerator()->ReleaseKey(ui::VKEY_V, ui::EF_COMMAND_DOWN);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockClipboardImageModelFactory> mock_image_factory_;
};

// Tests that search + v with no history fails to show a menu.
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

// Tests that search + v shows a menu when there is something to show.
TEST_F(ClipboardHistoryControllerTest, MultiShowMenu) {
  base::HistogramTester histogram_tester;
  // Copy something to enable the clipboard history menu.
  WriteToClipboard("test");

  ShowMenu();

  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", 1, 1);
  // No UserJourneyTime should be recorded as the menu is still showing.
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 0);
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", 1);

  // Hide the menu.
  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, /*flags=*/0);
  GetEventGenerator()->ReleaseKey(ui::VKEY_ESCAPE, /*flags=*/0);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 1);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", 1);

  // Reshow the menu.
  ShowMenu();

  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", 1, 2);

  // No new UserJourneyTime histogram should be recorded as the menu is
  // still showing.
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 1);

  histogram_tester.ExpectBucketCount(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", 1, 2);
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", 2);
  // Hide the menu.
  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, /*flags=*/0);
  GetEventGenerator()->ReleaseKey(ui::VKEY_ESCAPE, /*flags=*/0);

  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());
  histogram_tester.ExpectTotalCount(
      "Ash.ClipboardHistory.ContextMenu.UserJourneyTime", 2);
}

// Verifies that the clipboard history is disabled in some user modes, which
// means that the clipboard history should not be recorded and meanwhile the
// menu view should not show (https://crbug.com/1100739).
TEST_F(ClipboardHistoryControllerTest, VerifyAvailabilityInUserModes) {
  // Write one item into the clipboard history.
  WriteToClipboard("text");

  constexpr struct {
    user_manager::UserType user_type;
    bool is_enabled;
  } kTestCases[] = {{user_manager::USER_TYPE_REGULAR, true},
                    {user_manager::USER_TYPE_GUEST, true},
                    {user_manager::USER_TYPE_PUBLIC_ACCOUNT, false},
                    {user_manager::USER_TYPE_SUPERVISED, true},
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
    WriteToClipboard("test");

    const std::list<ClipboardHistoryItem>& items =
        Shell::Get()->clipboard_history_controller()->history()->GetItems();
    if (test_case.is_enabled) {
      // Verify that the new item should be included in the clipboard history
      // and the menu should be able to show.
      EXPECT_EQ(2u, items.size());

      ShowMenu();

      EXPECT_TRUE(
          Shell::Get()->clipboard_history_controller()->IsMenuShowing());

      GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, /*flags=*/0);
      GetEventGenerator()->ReleaseKey(ui::VKEY_ESCAPE, /*flags=*/0);

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
      EXPECT_EQ(1u, items.size());

      ShowMenu();

      EXPECT_FALSE(
          Shell::Get()->clipboard_history_controller()->IsMenuShowing());
    }
  }
}

}  // namespace ash
