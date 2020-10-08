// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/spoken_feedback_browsertest.h"

#include <queue>

#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/widget/widget.h"

namespace {
const double kExpectedPhoneticSpeechAndHintDelayMS = 1000;
}  // namespace

namespace chromeos {

LoggedInSpokenFeedbackTest::LoggedInSpokenFeedbackTest()
    : animation_mode_(ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
LoggedInSpokenFeedbackTest::~LoggedInSpokenFeedbackTest() = default;

void LoggedInSpokenFeedbackTest::SetUpInProcessBrowserTestFixture() {
  AccessibilityManager::SetBrailleControllerForTest(&braille_controller_);
}

void LoggedInSpokenFeedbackTest::TearDownOnMainThread() {
  AccessibilityManager::SetBrailleControllerForTest(nullptr);
  // Unload the ChromeVox extension so the browser doesn't try to respond to
  // in-flight requests during test shutdown. https://crbug.com/923090
  AccessibilityManager::Get()->EnableSpokenFeedback(false);
  AutomationManagerAura::GetInstance()->Disable();
}

void LoggedInSpokenFeedbackTest::SendKeyPress(ui::KeyboardCode key) {
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, key, false, false, false, false)));
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithControl(ui::KeyboardCode key) {
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, key, true, false, false, false)));
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithShift(ui::KeyboardCode key) {
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, key, false, true, false, false)));
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithSearchAndShift(
    ui::KeyboardCode key) {
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, key, false, true, false, true)));
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithSearch(ui::KeyboardCode key) {
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, key, false, false, false, true)));
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithSearchAndControl(
    ui::KeyboardCode key) {
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, key, true, false, false, true)));
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithSearchAndControlAndShift(
    ui::KeyboardCode key) {
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, key, true, true, false, true)));
}

void LoggedInSpokenFeedbackTest::SendStickyKeyCommand() {
  // To avoid flakes in sending keys, execute the command directly in js.
  extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser()->profile(), extension_misc::kChromeVoxExtensionId,
      "CommandHandler.onCommand('toggleStickyMode');");
}

void LoggedInSpokenFeedbackTest::SendMouseMoveTo(const gfx::Point& location) {
  ASSERT_NO_FATAL_FAILURE(
      ASSERT_TRUE(ui_controls::SendMouseMove(location.x(), location.y())));
}

void LoggedInSpokenFeedbackTest::SimulateTouchScreenInChromeVox() {
  // ChromeVox looks at whether 'ontouchstart' exists to know whether
  // or not it should respond to hover events. Fake it so that touch
  // exploration events get spoken.
  extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser()->profile(), extension_misc::kChromeVoxExtensionId,
      "window.ontouchstart = function() {};");
}

bool LoggedInSpokenFeedbackTest::PerformAcceleratorAction(
    ash::AcceleratorAction action) {
  return ash::AcceleratorController::Get()->PerformActionIfEnabled(action, {});
}

void LoggedInSpokenFeedbackTest::DisableEarcons() {
  // Playing earcons from within a test is not only annoying if you're
  // running the test locally, but seems to cause crashes
  // (http://crbug.com/396507). Work around this by just telling
  // ChromeVox to not ever play earcons (prerecorded sound effects).
  extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser()->profile(), extension_misc::kChromeVoxExtensionId,
      "ChromeVox.earcons.playEarcon = function() {};");
}

void LoggedInSpokenFeedbackTest::EnableChromeVox() {
  // Test setup.
  // Enable ChromeVox, wait for something to be spoken, and disable earcons.
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  sm_.ExpectSpeechPattern("*");
  sm_.Call([this]() { DisableEarcons(); });
}

