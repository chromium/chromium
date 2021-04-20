// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/spoken_feedback_browsertest.h"

#include <queue>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accessibility_controller.h"
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
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
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
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const double kExpectedPhoneticSpeechAndHintDelayMS = 1000;

}  // namespace

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

bool LoggedInSpokenFeedbackTest::PerformAcceleratorAction(
    AcceleratorAction action) {
  return AcceleratorController::Get()->PerformActionIfEnabled(action, {});
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
  // Enable ChromeVox, disable earcons and wait for key mappings to be fetched.
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  // TODO(accessibility): fix console error/warnings and insantiate
  // |console_observer_| here.

  // Load ChromeVox and block until it's fully loaded.
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  sm_.ExpectSpeechPattern("*");
  sm_.Call([this]() { DisableEarcons(); });
}

void LoggedInSpokenFeedbackTest::StablizeChromeVoxState() {
  sm_.Call([this]() {
    ui_test_utils::NavigateToURL(browser(),
                                 GURL("data:text/html;charset=utf-8,<button "
                                      "autofocus>Click me</button>"));
  });
  sm_.ExpectSpeech("Click me");
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
    EXPECT_TRUE(PerformAcceleratorAction(
        AcceleratorAction::TOGGLE_MESSAGE_CENTER_BUBBLE));
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
  sm_.ExpectSpeechPattern(
      "Press a qwerty key, refreshable braille key, or touch gesture to learn "
      "*");

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

IN_PROC_BROWSER_TEST_F(LoggedInSpokenFeedbackTest, LearnModeEscapeWithGesture) {
  EnableChromeVox();
  sm_.Call([this]() {
    extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
        browser()->profile(), extension_misc::kChromeVoxExtensionId,
        "CommandHandler.onCommand('showKbExplorerPage');");
  });
  sm_.ExpectSpeech("ChromeVox Learn Mode");
  sm_.ExpectSpeechPattern(
      "Press a qwerty key, refreshable braille key, or touch gesture to learn "
      "*");

  sm_.Call([]() {
    AccessibilityManager::Get()->HandleAccessibilityGesture(
        ax::mojom::Gesture::kSwipeLeft2, gfx::PointF());
  });
  sm_.ExpectSpeech("Swipe two fingers left");
  sm_.ExpectSpeech("Escape");
  sm_.ExpectSpeech("Stopping Learn Mode");

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
      command_line->AppendSwitch(switches::kGuestSession);
      command_line->AppendSwitch(::switches::kIncognito);
      command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
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

  sm_.Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
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
        ShelfID("FakeApp"), controller->shelf_model()->item_count(),
        base::ASCIIToUTF16(title));
  });

  // Focus on the shelf.
  sm_.Call(
      [this]() { PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF); });
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
      controller->CreateAppShortcutLauncherItem(ShelfID(app_id), base_index + i,
                                                base::ASCIIToUTF16(app_title));
    }

    // Enable the function of speaking text under mouse.
    EventRewriterController::Get()->SetSendMouseEvents(true);

    // Focus on the Shelf because voice text for focusing on Shelf is fixed.
    // Wait until voice announcements are finished.
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF));
  });
  sm_.ExpectSpeechPattern("Launcher");

  // Hover mouse on the Shelf button. Verifies that text under mouse is spoken.
  sm_.Call([this]() {
    ShelfView* shelf_view =
        Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
            ->shelf_widget()
            ->shelf_view_for_testing();
    const int first_app_index = shelf_view->model()->GetItemIndexForType(
        ShelfItemType::TYPE_PINNED_APP);
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
  ShelfItem item;
  item.id = ShelfID("TestApp");
  item.title = u"TestAppTitle";
  item.type = ShelfItemType::TYPE_APP;
  ShelfModel::Get()->Add(item);

  // Set the notification badge to be shown for the test app.
  ShelfModel::Get()->UpdateItemNotification("TestApp", /*has_badge=*/true);

  // Focus on the shelf.
  sm_.Call(
      [this]() { PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF); });
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

// Verifies that an announcement is triggered when focusing a paused app
// ShelfItem.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       ShelfPausedAppIconBadgeAnnouncement) {
  EnableChromeVox();

  std::string app_id = "TestApp";

  // Set the app status as paused;
  std::vector<apps::mojom::AppPtr> apps;
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kBuiltIn;
  app->app_id = app_id;
  app->readiness = apps::mojom::Readiness::kReady;
  app->paused = apps::mojom::OptionalBool::kTrue;
  apps.push_back(std::move(app));
  apps::AppServiceProxyFactory::GetForProfile(
      AccessibilityManager::Get()->profile())
      ->AppRegistryCache()
      .OnApps(std::move(apps), apps::mojom::AppType::kBuiltIn,
              false /* should_notify_initialized */);

  // Create and add a test app to the shelf model.
  ShelfItem item;
  item.id = ShelfID(app_id);
  item.title = u"TestAppTitle";
  item.type = ShelfItemType::TYPE_APP;
  item.app_status = AppStatus::kPaused;
  ShelfModel::Get()->Add(item);

  // Focus on the shelf.
  sm_.Call(
      [this]() { PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF); });
  sm_.ExpectSpeech("Launcher");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Shelf");
  sm_.ExpectSpeech("Tool bar");

  // Press right key twice to focus the test app.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("TestAppTitle");
  sm_.ExpectSpeech("Button");

  // Check that when a paused app shelf item is focused, the correct
  // announcement occurs.
  sm_.ExpectSpeech("Paused");

  sm_.Replay();
}

