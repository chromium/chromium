// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/accessibility/spoken_feedback_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/browsertest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"

namespace ash {
namespace {

void SendKeyPressWithShiftAndControl(ui::KeyboardCode key) {
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, key, true, true, false, false)));
}

}  // namespace

enum SpokenFeedbackAppListTestVariant { kTestAsNormalUser, kTestAsGuestUser };

class TestSuggestionChipResult : public TestSearchResult {
 public:
  explicit TestSuggestionChipResult(const std::u16string& title) {
    set_display_type(SearchResultDisplayType::kChip);
    set_title(title);
  }
  ~TestSuggestionChipResult() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSuggestionChipResult);
};

class SpokenFeedbackAppListTest
    : public LoggedInSpokenFeedbackTest,
      public ::testing::WithParamInterface<SpokenFeedbackAppListTestVariant> {
 protected:
  SpokenFeedbackAppListTest() = default;
  ~SpokenFeedbackAppListTest() override = default;

  void SetUp() override {
    // Do not run expand arrow hinting animation to avoid msan test crash.
    // (See https://crbug.com/926038)
    AppListView::SetShortAnimationForTesting(true);
    LoggedInSpokenFeedbackTest::SetUp();
  }

  void TearDown() override {
    LoggedInSpokenFeedbackTest::TearDown();
    AppListView::SetShortAnimationForTesting(false);
  }

  void SetUpOnMainThread() override {
    LoggedInSpokenFeedbackTest::SetUpOnMainThread();
    auto* controller = Shell::Get()->app_list_controller();
    controller->SetAppListModelForTest(
        std::make_unique<test::AppListTestModel>());
    app_list_test_model_ =
        static_cast<test::AppListTestModel*>(controller->GetModel());
    search_model = controller->GetSearchModel();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam() == kTestAsGuestUser) {
      command_line->AppendSwitch(switches::kGuestSession);
      command_line->AppendSwitch(::switches::kIncognito);
      command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
      command_line->AppendSwitchASCII(
          switches::kLoginUser, user_manager::GuestAccountId().GetUserEmail());
    }
  }

  // Populate apps grid with |num| items.
  void PopulateApps(size_t num) { app_list_test_model_->PopulateApps(num); }

  // Populate |num| suggestion chips.
  void PopulateChips(size_t num) {
    for (size_t i = 0; i < num; i++) {
      search_model->results()->Add(std::make_unique<TestSuggestionChipResult>(
          base::UTF8ToUTF16("Chip " + base::NumberToString(i))));
    }
  }

  void ReadWindowTitle() {
    extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
        browser()->profile(), extension_misc::kChromeVoxExtensionId,
        "CommandHandler.onCommand('readCurrentTitle');");
  }

 private:
  test::AppListTestModel* app_list_test_model_ = nullptr;
  SearchModel* search_model = nullptr;
};

INSTANTIATE_TEST_SUITE_P(TestAsNormalAndGuestUser,
                         SpokenFeedbackAppListTest,
                         ::testing::Values(kTestAsNormalUser,
                                           kTestAsGuestUser));

class TabletModeSpokenFeedbackAppListTest : public SpokenFeedbackAppListTest {
 protected:
  TabletModeSpokenFeedbackAppListTest() = default;
  ~TabletModeSpokenFeedbackAppListTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SpokenFeedbackAppListTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAshEnableTabletMode);
  }

  void SetTabletMode(bool enabled) {
    ShellTestApi().SetTabletModeEnabledForTest(enabled);
  }

  bool IsTabletModeEnabled() const { return TabletMode::Get()->InTabletMode(); }
};

INSTANTIATE_TEST_SUITE_P(TestAsNormalAndGuestUser,
                         TabletModeSpokenFeedbackAppListTest,
                         ::testing::Values(kTestAsNormalUser,
                                           kTestAsGuestUser));