// Flaky test, crbug.com/1081563
IN_PROC_BROWSER_TEST_F(LoggedInSpokenFeedbackTest, DISABLED_AddBookmark) {
  EnableChromeVox();

  sm_.Call(
      [this]() { chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR); });

  // Create a bookmark with title "foo".
  sm_.Call(
      [this]() { chrome::ExecuteCommand(browser(), IDC_BOOKMARK_THIS_TAB); });

  sm_.ExpectSpeech("Bookmark name");
  sm_.ExpectSpeech("about:blank");
  sm_.ExpectSpeech("selected");
  sm_.ExpectSpeech("Edit text");
  sm_.ExpectSpeech("Bookmark added");
  sm_.ExpectSpeech("Dialog");
  sm_.ExpectSpeech("Bookmark added, window");

  sm_.Call([this]() {
    SendKeyPress(ui::VKEY_F);
    SendKeyPress(ui::VKEY_O);
    SendKeyPress(ui::VKEY_O);
  });
  sm_.ExpectSpeech("F");
  sm_.ExpectSpeech("O");
  sm_.ExpectSpeech("O");

  sm_.Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm_.ExpectSpeech("Bookmark folder");
  sm_.ExpectSpeech("Bookmarks bar");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("has pop up");

  sm_.Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm_.ExpectSpeech("More…");

  sm_.Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm_.ExpectSpeech("Remove");

  sm_.Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm_.ExpectSpeech("Done");

  sm_.Call([this]() { SendKeyPress(ui::VKEY_RETURN); });
  // Focus goes back to window.
  sm_.ExpectSpeechPattern("about:blank*");

  // Focus bookmarks bar and listen for "foo".
  sm_.Call(
      [this]() { chrome::ExecuteCommand(browser(), IDC_FOCUS_BOOKMARKS); });
  sm_.ExpectSpeech("foo");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Bookmarks");
  sm_.ExpectSpeech("Tool bar");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(LoggedInSpokenFeedbackTest, NavigateNotificationCenter) {
  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(ash::TOGGLE_MESSAGE_CENTER_BUBBLE));
  });
  sm_.ExpectSpeech(
      "Quick Settings, Press search plus left to access the notification "
      "center., window");

  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  // If you are hitting this in the course of changing the UI, please fix. This
  // item needs a label.
  sm_.ExpectSpeech("List item");

  // Furthermore, navigation is generally broken using Search+Left.

  sm_.Replay();
}

// Test Learn Mode by pressing a few keys in Learn Mode. Only available while
// logged in.
IN_PROC_BROWSER_TEST_F(LoggedInSpokenFeedbackTest, LearnModeHardwareKeys) {
  EnableChromeVox();
  sm_.Call([this]() {
    extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
        browser()->profile(), extension_misc::kChromeVoxExtensionId,
        "CommandHandler.onCommand('showKbExplorerPage');");
  });
  sm_.ExpectSpeech("ChromeVox Learn Mode");
  sm_.ExpectSpeech(
      "Press a qwerty key, refreshable braille key, or touch gesture to learn "
      "its function. Press control with w or escape to exit.");

  // These are the default top row keys and their descriptions which live in
  // ChromeVox.
  sm_.Call([this]() { SendKeyPress(ui::VKEY_F1); });
  sm_.ExpectSpeech("back");
  sm_.Call([this]() { SendKeyPress(ui::VKEY_F2); });
  sm_.ExpectSpeech("forward");
  sm_.Call([this]() { SendKeyPress(ui::VKEY_F3); });
  sm_.ExpectSpeech("refresh");
  sm_.Call([this]() { SendKeyPress(ui::VKEY_F4); });
  sm_.ExpectSpeech("toggle full screen");
  sm_.Call([this]() { SendKeyPress(ui::VKEY_F5); });
  sm_.ExpectSpeech("window overview");
  sm_.Call([this]() { SendKeyPress(ui::VKEY_F6); });
  sm_.ExpectSpeech("Brightness down");
  sm_.Call([this]() { SendKeyPress(ui::VKEY_F7); });
  sm_.ExpectSpeech("Brightness up");
  sm_.Call([this]() { SendKeyPress(ui::VKEY_F8); });
  sm_.ExpectSpeech("volume mute");
  sm_.Call([this]() { SendKeyPress(ui::VKEY_F9); });
  sm_.ExpectSpeech("volume down");
  sm_.Call([this]() { SendKeyPress(ui::VKEY_F10); });
  sm_.ExpectSpeech("volume up");

  sm_.Replay();
}