// Verifies that an announcement is triggered when focusing a blocked app
// ShelfItem.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       ShelfBlockedAppIconBadgeAnnouncement) {
  EnableChromeVox();

  std::string app_id = "TestApp";

  // Set the app status as paused;
  std::vector<apps::mojom::AppPtr> apps;
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kBuiltIn;
  app->app_id = app_id;
  app->readiness = apps::mojom::Readiness::kDisabledByPolicy;
  apps.push_back(std::move(app));
  apps::AppServiceProxyFactory::GetForProfile(
      AccessibilityManager::Get()->profile())
      ->AppRegistryCache()
      .OnApps(std::move(apps), apps::mojom::AppType::kBuiltIn,
              false /* should_notify_initialized */);

  // Create and add a test app to the shelf model.
  ShelfItem item;
  item.id = ShelfID(app_id);
  item.title = u"TestAppTitle";
  item.type = ShelfItemType::TYPE_APP;
  item.app_status = AppStatus::kBlocked;
  ShelfModel::Get()->Add(item);

  // Focus on the shelf.
  sm_.Call(
      [this]() { PerformAcceleratorAction(AcceleratorAction::FOCUS_SHELF); });
  sm_.ExpectSpeech("Launcher");
  sm_.ExpectSpeech("Button");
  sm_.ExpectSpeech("Shelf");
  sm_.ExpectSpeech("Tool bar");

  // Press right key twice to focus the test app.
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("TestAppTitle");
  sm_.ExpectSpeech("Button");

  // Check that when a blocked shelf app shelf item is focused, the correct
  // announcement occurs.
  sm_.ExpectSpeech("Blocked");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, OpenStatusTray) {
  EnableChromeVox();

  sm_.Call([this]() {
    EXPECT_TRUE(
        PerformAcceleratorAction(AcceleratorAction::TOGGLE_SYSTEM_TRAY_BUBBLE));
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

  sm_.Call([this]() {
    (PerformAcceleratorAction(AcceleratorAction::TOGGLE_SYSTEM_TRAY_BUBBLE));
  });
  sm_.ExpectSpeech(
      "Quick Settings, Press search plus left to access the notification "
      "center., window");

  // Avatar button.
  sm_.Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm_.ExpectSpeech(GetParam() == kTestAsGuestUser ? "Guest" : "");
  sm_.ExpectSpeech("Button");

  // Exit button.
  sm_.Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm_.ExpectSpeech(GetParam() == kTestAsGuestUser ? "Exit guest" : "Sign out");
  sm_.ExpectSpeech("Button");

  // Shutdown button.
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

  sm_.Call([this]() {
    (PerformAcceleratorAction(AcceleratorAction::BRIGHTNESS_UP));
  });
  sm_.ExpectSpeechPattern("Brightness * percent");

  sm_.Call([this]() {
    (PerformAcceleratorAction(AcceleratorAction::BRIGHTNESS_DOWN));
  });
  sm_.ExpectSpeechPattern("Brightness * percent");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, VolumeSlider) {
  EnableChromeVox();

  sm_.Call([this]() {
    // Volume slider does not fire valueChanged event on first key press because
    // it has no widget.
    PerformAcceleratorAction(AcceleratorAction::VOLUME_UP);
    PerformAcceleratorAction(AcceleratorAction::VOLUME_UP);
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

  sm_.Call([this]() {
    (PerformAcceleratorAction(AcceleratorAction::TOGGLE_OVERVIEW));
  });

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

// TODO(crbug.com/1177140) Re-enable test
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       DISABLED_ChromeVoxNavigateAndSelect) {
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

// This tests ChromeVox sticky mode using raw key events as opposed to directly
// sending js commands above. This variant may be subject to flakes as it
// depends on more of the UI events stack and sticky mode invocation has a
// timing element to it.
// Consistently failing on ChromiumOS MSan. http://crbug.com/1182542
#if defined(OS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_ChromeVoxStickyModeRawKeys DISABLED_ChromeVoxStickyModeRawKeys
#else
#define MAYBE_ChromeVoxStickyModeRawKeys ChromeVoxStickyModeRawKeys
#endif
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, MAYBE_ChromeVoxStickyModeRawKeys) {
  EnableChromeVox();
  StablizeChromeVoxState();

  sm_.Call([this]() {
    SendKeyPress(ui::VKEY_LWIN);
    SendKeyPress(ui::VKEY_LWIN);
  });
  sm_.ExpectSpeech("Sticky mode enabled");

  sm_.Call([this]() {
    SendKeyPress(ui::VKEY_LWIN);
    SendKeyPress(ui::VKEY_LWIN);
  });
  sm_.ExpectSpeech("Sticky mode disabled");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, TouchExploreStatusTray) {
  EnableChromeVox();

  base::SimpleTestTickClock clock;
  auto* clock_ptr = &clock;
  ui::SetEventTickClockForTesting(clock_ptr);

  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root_window);
  auto* generator_ptr = &generator;

  // Touch the status tray.
  sm_.Call([clock_ptr, generator_ptr]() {
    const gfx::Point& tray_center = Shell::Get()
                                        ->GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->unified_system_tray()
                                        ->GetBoundsInScreen()
                                        .CenterPoint();

    ui::TouchEvent touch_press(
        ui::ET_TOUCH_PRESSED, tray_center, base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_press);

    clock_ptr->Advance(base::TimeDelta::FromSeconds(1));

    ui::TouchEvent touch_move(
        ui::ET_TOUCH_MOVED, tray_center, base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move);
  });

  sm_.ExpectSpeechPattern("Status tray, time* Battery at* percent*");
  sm_.ExpectSpeech("Button");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       TouchExploreRightEdgeVolumeSliderOn) {
  EnableChromeVox();

  base::SimpleTestTickClock clock;
  auto* clock_ptr = &clock;
  ui::SetEventTickClockForTesting(clock_ptr);

  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root_window);
  auto* generator_ptr = &generator;

  // Force right edge volume slider gesture on.
  sm_.Call([] {
    AccessibilityController::Get()->EnableChromeVoxVolumeSlideGesture();
  });

  // Touch and slide on the right edge of the screen.
  sm_.Call([clock_ptr, generator_ptr]() {
    ui::TouchEvent touch_press(
        ui::ET_TOUCH_PRESSED, gfx::Point(1280, 200), base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_press);

    clock_ptr->Advance(base::TimeDelta::FromSeconds(1));

    ui::TouchEvent touch_move(
        ui::ET_TOUCH_MOVED, gfx::Point(1280, 300), base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move);

    clock_ptr->Advance(base::TimeDelta::FromSeconds(1));

    ui::TouchEvent touch_move2(
        ui::ET_TOUCH_MOVED, gfx::Point(1280, 400), base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move2);
  });

  sm_.ExpectSpeech("Volume");
  sm_.ExpectSpeech("Slider");

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       TouchExploreRightEdgeVolumeSliderOff) {
  EnableChromeVox();

  base::SimpleTestTickClock clock;
  auto* clock_ptr = &clock;
  ui::SetEventTickClockForTesting(clock_ptr);

  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root_window);
  auto* generator_ptr = &generator;

  // Build a simple window with a button and position it at the right edge of
  // the screen.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);

  // This is the right edge of the screen.
  params.bounds = {1250, 0, 50, 700};
  widget->Init(std::move(params));

  views::View* view = new views::View();
  view->GetViewAccessibility().OverrideName("hello");
  view->GetViewAccessibility().OverrideRole(ax::mojom::Role::kButton);
  view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  widget->GetRootView()->AddChildView(view);

  // Show the widget, then touch and slide on the right edge of the screen.
  sm_.Call([widget, clock_ptr, generator_ptr]() {
    widget->Show();
    ui::TouchEvent touch_press(
        ui::ET_TOUCH_PRESSED, gfx::Point(1280, 200), base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_press);

    clock_ptr->Advance(base::TimeDelta::FromSeconds(1));

    ui::TouchEvent touch_move(
        ui::ET_TOUCH_MOVED, gfx::Point(1280, 300), base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move);

    clock_ptr->Advance(base::TimeDelta::FromSeconds(1));

    ui::TouchEvent touch_move2(
        ui::ET_TOUCH_MOVED, gfx::Point(1280, 400), base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move2);
  });

  // This should trigger reading of the button.
  sm_.ExpectSpeech("hello");
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
  StablizeChromeVoxState();
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_F7); });
  sm_.ExpectSpeech("Darken screen");
  sm_.Replay();
}

