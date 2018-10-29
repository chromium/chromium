// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/speech_monitor.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/speech/tts_platform.h"
#include "chrome/browser/ui/ash/ksv/keyboard_shortcut_viewer_util.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/mus/ax_remote_host.h"
#include "ui/views/widget/widget.h"

using extensions::api::braille_display_private::StubBrailleController;

namespace chromeos {

//
// Spoken feedback tests only in a logged in user's window.
//

class LoggedInSpokenFeedbackTest : public InProcessBrowserTest {
 public:
  LoggedInSpokenFeedbackTest()
      : animation_mode_(ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~LoggedInSpokenFeedbackTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    AccessibilityManager::SetBrailleControllerForTest(&braille_controller_);
  }

  void TearDownOnMainThread() override {
    AccessibilityManager::SetBrailleControllerForTest(nullptr);
    AutomationManagerAura::GetInstance()->Disable();
  }

  void SendKeyPress(ui::KeyboardCode key) {
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, key, false, false, false, false)));
  }

  void SendKeyPressWithControl(ui::KeyboardCode key) {
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, key, true, false, false, false)));
  }

  void SendKeyPressWithSearchAndShift(ui::KeyboardCode key) {
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, key, false, true, false, true)));
  }

  void SendKeyPressWithSearch(ui::KeyboardCode key) {
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, key, false, false, false, true)));
  }

  void RunJavaScriptInChromeVoxBackgroundPage(const std::string& script) {
    extensions::ExtensionHost* host =
        extensions::ProcessManager::Get(browser()->profile())
            ->GetBackgroundHostForExtension(
                extension_misc::kChromeVoxExtensionId);
    CHECK(content::ExecuteScript(host->host_contents(), script));
  }

  void SimulateTouchScreenInChromeVox() {
    // ChromeVox looks at whether 'ontouchstart' exists to know whether
    // or not it should respond to hover events. Fake it so that touch
    // exploration events get spoken.
    RunJavaScriptInChromeVoxBackgroundPage(
        "window.ontouchstart = function() {};");
  }

  bool PerformAcceleratorAction(ash::AcceleratorAction action) {
    ash::AcceleratorController* controller =
        ash::Shell::Get()->accelerator_controller();
    return controller->PerformActionIfEnabled(action);
  }

  void DisableEarcons() {
    // Playing earcons from within a test is not only annoying if you're
    // running the test locally, but seems to cause crashes
    // (http://crbug.com/396507). Work around this by just telling
    // ChromeVox to not ever play earcons (prerecorded sound effects).
    RunJavaScriptInChromeVoxBackgroundPage(
        "cvox.ChromeVox.earcons.playEarcon = function() {};");
  }

  void EnableChromeVox() {
    // Test setup.
    // Enable ChromeVox, skip welcome message/notification, and disable earcons.
    ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

    AccessibilityManager::Get()->EnableSpokenFeedback(true);
    EXPECT_TRUE(speech_monitor_.SkipChromeVoxEnabledMessage());
    DisableEarcons();
  }

  void PressRepeatedlyUntilUtterance(ui::KeyboardCode key,
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

  SpeechMonitor speech_monitor_;

 private:
  StubBrailleController braille_controller_;
  ui::ScopedAnimationDurationScaleMode animation_mode_;

  DISALLOW_COPY_AND_ASSIGN(LoggedInSpokenFeedbackTest);
};

// This test is very flakey with ChromeVox Next since we generate a lot more
// utterances for text fields.
// TODO(dtseng): Fix properly.
IN_PROC_BROWSER_TEST_F(LoggedInSpokenFeedbackTest, DISABLED_AddBookmark) {
  EnableChromeVox();
  chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);

  // Create a bookmark with title "foo".
  chrome::ExecuteCommand(browser(), IDC_BOOKMARK_PAGE);
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

// Tests the keyboard shortcut viewer, which is an out-of-process mojo app.
IN_PROC_BROWSER_TEST_F(LoggedInSpokenFeedbackTest, KeyboardShortcutViewer) {
  EnableChromeVox();
  keyboard_shortcut_viewer_util::ToggleKeyboardShortcutViewer();

  // Focus should move to the search field and ChromeVox should speak it.
  while ("Search for keyboard shortcuts" !=
         speech_monitor_.GetNextUtterance()) {
  }

  // Capture the destroyed AX tree id when the remote host disconnects.
  base::RunLoop run_loop;
  ui::AXTreeID destroyed_tree_id = ui::AXTreeIDUnknown();
  extensions::AutomationEventRouter::GetInstance()
      ->SetTreeDestroyedCallbackForTest(base::BindRepeating(
          [](base::RunLoop* run_loop, ui::AXTreeID* destroyed_tree_id,
             ui::AXTreeID tree_id) {
            *destroyed_tree_id = tree_id;
            run_loop->Quit();
          },
          &run_loop, &destroyed_tree_id));

  // Close the remote shortcut viewer app.
  keyboard_shortcut_viewer_util::ToggleKeyboardShortcutViewer();

  // Wait for the AX tree to be destroyed.
  run_loop.Run();

  // Verify an AX tree was destroyed. It's awkward to get the remote app's
  // actual tree ID, so just ensure it's a valid ID and not the desktop.
  EXPECT_NE(ui::AXTreeIDUnknown(), destroyed_tree_id);
  EXPECT_NE(ui::DesktopAXTreeID(), destroyed_tree_id);

  extensions::AutomationEventRouter::GetInstance()
      ->SetTreeDestroyedCallbackForTest(base::DoNothing());
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

INSTANTIATE_TEST_CASE_P(TestAsNormalAndGuestUser,
                        SpokenFeedbackTest,
                        ::testing::Values(kTestAsNormalUser, kTestAsGuestUser));

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

// TODO(newcomer): reimplement this test once the AppListFocus changes are
// complete (http://crbug.com/784942).
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_NavigateAppLauncher) {
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
                                 "Battery is*full."));
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
    if (base::MatchPattern(utterance, "Edit text"))
      break;
  }

  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (utterance == "Entered window overview mode")
      break;
  }

  SendKeyPress(ui::VKEY_TAB);
  // On Chrome OS accessibility title for tabbed browser windows contains app
  // name ("Chrome" or "Chromium") in overview mode.
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (base::MatchPattern(utterance, "Chromium - about:blank"))
      break;
  }
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());
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
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(&LoggedInSpokenFeedbackTest::SendKeyPress,
                 base::Unretained(this), ui::VKEY_LWIN),
      base::TimeDelta::FromMilliseconds(200));

  EXPECT_EQ("Sticky mode enabled", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_H);
  while ("No next heading." != speech_monitor_.GetNextUtterance()) {
  }

  SendKeyPress(ui::VKEY_LWIN);

  // Sticky key has a minimum 100 ms check to prevent key repeat from toggling
  // it.
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(&LoggedInSpokenFeedbackTest::SendKeyPress,
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
  js_checker().ExecuteAsync("$('language-select').focus()");
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

}  // namespace chromeos