//
// Spoken feedback tests in both a logged in browser window and guest mode.
//

enum SpokenFeedbackTestVariant { kTestAsNormalUser, kTestAsGuestUser };

class SpokenFeedbackTest
    : public LoggedInSpokenFeedbackTest,
      public ::testing::WithParamInterface<SpokenFeedbackTestVariant> {
 protected:
  SpokenFeedbackTest() {}
  virtual ~SpokenFeedbackTest() {}

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
};

INSTANTIATE_TEST_SUITE_P(TestAsNormalAndGuestUser,
                         SpokenFeedbackTest,
                         ::testing::Values(kTestAsNormalUser,
                                           kTestAsGuestUser));

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, EnableSpokenFeedback) {
  EnableChromeVox();
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, FocusToolbar) {
  EnableChromeVox();
  sm_.Call([this]() { chrome::ExecuteCommand(browser(), IDC_FOCUS_TOOLBAR); });
  sm_.ExpectSpeech("Reload");
  sm_.ExpectSpeech("Button");

  sm_.Replay();
}

// TODO(crbug.com/1065235): flaky.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_TypeInOmnibox) {
  EnableChromeVox();

  sm_.Call([this]() {
    ui_test_utils::NavigateToURL(
        browser(), GURL("data:text/html;charset=utf-8,<p>unused</p>"));
  });

  sm_.Call([this]() { SendKeyPressWithControl(ui::VKEY_L); });
  sm_.ExpectSpeech("Address and search bar");

  sm_.Call([this]() {
    // Select all the text.
    SendKeyPressWithControl(ui::VKEY_A);

    // Type x, y, and z.
    SendKeyPress(ui::VKEY_X);
    SendKeyPress(ui::VKEY_Y);
    SendKeyPress(ui::VKEY_Z);
  });
  sm_.ExpectSpeech("X");
  sm_.ExpectSpeech("Y");
  sm_.ExpectSpeech("Z");

  sm_.Call([this]() { SendKeyPress(ui::VKEY_BACK); });
  sm_.ExpectSpeech("Z");

  // Auto completions.
  sm_.ExpectSpeech("xy search");
  sm_.ExpectSpeech("List item");
  sm_.ExpectSpeech("1 of 1");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, FocusShelf) {
  EnableChromeVox();

  sm_.Call(
      [this]() { EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF)); });
  sm_.ExpectSpeechPattern("Launcher");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Shelf");
  sm_.ExpectSpeech("Tool bar");
  sm_.ExpectSpeech(", window");
  sm_.ExpectSpeech("Press Search plus Space to activate");

  sm_.Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm_.ExpectSpeechPattern("Button");
  sm_.Replay();
}

// Verifies that pressing right arrow button with search button should move
// focus to the next ShelfItem instead of the last one
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ShelfIconFocusForward) {
  const std::string title("MockApp");
  ChromeLauncherController* controller = ChromeLauncherController::instance();

  // Add the ShelfItem to the ShelfModel after enabling the ChromeVox. Because
  // when an extension is enabled, the ShelfItems which are not recorded as
  // pinned apps in user preference will be removed.
  EnableChromeVox();
  sm_.Call([controller, title]() {
    controller->CreateAppShortcutLauncherItem(
        ash::ShelfID("FakeApp"), controller->shelf_model()->item_count(),
        base::ASCIIToUTF16(title));
  });

  // Focus on the shelf.
  sm_.Call([this]() { PerformAcceleratorAction(ash::FOCUS_SHELF); });
  sm_.ExpectSpeech("Launcher");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Shelf");
  sm_.ExpectSpeech("Tool bar");

  // Verifies that pressing right key with search key should move the focus of
  // ShelfItem correctly.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  // Chromium or Google Chrome button here (not being tested).
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Shelf");
  sm_.ExpectSpeech("Tool bar");

  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("MockApp");
  sm_.ExpectSpeech("Button");

  sm_.Replay();
}