// Tests basic behavior of the tutorial when signed in.
// TODO(akihiroota): fix flakiness: http://crbug.com/1172390
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_Tutorial) {
  EnableChromeVox();
  sm_.Call([this]() {
    ui_test_utils::NavigateToURL(
        browser(), GURL("data:text/html,<button autofocus>Testing</button>"));
  });
  sm_.Call([this]() {
    SendKeyPressWithSearch(ui::VKEY_O);
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::VKEY_T, false, false, false, false));
  });
  sm_.ExpectSpeech("ChromeVox tutorial");
  sm_.ExpectSpeech(
      "Press Search plus Right Arrow, or Search plus Left Arrow to browse "
      "topics");
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Quick orientation");
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm_.ExpectSpeech("Essential keys");
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_B); });
  sm_.ExpectSpeech("Exit tutorial");
  sm_.Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm_.ExpectSpeech("Testing");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ClipboardCopySpeech) {
  EnableChromeVox();
  sm_.Call([this]() {
    ui_test_utils::NavigateToURL(browser(),
                                 GURL("data:text/html,<input autofocus "
                                      "type='text' value='Foo'></input>"));
  });

  // The input is autofocused.
  sm_.ExpectSpeech("Edit text");

  // Select and copy the first character.
  sm_.Call([this]() {
    SendKeyPressWithShift(ui::VKEY_RIGHT);
    SendKeyPressWithControl(ui::VKEY_C);
  });
  sm_.ExpectSpeech("copy F.");

  // Select and copy the first two characters.
  sm_.Call([this]() {
    SendKeyPressWithShift(ui::VKEY_RIGHT);
    SendKeyPressWithControl(ui::VKEY_C);
  });
  sm_.ExpectSpeech("copy Fo.");

  // Select and copy all characters.
  sm_.Call([this]() {
    SendKeyPressWithShift(ui::VKEY_RIGHT);
    SendKeyPressWithControl(ui::VKEY_C);
  });
  sm_.ExpectSpeech("copy Foo.");

  // Do it again with the command Search+Ctrl+C, which should do the same thing
  // but triggered through ChromeVox via synthesized keys.
  sm_.Call([this]() { SendKeyPressWithSearchAndControl(ui::VKEY_C); });
  sm_.ExpectSpeech("copy Foo.");

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
    command_line->AppendSwitch(switches::kDisableHIDDetectionOnOOBEForTesting);
    // We only start the tutorial in OOBE if the device is a Chromebook, so set
    // the device type so tutorial-related behavior can be tested.
    command_line->AppendSwitchASCII(switches::kFormFactor, "CHROMEBOOK");
  }

  test::SpeechMonitor sm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeSpokenFeedbackTest);
};

