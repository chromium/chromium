// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "apps/launcher.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/mojom/tray_action.mojom-shared.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/lock_screen_apps/state_controller.h"
#include "chrome/browser/ash/lock_screen_apps/state_observer.h"
#include "chrome/browser/ash/note_taking/note_taking_helper.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class BrowserMainParts;
}

namespace {

using ash::mojom::TrayActionState;

const char kTestAppId[] = "cadfeochfldmbdgoccgbeianhamecbae";

// Class used to wait for a specific lock_screen_apps::StateController state.
class LockScreenAppsEnabledWaiter : public lock_screen_apps::StateObserver {
 public:
  LockScreenAppsEnabledWaiter() = default;

  LockScreenAppsEnabledWaiter(const LockScreenAppsEnabledWaiter&) = delete;
  LockScreenAppsEnabledWaiter& operator=(const LockScreenAppsEnabledWaiter&) =
      delete;

  ~LockScreenAppsEnabledWaiter() override {}

  // Runs loop until lock_screen_apps::StateController enters |target_state|.
  // Note: as currently implemented, this will fail if a transition to a state
  // different than |target_state| is observed.
  bool WaitForState(TrayActionState target_state) {
    TrayActionState state =
        lock_screen_apps::StateController::Get()->GetLockScreenNoteState();
    if (target_state == state)
      return true;

    base::RunLoop run_loop;
    state_change_callback_ = run_loop.QuitClosure();
    lock_screen_apps_state_observation_.Observe(
        lock_screen_apps::StateController::Get());
    run_loop.Run();

    lock_screen_apps_state_observation_.Reset();

    return target_state ==
           lock_screen_apps::StateController::Get()->GetLockScreenNoteState();
  }

  void OnLockScreenNoteStateChanged(TrayActionState state) override {
    ASSERT_FALSE(state_change_callback_.is_null());
    std::move(state_change_callback_).Run();
  }

 private:
  base::ScopedObservation<lock_screen_apps::StateController,
                          lock_screen_apps::StateObserver>
      lock_screen_apps_state_observation_{this};

  base::OnceClosure state_change_callback_;
};

class LockScreenNoteTakingTest : public extensions::ExtensionBrowserTest {
 public:
  LockScreenNoteTakingTest() { set_chromeos_user_ = true; }

  LockScreenNoteTakingTest(const LockScreenNoteTakingTest&) = delete;
  LockScreenNoteTakingTest& operator=(const LockScreenNoteTakingTest&) = delete;

  ~LockScreenNoteTakingTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd_line) override {
    cmd_line->AppendSwitchASCII(extensions::switches::kAllowlistedExtensionID,
                                kTestAppId);
    cmd_line->AppendSwitch(ash::switches::kAshForceEnableStylusTools);

    extensions::ExtensionBrowserTest::SetUpCommandLine(cmd_line);
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    // Creating result catcher to be used by tests eary on to avoid flaky hangs
    // in the DataAvailableOnRestart test.
    // The tests expects the test app (which is installed in the tests PRE_
    // part) to run in response to the
    // lockScreen.data.onDataItemsAvailable event which is dispatched early on,
    // during "user" session start-up, which happens before test body is run.
    // This means that result catchers created in the test body might miss test
    // completion notifications from the app.
    result_catcher_ = std::make_unique<extensions::ResultCatcher>();
    extensions::ExtensionBrowserTest::CreatedBrowserMainParts(
        browser_main_parts);
  }

