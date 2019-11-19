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
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
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

void LoggedInSpokenFeedbackTest::SendKeyPressWithSearchAndShift(
    ui::KeyboardCode key) {
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, key, false, true, false, true)));
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithSearch(ui::KeyboardCode key) {
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, key, false, false, false, true)));
}

void LoggedInSpokenFeedbackTest::SendMouseMoveTo(const gfx::Point& location) {
  ASSERT_NO_FATAL_FAILURE(
      ASSERT_TRUE(ui_controls::SendMouseMove(location.x(), location.y())));
}

void LoggedInSpokenFeedbackTest::RunJavaScriptInChromeVoxBackgroundPage(
    const std::string& script) {
  extensions::ExtensionHost* host =
      extensions::ProcessManager::Get(browser()->profile())
          ->GetBackgroundHostForExtension(
              extension_misc::kChromeVoxExtensionId);
  CHECK(content::ExecuteScript(host->host_contents(), script));
}

void LoggedInSpokenFeedbackTest::SimulateTouchScreenInChromeVox() {
  // ChromeVox looks at whether 'ontouchstart' exists to know whether
  // or not it should respond to hover events. Fake it so that touch
  // exploration events get spoken.
  RunJavaScriptInChromeVoxBackgroundPage(
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
  RunJavaScriptInChromeVoxBackgroundPage(
      "ChromeVox.earcons.playEarcon = function() {};");
}

void LoggedInSpokenFeedbackTest::EnableChromeVox() {
  // Test setup.
  // Enable ChromeVox, skip welcome message/notification, and disable earcons.
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  EXPECT_TRUE(speech_monitor_.SkipChromeVoxEnabledMessage());
  DisableEarcons();
}

void LoggedInSpokenFeedbackTest::PressRepeatedlyUntilUtterance(
    ui::KeyboardCode key,
    const std::string& expected_utterance) {
  // This helper function is needed when you want to poll for something
  // that happens asynchronously. Keep pressing |key|, until
  // the speech feedback that follows is |expected_utterance|.
  // Note that this doesn't work if pressing that key doesn't speak anything
  // at all before the asynchronous event occurred.
  while (true) {
    SendKeyPress(key);
    const std::string& utterance = speech_monitor_.GetNextUtterance();
    if (utterance == expected_utterance)
      break;
  }
}

// This test is very flakey with ChromeVox Next since we generate a lot more
// utterances for text fields.
// TODO(dtseng): Fix properly.
IN_PROC_BROWSER_TEST_F(LoggedInSpokenFeedbackTest, DISABLED_AddBookmark) {
  EnableChromeVox();
  chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);

  // Create a bookmark with title "foo".
  chrome::ExecuteCommand(browser(), IDC_BOOKMARK_THIS_TAB);
  EXPECT_EQ("Bookmark added! dialog Bookmark name about:blank Edit text",
            speech_monitor_.GetNextUtterance());
  EXPECT_EQ("about:blank", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_F);
  EXPECT_EQ("f", speech_monitor_.GetNextUtterance());
  SendKeyPress(ui::VKEY_O);
  EXPECT_EQ("o", speech_monitor_.GetNextUtterance());
  SendKeyPress(ui::VKEY_O);
  EXPECT_EQ("o", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_TAB);
  EXPECT_EQ("Bookmark folder combo Box Bookmarks bar",
            speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_RETURN);

  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(), "*oolbar*"));
  // Wait for active window change to be announced to avoid interference from
  // that below.
  while (speech_monitor_.GetNextUtterance() != "window about blank tab") {
    // Do nothing.
  }

  // Focus bookmarks bar and listen for "foo".
  chrome::ExecuteCommand(browser(), IDC_FOCUS_BOOKMARKS);
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    VLOG(0) << "Got utterance: " << utterance;
    if (utterance == "Bookmarks,")
      break;
  }
  EXPECT_EQ("foo,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("button", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_F(LoggedInSpokenFeedbackTest,
                       DISABLED_NavigateNotificationCenter) {
  EnableChromeVox();

  EXPECT_TRUE(PerformAcceleratorAction(ash::TOGGLE_MESSAGE_CENTER_BUBBLE));

  // Tab to request the initial focus.
  SendKeyPress(ui::VKEY_TAB);

  // Wait for it to say "Notification Center, window".
  while ("Notification Center, window" != speech_monitor_.GetNextUtterance()) {
  }

  // Tab until we get to the Do Not Disturb button.
  SendKeyPress(ui::VKEY_TAB);
  do {
    std::string ut = speech_monitor_.GetNextUtterance();

    if (ut == "Do not disturb")
      break;
    else if (ut == "Button")
      SendKeyPress(ui::VKEY_TAB);
  } while (true);
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Not pressed", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_SPACE);
  EXPECT_EQ("Do not disturb", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Pressed", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_SPACE);
  EXPECT_EQ("Do not disturb", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Not pressed", speech_monitor_.GetNextUtterance());
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

// TODO(tommi): Flakily hitting HasOneRef DCHECK in
// AudioOutputResampler::Shutdown, see crbug.com/630031.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_EnableSpokenFeedback) {
  EnableChromeVox();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, FocusToolbar) {
  EnableChromeVox();
  chrome::ExecuteCommand(browser(), IDC_FOCUS_TOOLBAR);
  while (speech_monitor_.GetNextUtterance() != "Reload") {
  }
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_TypeInOmnibox) {
  EnableChromeVox();

  // Location bar has focus by default so just start typing.
  SendKeyPress(ui::VKEY_X);
  EXPECT_EQ("x", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_Y);
  EXPECT_EQ("y", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_Z);
  EXPECT_EQ("z", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_BACK);
  EXPECT_EQ("z", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, FocusShelf) {
  EnableChromeVox();

  EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF));
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "Launcher"))
      break;
  }
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());

  EXPECT_EQ("Shelf", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Tool bar", speech_monitor_.GetNextUtterance());
  EXPECT_EQ(", window", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_TAB);
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "*"));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "Button"));
}