IN_PROC_BROWSER_TEST_F(OobeSpokenFeedbackTest, SpokenFeedbackInOobe) {
  ui_controls::EnableUIControls();
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  AccessibilityManager::Get()->EnableSpokenFeedback(true);

  // If ChromeVox is started in OOBE, the tutorial is automatically opened.
  sm_.ExpectSpeech("Welcome to ChromeVox!");
  sm_.ExpectSpeechPattern(
      "Welcome to the ChromeVox tutorial*When you're ready, use the spacebar "
      "to move to the next lesson.");

  // The tutorial can be exited by pressing Escape.
  sm_.Call([]() {
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::VKEY_ESCAPE, false, false, false, false));
  });

  if (ash::features::IsNewOobeLayoutEnabled()) {
    // The Get started button gets initial focus.
    sm_.ExpectSpeech("Get started");
  } else {
    // The Let's go button gets initial focus.
    sm_.ExpectSpeech("Let's go");
  }

  sm_.Call([]() {
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::VKEY_TAB, false, false, false, false));
  });
  sm_.ExpectSpeech("Shut down");
  sm_.ExpectSpeech("Button");

  sm_.Replay();
}

// TODO(akihiroota): fix flakiness: http://crbug.com/1172390
IN_PROC_BROWSER_TEST_F(OobeSpokenFeedbackTest,
                       DISABLED_SpokenFeedbackTutorialInOobe) {
  ui_controls::EnableUIControls();
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  sm_.ExpectSpeech("Welcome to ChromeVox!");
  sm_.ExpectSpeechPattern(
      "Welcome to the ChromeVox tutorial*When you're ready, use the spacebar "
      "to move to the next lesson.");
  // Press space to move to the next lesson.
  sm_.Call([]() {
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::VKEY_SPACE, false, false, false, false));
  });
  sm_.ExpectSpeech("Essential Keys: Control");
  sm_.ExpectSpeechPattern("*To continue, press the Control key.*");
  // Press control to move to the next lesson.
  sm_.Call([]() {
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::VKEY_CONTROL, false, false, false, false));
  });
  sm_.ExpectSpeechPattern("*To continue, press the left Shift key.");
  sm_.Replay();
}