// Verifies that speaking text under mouse works for Shelf button and voice
// announcements should not be stacked when mouse goes over many Shelf buttons
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, SpeakingTextUnderMouseForShelfItem) {
  // Add the ShelfItem to the ShelfModel after enabling the ChromeVox. Because
  // when an extension is enabled, the ShelfItems which are not recorded as
  // pinned apps in user preference will be removed.
  EnableChromeVox();

  sm_.Call([this]() {
    // Add three Shelf buttons. Wait for the change on ShelfModel to reach ash.
    ChromeLauncherController* controller = ChromeLauncherController::instance();
    const int base_index = controller->shelf_model()->item_count();
    const std::string title("MockApp");
    const std::string id("FakeApp");
    const int insert_app_num = 3;
    for (int i = 0; i < insert_app_num; i++) {
      std::string app_title = title + base::NumberToString(i);
      std::string app_id = id + base::NumberToString(i);
      controller->CreateAppShortcutLauncherItem(
          ash::ShelfID(app_id), base_index + i, base::ASCIIToUTF16(app_title));
    }

    // Enable the function of speaking text under mouse.
    ash::EventRewriterController::Get()->SetSendMouseEventsToDelegate(true);

    // Focus on the Shelf because voice text for focusing on Shelf is fixed.
    // Wait until voice announcements are finished.
    EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF));
  });
  sm_.ExpectSpeechPattern("Launcher");

  // Hover mouse on the Shelf button. Verifies that text under mouse is spoken.
  sm_.Call([this]() {
    ash::ShelfView* shelf_view =
        ash::Shelf::ForWindow(ash::Shell::Get()->GetPrimaryRootWindow())
            ->shelf_widget()
            ->shelf_view_for_testing();
    const int first_app_index =
        shelf_view->model()->GetItemIndexForType(ash::TYPE_PINNED_APP);
    SendMouseMoveTo(shelf_view->view_model()
                        ->view_at(first_app_index)
                        ->GetBoundsInScreen()
                        .CenterPoint());
  });

  sm_.ExpectSpeechPattern("MockApp*");
  sm_.ExpectSpeech("Button");

  sm_.Replay();
}

class ShelfNotificationBadgeSpokenFeedbackTest : public SpokenFeedbackTest {
 protected:
  ShelfNotificationBadgeSpokenFeedbackTest() {
    scoped_features_.InitWithFeatures({::features::kNotificationIndicator}, {});
  }
  ~ShelfNotificationBadgeSpokenFeedbackTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_features_;
};

INSTANTIATE_TEST_SUITE_P(TestAsNormalAndGuestUser,
                         ShelfNotificationBadgeSpokenFeedbackTest,
                         ::testing::Values(kTestAsNormalUser,
                                           kTestAsGuestUser));