class NotificationSpokenFeedbackAppListTest : public SpokenFeedbackAppListTest {
 protected:
  NotificationSpokenFeedbackAppListTest() {
    scoped_features_.InitWithFeatures({::features::kNotificationIndicator}, {});
  }
  ~NotificationSpokenFeedbackAppListTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SpokenFeedbackAppListTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAshEnableTabletMode);
  }

  void SetNotificationBadgeForApp(const std::string& id, bool has_badge) {
    auto* model = Shell::Get()->app_list_controller()->GetModel();
    auto* item = model->FindItem(id);

    item->UpdateNotificationBadgeForTesting(has_badge);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

INSTANTIATE_TEST_SUITE_P(TestAsNormalAndGuestUser,
                         NotificationSpokenFeedbackAppListTest,
                         ::testing::Values(kTestAsNormalUser,
                                           kTestAsGuestUser));

// Checks that when an app list item with a notification badge is focused, an
// announcement is made that the item requests your attention.
IN_PROC_BROWSER_TEST_P(NotificationSpokenFeedbackAppListTest,
                       AppListItemNotificationBadgeAnnounced) {
  PopulateApps(1);
  SetNotificationBadgeForApp("Item 0", true);

  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeech("Shelf");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");
  // Move focus to expand all apps button.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_UP); });
  sm_.ExpectSpeech("Expand to all apps");
  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, all apps");

  // Move focus to 1st app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });

  // Check that the announcmenet for items with a notification badge occurs.
  sm_.ExpectSpeech("Item 0 requests your attention.");
  sm_.Replay();
}

// Checks that when a paused app list item is focused, an announcement 'Paused'
// is made.
IN_PROC_BROWSER_TEST_P(TabletModeSpokenFeedbackAppListTest,
                       AppListItemPausedAppAnnounced) {
  PopulateApps(1);
  Shell::Get()
      ->app_list_controller()
      ->GetModel()
      ->FindItem("Item 0")
      ->UpdateAppStatusForTesting(AppStatus::kPaused);

  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeech("Shelf");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");
  // Move focus to expand all apps button.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_UP); });
  sm_.ExpectSpeech("Expand to all apps");
  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, all apps");

  // Move focus to 1st app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });

  // Check that the announcmenet for items with a pause badge occurs.
  sm_.ExpectSpeech("Paused");
  sm_.Replay();
}

// Checks that when a blocked app list item is focused, an announcement
// 'Blocked' is made.
IN_PROC_BROWSER_TEST_P(TabletModeSpokenFeedbackAppListTest,
                       AppListItemBlockedAppAnnounced) {
  PopulateApps(1);
  Shell::Get()
      ->app_list_controller()
      ->GetModel()
      ->FindItem("Item 0")
      ->UpdateAppStatusForTesting(AppStatus::kBlocked);

  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeech("Shelf");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");
  // Move focus to expand all apps button.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_UP); });
  sm_.ExpectSpeech("Expand to all apps");
  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, all apps");

  // Move focus to 1st app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });

  // Check that the announcmenet for items with a block badge occurs.
  sm_.ExpectSpeech("Blocked");
  sm_.Replay();
}

// Checks that entering and exiting tablet mode with a browser window open does
// not generate an accessibility event.
IN_PROC_BROWSER_TEST_P(
    TabletModeSpokenFeedbackAppListTest,
    HiddenAppListDoesNotCreateAccessibilityEventWhenTransitioningToTabletMode) {
  EnableChromeVox();

  sm_.Call([this]() { EXPECT_FALSE(IsTabletModeEnabled()); });
  sm_.Call([this]() { SetTabletMode(true); });
  sm_.Call([this]() { EXPECT_TRUE(IsTabletModeEnabled()); });
  sm_.ExpectNextSpeechIsNot("Launcher, all apps");
  sm_.Call([this]() { SetTabletMode(false); });
  sm_.Call([this]() { EXPECT_FALSE(IsTabletModeEnabled()); });
  sm_.ExpectNextSpeechIsNot("Launcher, all apps");
  sm_.Replay();
}

// Checks that rotating the display in tablet mode does not generate an
// accessibility event.
IN_PROC_BROWSER_TEST_P(
    TabletModeSpokenFeedbackAppListTest,
    LauncherAppListScreenRotationDoesNotCreateAccessibilityEvent) {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const int display_id = display_manager->GetDisplayAt(0).id();
  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeech("Shelf");

  // Press space on the launcher button in shelf, this opens peeking launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");

  // Send a key press to enable keyboard traversal
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });

  // Move focus to expand all apps button.
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  sm_.ExpectSpeech("Expand to all apps");

  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, all apps");

  sm_.Call([this]() { EXPECT_FALSE(IsTabletModeEnabled()); });
  sm_.Call([this]() { SetTabletMode(true); });
  sm_.Call([this]() { EXPECT_TRUE(IsTabletModeEnabled()); });

  sm_.Call([this]() { browser()->window()->Minimize(); });
  // Set screen rotation to 90 degrees. No ChromeVox event should be created.
  sm_.Call([&, display_manager, display_id]() {
    display_manager->SetDisplayRotation(display_id, display::Display::ROTATE_90,
                                        display::Display::RotationSource::USER);
  });
  sm_.ExpectNextSpeechIsNot("Launcher, all apps");

  // Set screen rotation to 0 degrees. No ChromeVox event should be created.
  sm_.Call([&, display_manager, display_id]() {
    display_manager->SetDisplayRotation(display_id, display::Display::ROTATE_0,
                                        display::Display::RotationSource::USER);
  });
  sm_.ExpectNextSpeechIsNot("Launcher, all apps");

  sm_.Call([this]() { EXPECT_TRUE(IsTabletModeEnabled()); });
  sm_.Call([this]() { SetTabletMode(false); });
  sm_.Call([this]() { EXPECT_FALSE(IsTabletModeEnabled()); });
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest, LauncherStateTransition) {
  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeechPattern("Launcher");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Shelf");
  sm_.ExpectSpeech("Tool bar");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeechPattern("Search your device,*");
  sm_.ExpectSpeech("Edit text");
  // Check that Launcher, partial view state is announced.
  sm_.ExpectSpeech("Launcher, partial view");
  // Send a key press to enable keyboard traversal
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  // Move focus to expand all apps button;
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  sm_.ExpectSpeech("Expand to all apps");
  sm_.ExpectSpeech("Button");
  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech(
      "Search your device, apps, settings, and web."
      " Use the arrow keys to navigate your apps.");
  sm_.ExpectSpeech("Edit text");
  // Check that Launcher, all apps state is announced.
  sm_.ExpectSpeech("Launcher, all apps");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       DisabledFullscreenExpandButton) {
  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeech("Shelf");

  // Press space on the launcher button in shelf, this opens peeking launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");

  // Send a key press to enable keyboard traversal
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });

  // Move focus to expand all apps button.
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  sm_.ExpectSpeech("Expand to all apps");

  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, all apps");

  // Make sure the first traversal left is not the expand arrow button.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  sm_.ExpectNextSpeechIsNot("Expand to all apps");

  // Make sure the second traversal left is not the expand arrow button.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  sm_.ExpectNextSpeechIsNot("Expand to all apps");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       PeekingLauncherFocusTraversal) {
  // Add 3 suggestion chips.
  PopulateChips(3);

  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeech("Press Search plus Space to activate");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");
  // Move focus to 1st suggestion chip;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Chip 0");
  sm_.ExpectSpeech("Button");
  // Move focus to 2nd suggestion chip;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Chip 1");
  sm_.ExpectSpeech("Button");
  // Move focus to 3rd suggestion chip;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Chip 2");
  sm_.ExpectSpeech("Button");
  // Move focus to expand all apps button;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Expand to all apps");
  sm_.ExpectSpeech("Button");
  // Move focus to app list window;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeechPattern("Search your device,*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       FullscreenLauncherFocusTraversal) {
  // Add 1 suggestion chip and 3 apps.
  PopulateChips(1);
  PopulateApps(3);

  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeech("Press Search plus Space to activate");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");
  // Send a key press to enable keyboard traversal
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  // Move focus to expand all apps button.
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  sm_.ExpectSpeech("Press Search plus Space to activate");
  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, all apps");
  // Move focus to the suggestion chip;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Chip 0");
  sm_.ExpectSpeech("Button");
  // Move focus to 1st app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Item 0");
  sm_.ExpectSpeech("Button");
  // Move focus to 2nd app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Item 1");
  sm_.ExpectSpeech("Button");
  // Move focus to 3rd app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Item 2");
  sm_.ExpectSpeech("Button");
  // Move focus to app list window;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech(
      "Search your device, apps, settings, and web. Use the arrow keys to "
      "navigate your apps.");
  // Move focus to search box;
  sm_.ExpectSpeech("Edit text");
  sm_.Replay();
}