class SigninToUserProfileSwitchTest : public OobeSpokenFeedbackTest {
 public:
  // OobeSpokenFeedbackTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeSpokenFeedbackTest::SetUpCommandLine(command_line);
    // Force the help app to launch in the background.
    command_line->AppendSwitch(switches::kForceFirstRunUI);
  }

 protected:
  LoginManagerMixin login_manager_{&mixin_host_};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
};

// Verifies that spoken feedback correctly handles profile switch (signin ->
// user) and announces the sync consent screen correctly.
IN_PROC_BROWSER_TEST_F(SigninToUserProfileSwitchTest, LoginAsNewUser) {
  // Force sync screen.
  auto reset = WizardController::ForceBrandedBuildForTesting(true);
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  sm_.ExpectSpeechPattern("*");

  sm_.Call([this]() {
    ASSERT_EQ(AccessibilityManager::Get()->profile(),
              ProfileHelper::GetSigninProfile());
    login_manager_.LoginAsNewRegularUser();
  });

  sm_.ExpectSpeechPattern("Welcome to the ChromeVox tutorial*");

  // The tutorial can be exited by pressing Escape.
  sm_.Call([]() {
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::VKEY_ESCAPE, false, false, false, false));
  });

  std::string button_title =
      features::IsSplitSettingsSyncEnabled() ? "Got it" : "Accept and continue";
  sm_.ExpectSpeech(button_title);

  // Check that profile switched to the active user.
  sm_.Call([]() {
    ASSERT_EQ(AccessibilityManager::Get()->profile(),
              ProfileManager::GetActiveUserProfile());
  });
  sm_.Replay();
}

}  // namespace ash
