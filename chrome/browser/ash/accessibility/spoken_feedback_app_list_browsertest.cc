// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/accessibility/spoken_feedback_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
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
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
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

class SpokenFeedbackAppListTest
    : public LoggedInSpokenFeedbackTest,
      public ::testing::WithParamInterface<SpokenFeedbackAppListTestVariant> {
 protected:
  SpokenFeedbackAppListTest() = default;
  ~SpokenFeedbackAppListTest() override = default;

  // LoggedInSpokenFeedbackTest:
  void SetUp() override {
    // Do not run expand arrow hinting animation to avoid msan test crash.
    // (See https://crbug.com/926038)
    zero_duration_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    LoggedInSpokenFeedbackTest::SetUp();
  }

  void TearDown() override {
    LoggedInSpokenFeedbackTest::TearDown();
    zero_duration_mode_.reset();
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

  void SetUpOnMainThread() override {
    LoggedInSpokenFeedbackTest::SetUpOnMainThread();
    AppListClientImpl::GetInstance()->UpdateProfile();
  }

  // Populate apps grid with |num| items.
  void PopulateApps(size_t num) {
    // Only folders or page breaks are allowed to be added from the Ash side.
    // Therefore new apps should be added through `ChromeAppListModelUpdater`.
    ::test::PopulateDummyAppListItems(num);
  }

  std::vector<std::string> GetPublishedSuggestionChips() {
    std::vector<std::string> chips;
    std::vector<ChromeSearchResult*> published_results =
        AppListClientImpl::GetInstance()
            ->GetModelUpdaterForTest()
            ->GetPublishedSearchResultsForTest();
    for (auto* result : published_results) {
      if (result->display_type() == SearchResultDisplayType::kChip)
        chips.push_back(base::UTF16ToUTF8(result->title()));
    }
    return chips;
  }

  void ReadWindowTitle() {
    extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
        browser()->profile(), extension_misc::kChromeVoxExtensionId,
        "CommandHandler.onCommand('readCurrentTitle');");
  }

  AppListItem* FindItemByName(const std::string& name, int* index) {
    AppListModel* const model = AppListModelProvider::Get()->model();
    AppListItemList* item_list = model->top_level_item_list();
    for (int i = 0; i < item_list->item_count(); ++i) {
      if (item_list->item_at(i)->name() == name) {
        if (index)
          *index = i;
        return item_list->item_at(i);
      }
    }
    return nullptr;
  }

 private:
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;
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
  NotificationSpokenFeedbackAppListTest() = default;
  ~NotificationSpokenFeedbackAppListTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SpokenFeedbackAppListTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAshEnableTabletMode);
  }
};

INSTANTIATE_TEST_SUITE_P(TestAsNormalAndGuestUser,
                         NotificationSpokenFeedbackAppListTest,
                         ::testing::Values(kTestAsNormalUser,
                                           kTestAsGuestUser));

// Tests with feature ProductivityLauncher enabled.
class SpokenFeedbackAppListProductivityLauncherTest
    : public SpokenFeedbackAppListTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      ash::features::kProductivityLauncher};
};

INSTANTIATE_TEST_SUITE_P(TestAsNormalAndGuestUser,
                         SpokenFeedbackAppListProductivityLauncherTest,
                         ::testing::Values(kTestAsNormalUser,
                                           kTestAsGuestUser));

// Checks that when an app list item with a notification badge is focused, an
// announcement is made that the item requests your attention.
IN_PROC_BROWSER_TEST_P(NotificationSpokenFeedbackAppListTest,
                       AppListItemNotificationBadgeAnnounced) {
  PopulateApps(1);

  std::vector<std::string> suggestion_chips = GetPublishedSuggestionChips();

  int test_item_index = 0;
  ash::AppListItem* test_item = FindItemByName("app 0", &test_item_index);
  ASSERT_TRUE(test_item);
  test_item->UpdateNotificationBadgeForTesting(true);

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

  // Move focus over suggestion chip views.
  for (auto& chip : suggestion_chips) {
    sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
    sm_.ExpectSpeech(chip);
    sm_.ExpectSpeech("Button");
  }

  // Skip over apps that were installed before the test item.
  sm_.Call([this, &test_item_index]() {
    for (int i = 0; i < test_item_index + 1; ++i)
      SendKeyPressWithSearch(ui::VKEY_RIGHT);
  });

  // Check that the announcmenet for items with a notification badge occurs.
  sm_.ExpectSpeech("app 0 requests your attention.");
  sm_.Replay();
}