// Verifies that pressing right arrow button with search button should move
// focus to the next ShelfItem instead of the last one
// (see https://crbug.com/947683).
// This test is flaky, see http://crbug.com/997628
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_ShelfIconFocusForward) {
  const std::string title("MockApp");
  ChromeLauncherController* controller = ChromeLauncherController::instance();

  // Add the ShelfItem to the ShelfModel after enabling the ChromeVox. Because
  // when an extension is enabled, the ShelfItems which are not recorded as
  // pinned apps in user preference will be removed.
  EnableChromeVox();
  controller->CreateAppShortcutLauncherItem(
      ash::ShelfID("FakeApp"), controller->shelf_model()->item_count(),
      base::ASCIIToUTF16(title));

  // Wait for the change on ShelfModel to reach ash.
  base::RunLoop().RunUntilIdle();

  // Focus on the shelf.
  EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF));
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "Launcher"))
      break;
  }

  ASSERT_EQ("Button", speech_monitor_.GetNextUtterance());
  ASSERT_EQ("Shelf", speech_monitor_.GetNextUtterance());
  ASSERT_EQ("Tool bar", speech_monitor_.GetNextUtterance());
  ASSERT_EQ(", window", speech_monitor_.GetNextUtterance());
  ASSERT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Verifies that pressing right key with search key should move the focus of
  // ShelfItem correctly.
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "*"));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "Button"));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "*"));
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), title));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "Button"));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "*"));
}