// Verifies that an announcement is triggered when focusing a ShelfItem with a
// notification badge shown.
IN_PROC_BROWSER_TEST_P(ShelfNotificationBadgeSpokenFeedbackTest,
                       ShelfNotificationBadgeAnnouncement) {
  EnableChromeVox();

  // Create and add a test app to the shelf model.
  ash::ShelfItem item;
  item.id = ash::ShelfID("TestApp");
  item.title = base::ASCIIToUTF16("TestAppTitle");
  item.type = ash::ShelfItemType::TYPE_APP;
  ash::ShelfModel::Get()->Add(item);

  // Set the notification badge to be shown for the test app.
  ash::ShelfModel::Get()->UpdateItemNotification("TestApp", /*has_badge=*/true);

  // Focus on the shelf.
  sm_.Call([this]() { PerformAcceleratorAction(ash::FOCUS_SHELF); });
  sm_.ExpectSpeech("Launcher");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Shelf");
  sm_.ExpectSpeech("Tool bar");

  // Press right key twice to focus the test app.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("TestAppTitle");
  sm_.ExpectSpeech("Button");

  // Check that when a shelf app button with a notification badge is focused,
  // the correct announcement occurs.
  sm_.ExpectSpeech("TestAppTitle requests your attention.");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, OpenStatusTray) {
  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(ash::TOGGLE_SYSTEM_TRAY_BUBBLE));
  });
  sm_.ExpectSpeech(
      "Quick Settings, Press search plus left to access the notification "
      "center., window");
  sm_.Replay();
}

// Fails on ASAN. See http://crbug.com/776308 . (Note MAYBE_ doesn't work well
// with parameterized tests).
#if !defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, NavigateSystemTray) {
  EnableChromeVox();

  sm_.Call(
      [this]() { (PerformAcceleratorAction(ash::TOGGLE_SYSTEM_TRAY_BUBBLE)); });
  sm_.ExpectSpeechPattern(
      "Quick Settings, Press search plus left to access the notification "
      "center., window");

  sm_.Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm_.ExpectSpeech(GetParam() == kTestAsGuestUser ? "Exit guest" : "Sign out");
  sm_.ExpectSpeech("Button");

  // Next button.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_B); });
  sm_.ExpectSpeech("Shut down");
  sm_.ExpectSpeech("Button");

  sm_.Replay();
}
#endif  // !defined(ADDRESS_SANITIZER)