// Checks that when a paused app list item is focused, an announcement 'Paused'
// is made.
IN_PROC_BROWSER_TEST_P(TabletModeSpokenFeedbackAppListTest,
                       AppListItemPausedAppAnnounced) {
  PopulateApps(1);

  int test_item_index = 0;
  ash::AppListItem* test_item = FindItemByName("app 0", &test_item_index);
  ASSERT_TRUE(test_item);
  test_item->UpdateAppStatusForTesting(AppStatus::kPaused);

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

  // Skip suggestion chips if any are shown.
  if (!GetPublishedSuggestionChips().empty())
    sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });

  // Move focus to the first app.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });

  // Skip over apps that were installed before the test item.
  sm_.Call([this, &test_item_index]() {
    for (int i = 0; i < test_item_index; ++i)
      SendKeyPressWithSearch(ui::VKEY_RIGHT);
  });

  // Check that the announcmenet for items with a pause badge occurs.
  sm_.ExpectSpeech("app 0");
  sm_.ExpectSpeech("Paused");
  sm_.Replay();
}

// Checks that when a blocked app list item is focused, an announcement
// 'Blocked' is made.
IN_PROC_BROWSER_TEST_P(TabletModeSpokenFeedbackAppListTest,
                       AppListItemBlockedAppAnnounced) {
  PopulateApps(1);

  int test_item_index = 0;
  ash::AppListItem* test_item = FindItemByName("app 0", &test_item_index);
  ASSERT_TRUE(test_item);
  test_item->UpdateAppStatusForTesting(AppStatus::kBlocked);

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

  // Skip suggestion chips if any are shown.
  if (!GetPublishedSuggestionChips().empty())
    sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });

  // Move focus to the first app.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });

  // Skip over apps that were installed before the test item.
  sm_.Call([this, &test_item_index]() {
    for (int i = 0; i < test_item_index; ++i)
      SendKeyPressWithSearch(ui::VKEY_RIGHT);
  });

  // Check that the announcmenet for items with a block badge occurs.
  sm_.ExpectSpeech("app 0");
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
  std::vector<std::string> suggestion_chips = GetPublishedSuggestionChips();
  ASSERT_GE(suggestion_chips.size(), 1u);

  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeech("Press Search plus Space to activate");
  // Press space on the launcher button in shelf, this opens peeking
  // launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Launcher, partial view");

  // Move focus over suggestion chip views.
  for (auto& chip : suggestion_chips) {
    sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
    sm_.ExpectSpeech(chip);
    sm_.ExpectSpeech("Button");
  }

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
  const int default_app_count =
      AppListClientImpl::GetInstance()->GetModelUpdaterForTest()->ItemCount();
  PopulateApps(3);

  std::vector<std::string> suggestion_chips = GetPublishedSuggestionChips();
  ASSERT_GE(suggestion_chips.size(), 1u);

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

  // Move focus over suggestion chip views.
  for (auto& chip : suggestion_chips) {
    sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
    sm_.ExpectSpeech(chip);
    sm_.ExpectSpeech("Button");
  }

  // Focus through apps installed by default.
  sm_.Call([this, &default_app_count]() {
    for (int i = 0; i < default_app_count; ++i)
      SendKeyPressWithSearch(ui::VKEY_RIGHT);
  });

  // Move focus to the first app installed by the test.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("app 0");
  sm_.ExpectSpeech("Button");
  // Move focus to 2nd app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("app 1");
  sm_.ExpectSpeech("Button");
  // Move focus to 3rd app;
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("app 2");
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
  const int default_app_count =
      AppListClientImpl::GetInstance()->GetModelUpdaterForTest()->ItemCount();

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

  // Move focus to first app;
  sm_.Call([this, &default_app_count]() {
    SendKeyPressWithSearch(ui::VKEY_DOWN);

    if (default_app_count) {
      // Skip the suggestion chips.
      SendKeyPressWithSearch(ui::VKEY_DOWN);

      // Skip the default installed apps.
      for (int i = 0; i < default_app_count; ++i)
        SendKeyPressWithSearch(ui::VKEY_RIGHT);
    }
  });

  sm_.ExpectSpeech("app 0");
  sm_.ExpectSpeech("Button");

  // Combine items and create a new folder.
  sm_.Call([]() { SendKeyPressWithShiftAndControl(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Folder Unnamed");
  sm_.ExpectSpeech("Button");
  sm_.ExpectNextSpeechIsNot("Alert");
  sm_.ExpectSpeech("app 0 combined with app 1 to create new folder.");

  // Open the folder and move focus to the first item of the folder.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("app 1");
  sm_.ExpectSpeech("Button");

  // Remove the first item from the folder back to the top level app list.
  sm_.Call([]() { SendKeyPressWithShiftAndControl(ui::VKEY_LEFT); });
  sm_.ExpectSpeech("app 1");
  sm_.ExpectSpeech("Button");
  sm_.ExpectNextSpeechIsNot("Alert");
  std::string expected_text;
  sm_.ExpectSpeech(base::SStringPrintf(&expected_text,
                                       "Moved to Page 1, row 1, column %d.",
                                       default_app_count + 1));

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

// Checks that app list keyboard reordering is announced.
// TODO(mmourgos): The current method of accessibility announcements for item
// reordering uses alerts, this works for spoken feedback but does not work as
// well for braille users. The preferred way to handle this is to actually
// change focus as the user navigates, and to have each object's
// accessible name describe its position. (See crbug.com/1098495)
IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest, AppListReordering) {
  const int default_app_count =
      AppListClientImpl::GetInstance()->GetModelUpdaterForTest()->ItemCount();

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
  sm_.Call([this, &default_app_count]() {
    SendKeyPressWithSearch(ui::VKEY_DOWN);
    if (default_app_count) {
      // Skip the suggestion chips.
      SendKeyPressWithSearch(ui::VKEY_DOWN);

      // Skip the default installed apps.
      for (int i = 0; i < default_app_count; ++i)
        SendKeyPressWithSearch(ui::VKEY_RIGHT);
    }
  });
  sm_.ExpectSpeech("app 0");
  sm_.ExpectSpeech("Button");

  // The default column of app 0.
  const int original_column = default_app_count + 1;

  // The column of app 0 after rightward move.
  const int column_after_horizontal_move = original_column + 1;

  // Move the first item to the right.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_RIGHT); });
  sm_.ExpectNextSpeechIsNot("Alert");

  std::string expected_text;
  sm_.ExpectSpeech(base::SStringPrintf(&expected_text,
                                       "Moved to Page 1, row 1, column %d.",
                                       column_after_horizontal_move));

  // Move the focused item down.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_DOWN); });
  sm_.ExpectNextSpeechIsNot("Alert");
  sm_.ExpectSpeech(base::SStringPrintf(&expected_text,
                                       "Moved to Page 1, row 2, column %d.",
                                       column_after_horizontal_move));

  // Move the focused item down.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_DOWN); });
  sm_.ExpectNextSpeechIsNot("Alert");
  sm_.ExpectSpeech(base::SStringPrintf(&expected_text,
                                       "Moved to Page 1, row 3, column %d.",
                                       column_after_horizontal_move));

  // Move the focused item down.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_DOWN); });
  sm_.ExpectNextSpeechIsNot("Alert");
  sm_.ExpectSpeech(base::SStringPrintf(&expected_text,
                                       "Moved to Page 1, row 4, column %d.",
                                       column_after_horizontal_move));

  // Move the focused item down to page 2.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_DOWN); });
  sm_.ExpectNextSpeechIsNot("Alert");
  sm_.ExpectSpeech(base::SStringPrintf(&expected_text,
                                       "Moved to Page 2, row 1, column %d.",
                                       column_after_horizontal_move));

  // Move the focused item to the left.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_LEFT); });
  sm_.ExpectNextSpeechIsNot("Alert");
  sm_.ExpectSpeech(base::SStringPrintf(
      &expected_text, "Moved to Page 2, row 1, column %d.", original_column));

  // Move the focused item back up to page 1..
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_UP); });
  sm_.ExpectNextSpeechIsNot("Alert");
  sm_.ExpectSpeech(base::SStringPrintf(
      &expected_text, "Moved to Page 1, row 4, column %d.", original_column));

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListProductivityLauncherTest,
                       ClamshellLauncher) {
  std::vector<std::string> suggestion_chips = GetPublishedSuggestionChips();
  PopulateApps(3);

  int test_item_index = 0;
  ash::AppListItem* test_item = FindItemByName("app 0", &test_item_index);
  ASSERT_TRUE(test_item);

  EnableChromeVox();

  // Focus the shelf. This selects the launcher button.
  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeechPattern("Launcher");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Shelf");
  sm_.ExpectSpeech("Tool bar");

  // Activate the launcher button. This opens bubble launcher.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeechPattern("Search your device,*");
  sm_.ExpectSpeech("Edit text");

  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Button");

  // Move focus over recent apps, which are currently populated using suggestion
  // chip results.
  // TODO(https://crbug.com/1260427): Traverse over all recent apps when the
  // linked issue is fixed.
  if (!suggestion_chips.empty()) {
    sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
    sm_.ExpectSpeech("Button");
  }

  // Skip over apps that were installed before the test item.
  // This selects the first app installed by the test.
  for (int i = 0; i < test_item_index; ++i) {
    sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  }
  sm_.ExpectSpeech("app 0");
  sm_.ExpectSpeech("Button");

  // Move the focused item to the right. The announcement does not include a
  // page because the bubble launcher apps grid is scrollable, not paged.
  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_RIGHT); });

  std::string expected_text;
  sm_.ExpectSpeech(base::SStringPrintf(
      &expected_text, "Moved to row 1, column %d.", test_item_index + 2));

  sm_.Replay();
}

}  // namespace ash