// Verifies that speaking text under mouse works for Shelf button and voice
// announcements should not be stacked when mouse goes over many Shelf buttons
// (see https://crbug.com/958120 and https://crbug.com/921182).
// TODO(crbug.com/921182): Fix test correctness/reliability and re-enable.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       DISABLED_SpeakingTextUnderMouseForShelfItem) {
  // Add the ShelfItem to the ShelfModel after enabling the ChromeVox. Because
  // when an extension is enabled, the ShelfItems which are not recorded as
  // pinned apps in user preference will be removed.
  EnableChromeVox();

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
  base::RunLoop().RunUntilIdle();

  // Enable the function of speaking text under mouse.
  ash::EventRewriterController::Get()->SetSendMouseEventsToDelegate(true);

  // Focus on the Shelf because voice text for focusing on Shelf is fixed. Wait
  // until voice announcements are finished.
  EXPECT_TRUE(PerformAcceleratorAction(ash::FOCUS_SHELF));
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "Launcher"))
      break;
  }
  ASSERT_EQ("Button", speech_monitor_.GetNextUtterance());
  ASSERT_EQ("Shelf", speech_monitor_.GetNextUtterance());
  ASSERT_EQ("Tool bar", speech_monitor_.GetNextUtterance());
  ASSERT_EQ(", window", speech_monitor_.GetNextUtterance());
  ASSERT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Hover mouse on the Shelf button. Verifies that text under mouse is spoken.
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
  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(), "MockApp0"));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "Button"));

  // Move mouse to the third Shelf button through the second one. Verifies that
  // only the last Shelf button is announced by ChromeVox.
  const int second_app_index = first_app_index + 1;
  SendMouseMoveTo(shelf_view->view_model()
                      ->view_at(second_app_index)
                      ->GetBoundsInScreen()
                      .CenterPoint());
  const int third_app_index = first_app_index + 2;
  SendMouseMoveTo(shelf_view->view_model()
                      ->view_at(third_app_index)
                      ->GetBoundsInScreen()
                      .CenterPoint());
  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(), "MockApp2"));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "Button"));
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, OpenStatusTray) {
  EnableChromeVox();

  EXPECT_TRUE(PerformAcceleratorAction(ash::TOGGLE_SYSTEM_TRAY_BUBBLE));
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "Status tray*"))
      break;
  }
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "time *"));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(),
                                 "Battery at * percent."));
  EXPECT_EQ("Dialog", speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(), "*window"));
}

// Fails on ASAN. See http://crbug.com/776308 . (Note MAYBE_ doesn't work well
// with parameterized tests).
#if !defined(ADDRESS_SANITIZER) && !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, NavigateSystemTray) {
  EnableChromeVox();

  EXPECT_TRUE(PerformAcceleratorAction(ash::TOGGLE_SYSTEM_TRAY_BUBBLE));
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "Status tray,"))
      break;
  }
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "*window"))
      break;
  }

  SendKeyPress(ui::VKEY_TAB);
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "Button"))
      break;
  }

  // Next element.
  SendKeyPressWithSearch(ui::VKEY_RIGHT);
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "*"));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "Button"));

  // Next button.
  SendKeyPressWithSearch(ui::VKEY_B);
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "*"));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "Button"));

  // Navigate to Bluetooth sub-menu and open it.
  while (true) {
    SendKeyPress(ui::VKEY_TAB);
    std::string content = speech_monitor_.GetNextUtterance();
    std::string role = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(content, "*Bluetooth*") &&
        base::MatchPattern(role, "Button"))
      break;
  }
  SendKeyPress(ui::VKEY_RETURN);

  // Navigate to return to previous menu button and press it.
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "Previous menu"))
      break;
    SendKeyPress(ui::VKEY_TAB);
  }
  SendKeyPress(ui::VKEY_RETURN);

  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "Bluetooth*"))
      break;
  }
}
#endif  // !defined(ADDRESS_SANITIZER) && !defined(OS_CHROMEOS)