// Checks that app list keyboard foldering is announced.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest, AppListFoldering) {
  // Add 3 apps.
  PopulateApps(3);

  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeech("Shelf");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");
  // Move focus to expand all apps button.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_UP); });
  sm_.ExpectSpeech("Expand to all apps");
  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, all apps");

  // Move focus to 1st app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Item 0");
  sm_.ExpectSpeech("Button");

  // Combine items and create a new folder.
  sm_.Call([]() { SendKeyPressWithShiftAndControl(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Folder Unnamed");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Alert");
  sm_.ExpectSpeech("Item 0 combined with Item 1 to create new folder.");

  // Open the folder and move focus to the first item of the folder.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Item 1");
  sm_.ExpectSpeech("Button");

  // Remove the first item from the folder back to the top level app list.
  sm_.Call([]() { SendKeyPressWithShiftAndControl(ui::VKEY_LEFT); });
  sm_.ExpectSpeech("Item 1");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Alert");
  sm_.ExpectSpeech("Moved to Page 1, row 1, column 1.");

  sm_.Replay();
}

// Checks that app list keyboard reordering is announced.
// TODO(mmourgos): The current method of accessibility announcements for item
// reordering uses alerts, this works for spoken feedback but does not work as
// well for braille users. The preferred way to handle this is to actually
// change focus as the user navigates, and to have each object's
// accessible name describe its position. (See crbug.com/1098495)
IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest, AppListReordering) {
  // Add 7 apps.
  PopulateApps(22);

  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeech("Shelf");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");
  // Send a key press to enable keyboard traversal
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  // Move focus to expand all apps button.
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_TAB); });
  sm_.ExpectSpeech("Expand to all apps");
  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, all apps");

  // Move focus to first app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Item 0");
  sm_.ExpectSpeech("Button");

  // Move the first item to the right.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Alert");
  sm_.ExpectSpeech("Moved to Page 1, row 1, column 2.");

  // Move the focused item down.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_DOWN); });
  sm_.ExpectSpeech("Alert");
  sm_.ExpectSpeech("Moved to Page 1, row 2, column 2.");

  // Move the focused item down.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_DOWN); });
  sm_.ExpectSpeech("Alert");
  sm_.ExpectSpeech("Moved to Page 1, row 3, column 2.");

  // Move the focused item down.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_DOWN); });
  sm_.ExpectSpeech("Alert");
  sm_.ExpectSpeech("Moved to Page 1, row 4, column 2.");

  // Move the focused item down to page 2.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_DOWN); });
  sm_.ExpectSpeech("Alert");
  sm_.ExpectSpeech("Moved to Page 2, row 1, column 2.");

  // Move the focused item to the left.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_LEFT); });
  sm_.ExpectSpeech("Alert");
  sm_.ExpectSpeech("Moved to Page 2, row 1, column 1.");

  // Move the focused item back up to page 1..
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_UP); });
  sm_.ExpectSpeech("Alert");
  sm_.ExpectSpeech("Moved to Page 1, row 4, column 1.");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       LauncherWindowTitleAnnouncement) {
  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeechPattern("Launcher");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Shelf");
  sm_.ExpectSpeech("Tool bar");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeechPattern("Search your device,*");
  sm_.ExpectSpeech("Edit text");
  sm_.ExpectSpeech("Launcher, partial view");
  sm_.Call([this]() { ReadWindowTitle(); });
  sm_.ExpectSpeech("Launcher, partial view");
  // Move focus to expand all apps button.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_UP); });
  sm_.ExpectSpeech("Expand to all apps");
  // Press space on expand arrow to go to fullscreen launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeechPattern("Search your device,*");
  sm_.ExpectSpeech("Edit text");
  sm_.ExpectSpeech("Launcher, all apps");
  sm_.Call([this]() { ReadWindowTitle(); });
  sm_.ExpectSpeech("Launcher, all apps");
  // Activate the search widget.
  sm_.Call([this]() { SendKeyPress(ui::VKEY_A); });
  sm_.ExpectSpeechPattern("Displaying *");
  sm_.Call([this]() { ReadWindowTitle(); });
  sm_.ExpectSpeech("Launcher");
  sm_.Replay();
}

}  // namespace ash