// TODO: these brightness announcements are actually not made.
// https://crbug.com/1064788
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_ScreenBrightness) {
  EnableChromeVox();

  sm_.Call([this]() { (PerformAcceleratorAction(ash::BRIGHTNESS_UP)); });
  sm_.ExpectSpeechPattern("Brightness * percent");

  sm_.Call([this]() { (PerformAcceleratorAction(ash::BRIGHTNESS_DOWN)); });
  sm_.ExpectSpeechPattern("Brightness * percent");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, VolumeSlider) {
  EnableChromeVox();

  sm_.Call([this]() {
    // Volume slider does not fire valueChanged event on first key press because
    // it has no widget.
    PerformAcceleratorAction(ash::VOLUME_UP);
    PerformAcceleratorAction(ash::VOLUME_UP);
  });
  sm_.ExpectSpeechPattern("* percent*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, OverviewMode) {
  EnableChromeVox();
  sm_.Call([this]() {
    ui_test_utils::NavigateToURL(browser(),
                                 GURL("data:text/html;charset=utf-8,<button "
                                      "autofocus>Click me</button>"));
  });

  sm_.ExpectSpeech("Click me");

  sm_.Call([this]() { (PerformAcceleratorAction(ash::TOGGLE_OVERVIEW)); });

  sm_.ExpectSpeech(
      "Entered window overview mode. Swipe to navigate, or press tab if using "
      "a keyboard.");

  sm_.Call([this]() { SendKeyPressWithShift(ui::VKEY_TAB); });
  sm_.ExpectSpeechPattern(
      "Chrom* - data:text slash html;charset equal utf-8, less than button "
      "autofocus greater than Click me less than slash button greater than");
  sm_.ExpectSpeechPattern("Press Ctrl plus W to close.");
  sm_.ExpectSpeechPattern(", window");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxFindInPage) {
  EnableChromeVox();

  sm_.Call([this]() {
    ui_test_utils::NavigateToURL(browser(),
                                 GURL("data:text/html;charset=utf-8,<button "
                                      "autofocus>Click me</button>"));
  });

  sm_.ExpectSpeech("Click me");

  // Press Search+/ to enter ChromeVox's "find in page".
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_2); });
  sm_.ExpectSpeech("Find in page");
  sm_.ExpectSpeech("Search");
  sm_.ExpectSpeech(
      "Type to search the page. Press enter to jump to the result, up or down "
      "arrows to browse results, keep typing to change your search, or escape "
      "to cancel.");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxNavigateAndSelect) {
  EnableChromeVox();

  sm_.Call([this]() {
    ui_test_utils::NavigateToURL(browser(),
                                 GURL("data:text/html;charset=utf-8,"
                                      "<h1>Title</h1>"
                                      "<button autofocus>Click me</button>"));
  });
  sm_.ExpectSpeech("Click me");

  // Press Search+Left to navigate to the previous item.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  sm_.ExpectSpeech("Title");
  sm_.ExpectSpeech("Heading 1");

  // Press Search+S to select the text.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_S); });
  sm_.ExpectSpeech("Title");
  sm_.ExpectSpeech("selected");

  // Press again to end the selection.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_S); });
  sm_.ExpectSpeech("End selection");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxStickyMode) {
  EnableChromeVox();

  sm_.Call([this]() {
    ui_test_utils::NavigateToURL(browser(),
                                 GURL("data:text/html;charset=utf-8,<button "
                                      "autofocus>Click me</button>"));
  });
  sm_.ExpectSpeech("Click me");

  // Press the sticky-key sequence: Search Search.
  sm_.Call([this]() { SendStickyKeyCommand(); });

  sm_.ExpectSpeech("Sticky mode enabled");

  sm_.Call([this]() { SendKeyPress(ui::VKEY_H); });
  sm_.ExpectSpeech("No next heading");

  sm_.Call([this]() { SendStickyKeyCommand(); });
  sm_.ExpectSpeech("Sticky mode disabled");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, TouchExploreStatusTray) {
  EnableChromeVox();
  sm_.Call([this]() { SimulateTouchScreenInChromeVox(); });

  // Send an accessibility hover event on the system tray, which is
  // what we get when you tap it on a touch screen when ChromeVox is on.
  sm_.Call([]() {
    ash::TrayBackgroundView* tray = ash::Shell::Get()
                                        ->GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->unified_system_tray();
    tray->NotifyAccessibilityEvent(ax::mojom::Event::kHover, true);
  });
  sm_.ExpectSpeechPattern("Status tray, time* Battery at* percent*");
  sm_.ExpectSpeech("Button");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxNextTabRecovery) {
  EnableChromeVox();

  sm_.Call([this]() {
    ui_test_utils::NavigateToURL(
        browser(), GURL("data:text/html;charset=utf-8,"
                        "<button id='b1' autofocus>11</button>"
                        "<button>22</button>"
                        "<button>33</button>"
                        "<h1>Middle</h1>"
                        "<button>44</button>"
                        "<button>55</button>"
                        "<div id=console aria-live=polite></div>"
                        "<script>"
                        "var b1 = document.getElementById('b1');"
                        "b1.addEventListener('blur', function() {"
                        "  document.getElementById('console').innerText = "
                        "'button lost focus';"
                        "});"
                        "</script>"));
  });
  sm_.ExpectSpeech("Button");

  // Press Search+H to go to the next heading
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_H); });

  sm_.ExpectSpeech("Middle");

  // To ensure that the setSequentialFocusNavigationStartingPoint has
  // executed before pressing Tab, the page has an event handler waiting
  // for the 'blur' event on the button, and when it loses focus it
  // triggers a live region announcement that we wait for, here.
  sm_.ExpectSpeech("button lost focus");

  // Now we know that focus has left the button, so the sequential focus
  // navigation starting point must be on the heading. Press Tab and
  // ensure that we land on the first link past the heading.
  sm_.Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm_.ExpectSpeech("44");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       MoveByCharacterPhoneticSpeechAndHints) {
  EnableChromeVox();
  sm_.Call([this]() {
    ui_test_utils::NavigateToURL(
        browser(), GURL("data:text/html,<button autofocus>Click me</button>"));
  });
  sm_.ExpectSpeech("Click me");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Press Search plus Space to activate");

  // Move by character through the button.
  // Assert that phonetic speech and hints are delayed.
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("L");
  sm_.ExpectSpeech("lima");
  sm_.Call([this]() {
    EXPECT_TRUE(sm_.GetDelayForLastUtteranceMS() >=
                kExpectedPhoneticSpeechAndHintDelayMS);
  });
  sm_.ExpectSpeech("Press Search plus Space to activate");
  sm_.Call([this]() {
    EXPECT_TRUE(sm_.GetDelayForLastUtteranceMS() >=
                kExpectedPhoneticSpeechAndHintDelayMS);
  });
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("I");
  sm_.ExpectSpeech("india");
  sm_.Call([this]() {
    EXPECT_TRUE(sm_.GetDelayForLastUtteranceMS() >=
                kExpectedPhoneticSpeechAndHintDelayMS);
  });
  sm_.ExpectSpeech("Press Search plus Space to activate");
  sm_.Call([this]() {
    EXPECT_TRUE(sm_.GetDelayForLastUtteranceMS() >=
                kExpectedPhoneticSpeechAndHintDelayMS);
  });
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("C");
  sm_.ExpectSpeech("charlie");
  sm_.Call([this]() {
    EXPECT_TRUE(sm_.GetDelayForLastUtteranceMS() >=
                kExpectedPhoneticSpeechAndHintDelayMS);
  });
  sm_.ExpectSpeech("Press Search plus Space to activate");
  sm_.Call([this]() {
    EXPECT_TRUE(sm_.GetDelayForLastUtteranceMS() >=
                kExpectedPhoneticSpeechAndHintDelayMS);
  });
  sm_.Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("K");
  sm_.ExpectSpeech("kilo");
  sm_.Call([this]() {
    EXPECT_TRUE(sm_.GetDelayForLastUtteranceMS() >=
                kExpectedPhoneticSpeechAndHintDelayMS);
  });
  sm_.ExpectSpeech("Press Search plus Space to activate");
  sm_.Call([this]() {
    EXPECT_TRUE(sm_.GetDelayForLastUtteranceMS() >=
                kExpectedPhoneticSpeechAndHintDelayMS);
  });
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ResetTtsSettings) {
  EnableChromeVox();
  sm_.Call([this]() {
    ui_test_utils::NavigateToURL(
        browser(), GURL("data:text/html,<button autofocus>Click me</button>"));
  });

  sm_.ExpectSpeech("Click me");

  // Reset Tts settings using hotkey and assert speech output.
  sm_.Call(
      [this]() { SendKeyPressWithSearchAndControlAndShift(ui::VKEY_OEM_5); });
  sm_.ExpectSpeech("Reset text to speech settings to default values");
  // Increase speech rate.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_4); });
  sm_.ExpectSpeech("Rate 19 percent");
  // Increase speech pitch.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_6); });
  sm_.ExpectSpeech("Pitch 50 percent");
  // Reset Tts settings again.
  sm_.Call(
      [this]() { SendKeyPressWithSearchAndControlAndShift(ui::VKEY_OEM_5); });
  sm_.ExpectSpeech("Reset text to speech settings to default values");
  // Ensure that increasing speech rate and pitch jump to the same values as
  // before.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_4); });
  sm_.ExpectSpeech("Rate 19 percent");
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_6); });
  sm_.ExpectSpeech("Pitch 50 percent");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, SmartStickyMode) {
  EnableChromeVox();
  sm_.Call([this]() {
    ui_test_utils::NavigateToURL(browser(),
                                 GURL("data:text/html,<p>start</p><input "
                                      "autofocus type='text'><p>end</p>"));
  });

  // The input is autofocused.
  sm_.ExpectSpeech("Edit text");

  // First, navigate with sticky mode on.
  sm_.Call([this]() { SendStickyKeyCommand(); });
  sm_.ExpectSpeech("Sticky mode enabled");

  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectNextSpeechIsNotPattern("Sticky mode *abled");
  sm_.ExpectSpeech("end");

  // Jump to beginning.
  sm_.Call([this]() { SendKeyPressWithSearchAndControl(ui::VKEY_LEFT); });
  sm_.ExpectNextSpeechIsNotPattern("Sticky mode *abled");
  sm_.ExpectSpeech("start");

  // The nextEditText command is explicitly excluded from toggling.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_E); });
  sm_.ExpectNextSpeechIsNotPattern("Sticky mode *abled");
  sm_.ExpectSpeech("Edit text");

  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectNextSpeechIsNotPattern("Sticky mode *abled");
  sm_.ExpectSpeech("end");

  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  sm_.ExpectSpeech("Sticky mode disabled");
  sm_.ExpectSpeech("Edit text");

  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  sm_.ExpectSpeech("Sticky mode enabled");
  sm_.ExpectSpeech("start");

  // Try a few jump commands and linear nav with no Search modifier. We never
  // leave sticky mode.
  sm_.Call([this]() { SendKeyPress(ui::VKEY_E); });
  sm_.ExpectSpeech("Edit text");

  sm_.Call([this]() { SendKeyPressWithShift(ui::VKEY_F); });
  sm_.ExpectSpeech("Edit text");

  sm_.Call([this]() { SendKeyPress(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("end");

  sm_.Call([this]() { SendKeyPress(ui::VKEY_F); });
  sm_.ExpectSpeech("Edit text");

  sm_.Call([this]() { SendKeyPressWithShift(ui::VKEY_E); });
  sm_.ExpectSpeech("Edit text");

  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  sm_.ExpectSpeech("start");

  // Now, navigate with sticky mode off.
  sm_.Call([this]() { SendStickyKeyCommand(); });
  sm_.ExpectSpeech("Sticky mode disabled");

  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectNextSpeechIsNotPattern("Sticky mode *abled");
  sm_.ExpectSpeech("Edit text");

  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectNextSpeechIsNotPattern("Sticky mode *abled");
  sm_.ExpectSpeech("end");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, HardwareKeysGetRewritten) {
  EnableChromeVox();
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_F7); });
  sm_.ExpectSpeech("Darken screen");
  sm_.Replay();
}