// See http://crbug.com/443608
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_ScreenBrightness) {
  EnableChromeVox();

  EXPECT_TRUE(PerformAcceleratorAction(ash::BRIGHTNESS_UP));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(),
                                 "Brightness * percent"));

  EXPECT_TRUE(PerformAcceleratorAction(ash::BRIGHTNESS_DOWN));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(),
                                 "Brightness * percent"));
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_VolumeSlider) {
  EnableChromeVox();

  // Volume slider does not fire valueChanged event on first key press because
  // it has no widget.
  EXPECT_TRUE(PerformAcceleratorAction(ash::VOLUME_UP));
  EXPECT_TRUE(PerformAcceleratorAction(ash::VOLUME_UP));
  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(), "* percent*"));
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, OverviewMode) {
  EnableChromeVox();

  EXPECT_TRUE(PerformAcceleratorAction(ash::TOGGLE_OVERVIEW));
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (utterance == "Entered window overview mode. Press tab to navigate.")
      break;
  }

  // On Chrome OS accessibility title for tabbed browser windows contains app
  // name ("Chrome" or "Chromium") in overview mode.
  while (true) {
    // Tabbing may select a desk item in the overview desks bar, so we tab
    // repeatedly until the window is selected.
    SendKeyPress(ui::VKEY_TAB);
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "Chrom*"))
      break;
  }
  EXPECT_EQ("about:blank,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("window", speech_monitor_.GetNextUtterance());
}

#if defined(MEMORY_SANITIZER) || defined(OS_CHROMEOS)
// Fails under MemorySanitizer: http://crbug.com/472125
// Test is flaky under ChromeOS: http://crbug.com/897249
#define MAYBE_ChromeVoxShiftSearch DISABLED_ChromeVoxShiftSearch
#else
#define MAYBE_ChromeVoxShiftSearch ChromeVoxShiftSearch
#endif
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, MAYBE_ChromeVoxShiftSearch) {
  EnableChromeVox();

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html;charset=utf-8,<button autofocus>Click me</button>"));
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (utterance == "Click me")
      break;
  }

  // Press Search+/ to enter ChromeVox's "find in page".
  SendKeyPressWithSearch(ui::VKEY_OEM_2);

  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (utterance == "Find in page.")
      break;
  }
}

#if defined(MEMORY_SANITIZER) || defined(OS_CHROMEOS)
// Fails under MemorySanitizer: http://crbug.com/472125
// TODO(crbug.com/721475): Flaky on CrOS.
#define MAYBE_ChromeVoxNavigateAndSelect DISABLED_ChromeVoxNavigateAndSelect
#else
#define MAYBE_ChromeVoxNavigateAndSelect ChromeVoxNavigateAndSelect
#endif
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, MAYBE_ChromeVoxNavigateAndSelect) {
  EnableChromeVox();

  ui_test_utils::NavigateToURL(browser(),
                               GURL("data:text/html;charset=utf-8,"
                                    "<h1>Title</h1>"
                                    "<button autofocus>Click me</button>"));
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (utterance == "Click me")
      break;
  }
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());

  // Press Search+Left to navigate to the previous item.
  SendKeyPressWithSearch(ui::VKEY_LEFT);
  EXPECT_EQ("Title", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Heading 1", speech_monitor_.GetNextUtterance());

  // Press Search+S to select the text.
  SendKeyPressWithSearch(ui::VKEY_S);
  EXPECT_EQ("Title", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("selected", speech_monitor_.GetNextUtterance());

  // Press again to end the selection.
  SendKeyPressWithSearch(ui::VKEY_S);
  EXPECT_EQ("End selection", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Title", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_ChromeVoxNextStickyMode) {
  EnableChromeVox();

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html;charset=utf-8,<button autofocus>Click me</button>"));
  while ("Button" != speech_monitor_.GetNextUtterance()) {
  }

  // Press the sticky-key sequence: Search Search.
  SendKeyPress(ui::VKEY_LWIN);

  // Sticky key has a minimum 100 ms check to prevent key repeat from toggling
  // it.
  base::PostDelayedTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&LoggedInSpokenFeedbackTest::SendKeyPress,
                     base::Unretained(this), ui::VKEY_LWIN),
      base::TimeDelta::FromMilliseconds(200));

  EXPECT_EQ("Sticky mode enabled", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_H);
  while ("No next heading." != speech_monitor_.GetNextUtterance()) {
  }

  SendKeyPress(ui::VKEY_LWIN);

  // Sticky key has a minimum 100 ms check to prevent key repeat from toggling
  // it.
  base::PostDelayedTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&LoggedInSpokenFeedbackTest::SendKeyPress,
                     base::Unretained(this), ui::VKEY_LWIN),
      base::TimeDelta::FromMilliseconds(200));

  while ("Sticky mode disabled" != speech_monitor_.GetNextUtterance()) {
  }
}

