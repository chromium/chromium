// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/test/app_list_test_model.h"
#include "ash/app_list/test/test_search_result.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/shell.h"
#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/accessibility/spoken_feedback_browsertest.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

enum SpokenFeedbackAppListTestVariant { kTestAsNormalUser, kTestAsGuestUser };

class TestSuggestionChipResult : public ash::TestSearchResult {
 public:
  explicit TestSuggestionChipResult(const base::string16& title) {
    set_display_type(ash::SearchResultDisplayType::kRecommendation);
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
    ash::AppListView::SetShortAnimationForTesting(true);
    LoggedInSpokenFeedbackTest::SetUp();
  }

  void TearDown() override {
    LoggedInSpokenFeedbackTest::TearDown();
    ash::AppListView::SetShortAnimationForTesting(false);
  }

  void SetUpOnMainThread() override {
    LoggedInSpokenFeedbackTest::SetUpOnMainThread();
    auto* controller = ash::Shell::Get()->app_list_controller();
    controller->SetSearchTabletAndClamshellAccessibleName(
        l10n_util::GetStringUTF16(IDS_SEARCH_BOX_ACCESSIBILITY_NAME_TABLET),
        l10n_util::GetStringUTF16(IDS_SEARCH_BOX_ACCESSIBILITY_NAME));
    controller->SetAppListModelForTest(
        std::make_unique<ash::test::AppListTestModel>());
    app_list_test_model_ =
        static_cast<ash::test::AppListTestModel*>(controller->GetModel());
    search_model = controller->GetSearchModel();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam() == kTestAsGuestUser) {
      command_line->AppendSwitch(chromeos::switches::kGuestSession);
      command_line->AppendSwitch(::switches::kIncognito);
      command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                      "user");
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

 private:
  ash::test::AppListTestModel* app_list_test_model_ = nullptr;
  ash::SearchModel* search_model = nullptr;
};