//
// Spoken feedback tests of the out-of-box experience.
//

class OobeSpokenFeedbackTest : public OobeBaseTest {
 protected:
  OobeSpokenFeedbackTest() = default;
  ~OobeSpokenFeedbackTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    // Many bots don't have keyboard/mice which triggers the HID detection
    // dialog in the OOBE.  Avoid confusing the tests with that.
    command_line->AppendSwitch(chromeos::switches::kDisableHIDDetectionOnOOBE);
  }

  SpeechMonitor sm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeSpokenFeedbackTest);
};

#if defined(MEMORY_SANITIZER)
// Times out under MSan: https://crbug.com/1071693
#define MAYBE_SpokenFeedbackInOobe DISABLED_SpokenFeedbackInOobe
#else
#define MAYBE_SpokenFeedbackInOobe SpokenFeedbackInOobe
#endif
IN_PROC_BROWSER_TEST_F(OobeSpokenFeedbackTest, MAYBE_SpokenFeedbackInOobe) {
  ui_controls::EnableUIControls();
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  AccessibilityManager::Get()->EnableSpokenFeedback(true);

  // The Let's go button gets initial focus.
  sm_.ExpectSpeech("Let's go");

  sm_.Call([]() {
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::VKEY_TAB, false, false, false, false));
  });
  sm_.ExpectSpeech("Shut down");
  sm_.ExpectSpeech("Button");

  sm_.Replay();
}

}  // namespace chromeos