// Flaky on Linux ChromiumOS MSan Tests. https://crbug.com/752427
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_TouchExploreStatusTray) {
  EnableChromeVox();
  SimulateTouchScreenInChromeVox();

  // Send an accessibility hover event on the system tray, which is
  // what we get when you tap it on a touch screen when ChromeVox is on.
  ash::TrayBackgroundView* tray = ash::Shell::Get()
                                      ->GetPrimaryRootWindowController()
                                      ->GetStatusAreaWidget()
                                      ->unified_system_tray();
  tray->NotifyAccessibilityEvent(ax::mojom::Event::kHover, true);

  EXPECT_EQ("Status tray,", speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(), "time*,"));
  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(), "Battery*"));
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxNextTabRecovery) {
  EnableChromeVox();

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
  while ("Button" != speech_monitor_.GetNextUtterance()) {
  }

  // Press Search+H to go to the next heading
  SendKeyPressWithSearch(ui::VKEY_H);
  while ("Middle" != speech_monitor_.GetNextUtterance()) {
  }

  // To ensure that the setSequentialFocusNavigationStartingPoint has
  // executed before pressing Tab, the page has an event handler waiting
  // for the 'blur' event on the button, and when it loses focus it
  // triggers a live region announcement that we wait for, here.
  while ("button lost focus" != speech_monitor_.GetNextUtterance()) {
  }

  // Now we know that focus has left the button, so the sequential focus
  // navigation starting point must be on the heading. Press Tab and
  // ensure that we land on the first link past the heading.
  SendKeyPress(ui::VKEY_TAB);
  while ("44" != speech_monitor_.GetNextUtterance()) {
  }
}

//
// Spoken feedback tests that run only in guest mode.
//

class GuestSpokenFeedbackTest : public LoggedInSpokenFeedbackTest {
 protected:
  GuestSpokenFeedbackTest() {}
  ~GuestSpokenFeedbackTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(
        switches::kLoginUser, user_manager::GuestAccountId().GetUserEmail());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestSpokenFeedbackTest);
};

IN_PROC_BROWSER_TEST_F(GuestSpokenFeedbackTest, FocusToolbar) {
  EnableChromeVox();
  chrome::ExecuteCommand(browser(), IDC_FOCUS_TOOLBAR);
  while (speech_monitor_.GetNextUtterance() != "Reload") {
  }
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
}

//
// Spoken feedback tests of the out-of-box experience.
//

class OobeSpokenFeedbackTest : public LoginManagerTest {
 protected:
  OobeSpokenFeedbackTest()
      : LoginManagerTest(false, true /* should_initialize_webui */) {}
  ~OobeSpokenFeedbackTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    // Many bots don't have keyboard/mice which triggers the HID detection
    // dialog in the OOBE.  Avoid confusing the tests with that.
    command_line->AppendSwitch(chromeos::switches::kDisableHIDDetectionOnOOBE);
  }

  SpeechMonitor speech_monitor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeSpokenFeedbackTest);
};