INSTANTIATE_TEST_SUITE_P(TestAsNormalAndGuestUser,
                         SpokenFeedbackAppListTest,
                         ::testing::Values(kTestAsNormalUser,
                                           kTestAsGuestUser));

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest, LauncherStateTransition) {
  EnableChromeVox();

  EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF));

  while (!base::MatchPattern(speech_monitor_.GetNextUtterance(), "Launcher")) {
  }

  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Shelf", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Tool bar", speech_monitor_.GetNextUtterance());
  EXPECT_EQ(", window", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Press space on the launcher button in shelf, this opens peeking launcher.
  SendKeyPressWithSearch(ui::VKEY_SPACE);
  EXPECT_EQ("Search your device,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("apps,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("and web.", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Use the arrow keys to navigate your apps.",
            speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Edit text", speech_monitor_.GetNextUtterance());
  EXPECT_EQ(", window", speech_monitor_.GetNextUtterance());

  // Check that Launcher, partial view state is announced.
  EXPECT_EQ("Launcher, partial view", speech_monitor_.GetNextUtterance());

  // Send a key press to enable keyboard traversal
  SendKeyPressWithSearchAndShift(ui::VKEY_TAB);

  // Move focus to expand all apps button;
  SendKeyPressWithSearchAndShift(ui::VKEY_TAB);
  EXPECT_EQ("Expand to all apps", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Press space on expand arrow to go to fullscreen launcher.
  SendKeyPressWithSearch(ui::VKEY_SPACE);
  EXPECT_EQ("Search your device,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("apps,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("and web.", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Use the arrow keys to navigate your apps.",
            speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Edit text", speech_monitor_.GetNextUtterance());

  // Check that Launcher, all apps state is announced.
  EXPECT_EQ("Launcher, all apps", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       DisabledFullscreenExpandButton) {
  EnableChromeVox();

  EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF));

  while (speech_monitor_.GetNextUtterance() !=
         "Press Search plus Space to activate.") {
  }

  // Press space on the launcher button in shelf, this opens peeking launcher.
  SendKeyPressWithSearch(ui::VKEY_SPACE);
  while (speech_monitor_.GetNextUtterance() != "Launcher, partial view") {
  }

  // Send a key press to enable keyboard traversal
  SendKeyPressWithSearchAndShift(ui::VKEY_TAB);

  // Move focus to expand all apps button.
  SendKeyPressWithSearchAndShift(ui::VKEY_TAB);
  while (speech_monitor_.GetNextUtterance() !=
         "Press Search plus Space to activate.") {
  }

  // Press space on expand arrow to go to fullscreen launcher.
  SendKeyPressWithSearch(ui::VKEY_SPACE);
  while (speech_monitor_.GetNextUtterance() != "Launcher, all apps") {
  }

  // Make sure the first traversal left is not the expand arrow button.
  SendKeyPressWithSearch(ui::VKEY_LEFT);
  EXPECT_NE("Expand to all apps", speech_monitor_.GetNextUtterance());

  // Make sure the second traversal left is not the expand arrow button.
  SendKeyPressWithSearch(ui::VKEY_LEFT);
  EXPECT_NE("Expand to all apps", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       PeekingLauncherFocusTraversal) {
  // Add 3 suggestion chips.
  PopulateChips(3);

  EnableChromeVox();

  EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF));

  while (speech_monitor_.GetNextUtterance() !=
         "Press Search plus Space to activate.") {
  }

  // Press space on the launcher button in shelf, this opens peeking launcher.
  SendKeyPressWithSearch(ui::VKEY_SPACE);
  while (speech_monitor_.GetNextUtterance() != "Launcher, partial view") {
  }

  // Move focus to 1st suggestion chip;
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_EQ("Chip 0", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Move focus to 2nd suggestion chip;
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_EQ("Chip 1", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Move focus to 3rd suggestion chip;
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_EQ("Chip 2", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Move focus to expand all apps button;
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_EQ("Expand to all apps", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Move focus to app list window;
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_EQ(", window", speech_monitor_.GetNextUtterance());

  // Move focus to search box;
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_EQ("Search your device,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("apps,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("and web.", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Use the arrow keys to navigate your apps.",
            speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Edit text", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       FullscreenLauncherFocusTraversal) {
  // Add 1 suggestion chip and 3 apps.
  PopulateChips(1);
  PopulateApps(3);

  EnableChromeVox();

  EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF));

  while (speech_monitor_.GetNextUtterance() !=
         "Press Search plus Space to activate.") {
  }

  // Press space on the launcher button in shelf, this opens peeking launcher.
  SendKeyPressWithSearch(ui::VKEY_SPACE);
  while (speech_monitor_.GetNextUtterance() != "Launcher, partial view") {
  }

  // Send a key press to enable keyboard traversal
  SendKeyPressWithSearchAndShift(ui::VKEY_TAB);

  // Move focus to expand all apps button.
  SendKeyPressWithSearchAndShift(ui::VKEY_TAB);
  while (speech_monitor_.GetNextUtterance() !=
         "Press Search plus Space to activate.") {
  }

  // Press space on expand arrow to go to fullscreen launcher.
  SendKeyPressWithSearch(ui::VKEY_SPACE);
  while (speech_monitor_.GetNextUtterance() != "Launcher, all apps") {
  }

  // Move focus to the suggestion chip;
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_EQ("Chip 0", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Move focus to 1st app;
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_EQ("Item 0", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Move focus to 2nd app;
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_EQ("Item 1", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Move focus to 3rd app;
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_EQ("Item 2", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Move focus to app list window;
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_EQ(", window", speech_monitor_.GetNextUtterance());

  // Move focus to search box;
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_EQ("Search your device,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("apps,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("and web.", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Use the arrow keys to navigate your apps.",
            speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Edit text", speech_monitor_.GetNextUtterance());
}

// TODO(newcomer): reimplement this test once the AppListFocus changes are
// complete (http://crbug.com/784942).
IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       DISABLED_NavigateAppLauncher) {
  EnableChromeVox();

  EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF));

  // Wait for it to say "Launcher", "Button", "Shelf", "Tool bar".
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "Launcher"))
      break;
  }
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Shelf", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Tool bar", speech_monitor_.GetNextUtterance());

  // Click on the launcher, it brings up the app list UI.
  SendKeyPress(ui::VKEY_SPACE);
  while ("Search or type URL" != speech_monitor_.GetNextUtterance()) {
  }
  while ("Edit text" != speech_monitor_.GetNextUtterance()) {
  }

  // Close it and open it again.
  SendKeyPress(ui::VKEY_ESCAPE);
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "*window*"))
      break;
  }

  EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF));
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "Button"))
      break;
  }
  SendKeyPress(ui::VKEY_SPACE);

  // Now type a space into the text field and wait until we hear "space".
  // This makes the test more robust as it allows us to skip over other
  // speech along the way.
  SendKeyPress(ui::VKEY_SPACE);
  while (true) {
    if ("space" == speech_monitor_.GetNextUtterance())
      break;
  }

  // Now press the down arrow and we should be focused on an app button
  // in a dialog.
  SendKeyPress(ui::VKEY_DOWN);
  while ("Button" != speech_monitor_.GetNextUtterance()) {
  }
}

}  // namespace chromeos