  void TearDownOnMainThread() override {
    result_catcher_.reset();
    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

  bool EnableLockScreenAppLaunch(const std::string& app_id) {
    ash::NoteTakingHelper::Get()->SetPreferredApp(profile(), app_id);
    ash::NoteTakingHelper::Get()->SetPreferredAppEnabledOnLockScreen(profile(),
                                                                     true);

    session_manager::SessionManager::Get()->SetSessionState(
        session_manager::SessionState::LOCKED);

    return LockScreenAppsEnabledWaiter().WaitForState(
        ash::mojom::TrayActionState::kAvailable);
  }

  bool RunTestAppInLockScreenContext(const std::string& test_app,
                                     std::string* error) {
    scoped_refptr<const extensions::Extension> app =
        LoadExtension(test_data_dir_.AppendASCII(test_app));
    if (!app) {
      *error = "Unable to load the test app.";
      return false;
    }

    if (!EnableLockScreenAppLaunch(app->id())) {
      *error = "Failed to enable app for lock screen.";
      return false;
    }

    // The test app will send "readyToClose" message from the app window created
    // as part of the test. The message will be sent after the tests in the app
    // window context have been run and the window is ready to be closed.
    // The test should reply to this message in order for the app window to
    // close itself.
    ExtensionTestMessageListener ready_to_close("readyToClose",
                                                ReplyBehavior::kWillReply);

    lock_screen_apps::StateController::Get()->RequestNewLockScreenNote(
        ash::mojom::LockScreenNoteOrigin::kLockScreenButtonTap);

    if (lock_screen_apps::StateController::Get()->GetLockScreenNoteState() !=
        ash::mojom::TrayActionState::kLaunching) {
      *error = "App launch request failed";
      return false;
    }

    // The test will run two sets of tests:
    // *  in the window that gets created as the response to the new_note action
    //    launch
    // *  in the app background page - the test will launch an app window and
    //    wait for it to be closed
    // Test runner should wait for both of those to finish (test result message
    // will be sent for each set of tests).
    if (!result_catcher_->GetNextResult()) {
      *error = result_catcher_->message();
      if (ready_to_close.was_satisfied())
        ready_to_close.Reply("failed");
      return false;
    }

    if (!ready_to_close.WaitUntilSatisfied()) {
      *error = "Failed waiting for readyToClose message.";
      return false;
    }

    // Close the app window created by the API test.
    ready_to_close.Reply("close");

    if (!result_catcher_->GetNextResult()) {
      *error = result_catcher_->message();
      return false;
    }

    return true;
  }

  extensions::ResultCatcher* result_catcher() { return result_catcher_.get(); }

 private:
  std::unique_ptr<extensions::ResultCatcher> result_catcher_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(LockScreenNoteTakingTest, Launch) {
  std::string error_message;
  ASSERT_TRUE(RunTestAppInLockScreenContext("lock_screen_apps/app_launch",
                                            &error_message))
      << error_message;

  EXPECT_EQ(ash::mojom::TrayActionState::kAvailable,
            lock_screen_apps::StateController::Get()->GetLockScreenNoteState());
}

// Tests that lock screen app window creation fails if not requested from the
// lock screen context - the test app runs tests as a response to a launch event
// in the user's profile (rather than the lock screen profile).
IN_PROC_BROWSER_TEST_F(LockScreenNoteTakingTest, LaunchInNonLockScreenContext) {
  scoped_refptr<const extensions::Extension> app = LoadExtension(
      test_data_dir_.AppendASCII("lock_screen_apps/non_lock_screen_context"));
  ASSERT_TRUE(app);
  ASSERT_TRUE(EnableLockScreenAppLaunch(app->id()));

  // Get the lock screen apps state controller to the state where lock screen
  // enabled app window creation is allowed (provided the window is created
  // from a lock screen context).
  // NOTE: This is not mandatory for the test to pass, but without it, app
  //     window creation would fail regardless of the context from which
  //     chrome.app.window.create is called.
  lock_screen_apps::StateController::Get()->RequestNewLockScreenNote(
      ash::mojom::LockScreenNoteOrigin::kLockScreenButtonTap);

  ASSERT_EQ(ash::mojom::TrayActionState::kLaunching,
            lock_screen_apps::StateController::Get()->GetLockScreenNoteState());

  // Launch note taking in regular, non lock screen context. The test will
  // verify the app cannot create lock screen enabled app windows in this case.
  extensions::api::app_runtime::ActionData action_data;
  action_data.action_type = extensions::api::app_runtime::ActionType::kNewNote;
  apps::LaunchPlatformAppWithAction(profile(), app.get(),
                                    std::move(action_data));

  ASSERT_TRUE(result_catcher()->GetNextResult()) << result_catcher()->message();
}

IN_PROC_BROWSER_TEST_F(LockScreenNoteTakingTest, DataCreation) {
  std::string error_message;
  ASSERT_TRUE(RunTestAppInLockScreenContext("lock_screen_apps/data_provider",
                                            &error_message))
      << error_message;

  EXPECT_EQ(ash::mojom::TrayActionState::kAvailable,
            lock_screen_apps::StateController::Get()->GetLockScreenNoteState());

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Unlocking the session should trigger onDataItemsAvailable event, which
  // should be catched by the background page in the main app - the event should
  // start another test sequence.
  ASSERT_TRUE(result_catcher()->GetNextResult()) << result_catcher()->message();
}

IN_PROC_BROWSER_TEST_F(LockScreenNoteTakingTest, PRE_DataAvailableOnRestart) {
  std::string error_message;
  ASSERT_TRUE(RunTestAppInLockScreenContext("lock_screen_apps/data_provider",
                                            &error_message))
      << error_message;

  EXPECT_EQ(ash::mojom::TrayActionState::kAvailable,
            lock_screen_apps::StateController::Get()->GetLockScreenNoteState());
}

IN_PROC_BROWSER_TEST_F(LockScreenNoteTakingTest, DataAvailableOnRestart) {
  // In PRE_ part  of the test there were data items created in the lock screen
  // storage - when the lock screen note taking is initialized,
  // OnDataItemsAvailable should be dispatched to the test app (given that the
  // lock screen app's data storage is not empty), which should in turn run a
  // sequence of API tests (in the test app background page).
  // This test is intended to catch the result of these tests.
  ASSERT_TRUE(result_catcher()->GetNextResult()) << result_catcher()->message();
}

IN_PROC_BROWSER_TEST_F(LockScreenNoteTakingTest, AppLaunchActionDataParams) {
  scoped_refptr<const extensions::Extension> app = LoadExtension(
      test_data_dir_.AppendASCII("lock_screen_apps/app_launch_action_data"));
  ASSERT_TRUE(app);
  ASSERT_TRUE(EnableLockScreenAppLaunch(app->id()));

  lock_screen_apps::StateController::Get()->RequestNewLockScreenNote(
      ash::mojom::LockScreenNoteOrigin::kLockScreenButtonTap);
  ASSERT_EQ(ash::mojom::TrayActionState::kLaunching,
            lock_screen_apps::StateController::Get()->GetLockScreenNoteState());

  ExtensionTestMessageListener expected_action_data("getExpectedActionData",
                                                    ReplyBehavior::kWillReply);

  ASSERT_TRUE(expected_action_data.WaitUntilSatisfied());
  expected_action_data.Reply(R"({"actionType": "new_note",
                                 "isLockScreenAction": true,
                                 "restoreLastActionState": true})");
  ASSERT_TRUE(result_catcher()->GetNextResult()) << result_catcher()->message();
  expected_action_data.Reset();

  // Reset the lock screen app state by resetting screen lock, so the app is
  // launchable again.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  profile()->GetPrefs()->SetBoolean(prefs::kRestoreLastLockScreenNote, false);

  lock_screen_apps::StateController::Get()->RequestNewLockScreenNote(
      ash::mojom::LockScreenNoteOrigin::kLockScreenButtonTap);
  ASSERT_EQ(ash::mojom::TrayActionState::kLaunching,
            lock_screen_apps::StateController::Get()->GetLockScreenNoteState());

  ASSERT_TRUE(expected_action_data.WaitUntilSatisfied());
  expected_action_data.Reply(R"({"actionType": "new_note",
                                 "isLockScreenAction": true,
                                 "restoreLastActionState": false})");
  ASSERT_TRUE(result_catcher()->GetNextResult()) << result_catcher()->message();
}