// Test is flaky: http://crbug.com/346797
IN_PROC_BROWSER_TEST_F(OobeSpokenFeedbackTest, DISABLED_SpokenFeedbackInOobe) {
  ui_controls::EnableUIControls();
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  LoginDisplayHost* login_display_host = LoginDisplayHost::default_host();
  WebUILoginView* web_ui_login_view = login_display_host->GetWebUILoginView();
  views::Widget* widget = web_ui_login_view->GetWidget();
  gfx::NativeWindow window = widget->GetNativeWindow();

  // We expect to be in the language select dropdown for this test to work,
  // so make sure that's the case.
  test::OobeJS().ExecuteAsync("$('language-select').focus()");
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  ASSERT_TRUE(speech_monitor_.SkipChromeVoxEnabledMessage());
  // There's no guarantee that ChromeVox speaks anything when injected after
  // the page loads, which is by design.  Tab forward and then backward
  // to make sure we get the right feedback from the language and keyboard
  // selection fields.
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      window, ui::VKEY_TAB, false, false, false, false));

  while (speech_monitor_.GetNextUtterance() != "Select your keyboard:") {
  }
  EXPECT_EQ("U S", speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(),
                                 "Combo box * of *"));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      window, ui::VKEY_TAB, false, true /*shift*/, false, false));
  while (speech_monitor_.GetNextUtterance() != "Select your language:") {
  }
  EXPECT_EQ("English ( United States)", speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(),
                                 "Combo box * of *"));
}

// This test is flaky (https://crbug.com/1013551).
IN_PROC_BROWSER_TEST_F(OobeSpokenFeedbackTest,
                       DISABLED_ChromeVoxPanelTabsMenuEmpty) {
  // The ChromeVox panel should not populate the tabs menu if we are in the
  // OOBE.
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  // Included to reduce flakiness.
  while (speech_monitor_.GetNextUtterance() !=
         "Press Search plus Space to activate.") {
  }
  // Press [search + .] to open ChromeVox Panel
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::VKEY_OEM_PERIOD, false, false, false, true));
  while (speech_monitor_.GetNextUtterance() != "ChromeVox Panel") {
  }
  // Go to tabs menu and verify that it has no items.
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::VKEY_RIGHT, false, false, false, false));
  while (speech_monitor_.GetNextUtterance() != "Speech") {
  }
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::VKEY_RIGHT, false, false, false, false));
  while (speech_monitor_.GetNextUtterance() != "Tabs") {
  }
  EXPECT_EQ("Menu", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("No items.", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       MoveByCharacterPhoneticSpeechAndHints) {
  EnableChromeVox();

  ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<button autofocus>Click me</button>"));
  EXPECT_EQ("Web Content", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Click me", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());

  // Move by character through the button.
  // Assert that phonetic speech and hints are delayed.
  SendKeyPressWithSearchAndShift(ui::VKEY_RIGHT);
  EXPECT_EQ("L", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("lima", speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(speech_monitor_.GetDelayForLastUtteranceMS() >=
              kExpectedPhoneticSpeechAndHintDelayMS);
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(speech_monitor_.GetDelayForLastUtteranceMS() >=
              kExpectedPhoneticSpeechAndHintDelayMS);
  SendKeyPressWithSearchAndShift(ui::VKEY_RIGHT);
  EXPECT_EQ("I", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("india", speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(speech_monitor_.GetDelayForLastUtteranceMS() >=
              kExpectedPhoneticSpeechAndHintDelayMS);
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(speech_monitor_.GetDelayForLastUtteranceMS() >=
              kExpectedPhoneticSpeechAndHintDelayMS);
  SendKeyPressWithSearchAndShift(ui::VKEY_RIGHT);
  EXPECT_EQ("C", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("charlie", speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(speech_monitor_.GetDelayForLastUtteranceMS() >=
              kExpectedPhoneticSpeechAndHintDelayMS);
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(speech_monitor_.GetDelayForLastUtteranceMS() >=
              kExpectedPhoneticSpeechAndHintDelayMS);
  SendKeyPressWithSearchAndShift(ui::VKEY_RIGHT);
  EXPECT_EQ("K", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("kilo", speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(speech_monitor_.GetDelayForLastUtteranceMS() >=
              kExpectedPhoneticSpeechAndHintDelayMS);
  EXPECT_EQ("Press Search plus Space to activate.",
            speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(speech_monitor_.GetDelayForLastUtteranceMS() >=
              kExpectedPhoneticSpeechAndHintDelayMS);
}

}  // namespace chromeos
