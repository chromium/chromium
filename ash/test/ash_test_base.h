// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_BASE_H_
#define ASH_TEST_ASH_TEST_BASE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_types.h"
#include "base/compiler_specific.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/traits_bag.h"
#include "chromeos/ui/base/app_types.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/env.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/display/display.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
class WindowDelegate;
}  // namespace aura

namespace base {
namespace test {
class TaskEnvironment;
}
}  // namespace base

namespace chromeos {
class FakePowerManagerClient;
}

namespace display {
class Display;
class DisplayManager;

namespace test {
class DisplayManagerTestApi;
}  // namespace test
}  // namespace display

namespace gfx {
class Rect;
}

namespace ash {

class AmbientAshTestHelper;
class AppListTestHelper;
class AshPixelDiffer;
class AshTestHelper;
class NotificationCenterTray;
class Shelf;
class TestAppListClient;
class TestShellDelegate;
class TestSystemTrayClient;
class UnifiedSystemTray;
class WorkAreaInsets;

// Base class for most tests in //ash. Constructs ash::Shell and all its
// dependencies. Provides a user login session (use NoSessionAshTestBase for
// tests that start at the login screen or need unusual user types). Sets
// animation durations to zero via AshTestHelper/AuraTestHelper.
class AshTestBase : public testing::Test {
 public:
  // Constructs an AshTestBase with |traits| being forwarded to its
  // TaskEnvironment. MainThreadType always defaults to UI and must not be
  // specified.
  template <typename... TaskEnvironmentTraits>
  NOINLINE explicit AshTestBase(TaskEnvironmentTraits&&... traits)
      : AshTestBase(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI,
            std::forward<TaskEnvironmentTraits>(traits)...)) {}

  // Alternatively a subclass may pass a TaskEnvironment directly.
  explicit AshTestBase(
      std::unique_ptr<base::test::TaskEnvironment> task_environment);

  AshTestBase(const AshTestBase&) = delete;
  AshTestBase& operator=(const AshTestBase&) = delete;

  ~AshTestBase() override;

  // testing::Test:
  void SetUp() override;
  void SetUp(std::unique_ptr<TestShellDelegate> delegate);
  void TearDown() override;

  // Returns the notification center tray on the primary display.
  static NotificationCenterTray* GetPrimaryNotificationCenterTray();

  // Returns the Shelf for the primary display.
  static Shelf* GetPrimaryShelf();

  // Returns the unified system tray on the primary display.
  static UnifiedSystemTray* GetPrimaryUnifiedSystemTray();

  // Returns WorkAreaInsets for the primary display.
  static WorkAreaInsets* GetPrimaryWorkAreaInsets();

  // Update the display configuration as given in |display_specs|.
  // See ash::DisplayManagerTestApi::UpdateDisplay for more details.
  // Note: To properly specify the radii of display's panel upon startup, set it
  // via specifying the command line switch `ash-host-window-bounds`.
  void UpdateDisplay(const std::string& display_specs,
                     bool from_native_platform = false,
                     bool generate_new_ids = false);

  // Returns a root Window. Usually this is the active root Window, but that
  // method can return NULL sometimes, and in those cases, we fall back on the
  // primary root Window.
  aura::Window* GetContext();

  // Creates and shows a widget. See ash/public/cpp/shell_window_ids.h for
  // values for |container_id|.
  static std::unique_ptr<views::Widget> CreateTestWidget(
      views::Widget::InitParams::Ownership ownership,
      views::WidgetDelegate* delegate = nullptr,
      int container_id = desks_util::GetActiveDeskContainerId(),
      const gfx::Rect& bounds = gfx::Rect(),
      bool show = true);

  // Creates a frameless widget for testing.
  // TODO(crbug.com/339619005) - Make the ownership parameter required.
  static std::unique_ptr<views::Widget> CreateFramelessTestWidget(
      views::Widget::InitParams::Ownership ownership =
          views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  // Creates a widget with a visible WINDOW_TYPE_NORMAL window with the given
  // |app_type|. If |app_type| is chromeos::AppType::NON_APP, this window is
  // considered a non-app window. If |bounds_in_screen| is empty the window is
  // added to the primary root window, otherwise the window is added to the
  // display matching |bounds_in_screen|. |shell_window_id| is the shell window
  // id to give to the new window. If |delegate| is empty, a new
  // |TestWidgetDelegate| instance will be set as this widget's delegate.
  std::unique_ptr<aura::Window> CreateAppWindow(
      const gfx::Rect& bounds_in_screen = gfx::Rect(),
      chromeos::AppType app_type = chromeos::AppType::SYSTEM_APP,
      int shell_window_id = kShellWindowId_Invalid,
      views::WidgetDelegate* delegate = nullptr);

  // Creates a visible window in the appropriate container. If
  // |bounds_in_screen| is empty the window is added to the primary root
  // window, otherwise the window is added to the display matching
  // |bounds_in_screen|. |shell_window_id| is the shell window id to give to
  // the new window.
  // If |type| is WINDOW_TYPE_NORMAL this creates a views::Widget, otherwise
  // this creates an aura::Window.
  std::unique_ptr<aura::Window> CreateTestWindow(
      const gfx::Rect& bounds_in_screen = gfx::Rect(),
      aura::client::WindowType type = aura::client::WINDOW_TYPE_NORMAL,
      int shell_window_id = kShellWindowId_Invalid);

  // Creates a visible top-level window with a delegate.
  std::unique_ptr<aura::Window> CreateToplevelTestWindow(
      const gfx::Rect& bounds_in_screen = gfx::Rect(),
      int shell_window_id = kShellWindowId_Invalid);

  // Versions of the functions in aura::test:: that go through our shell
  // StackingController instead of taking a parent.
  aura::Window* CreateTestWindowInShellWithId(int id);
  aura::Window* CreateTestWindowInShellWithBounds(const gfx::Rect& bounds);
  aura::Window* CreateTestWindowInShellWithDelegate(
      aura::WindowDelegate* delegate,
      int id,
      const gfx::Rect& bounds);
  aura::Window* CreateTestWindowInShellWithDelegateAndType(
      aura::WindowDelegate* delegate,
      aura::client::WindowType type,
      int id,
      const gfx::Rect& bounds);

  // Attach |window| to the current shell's root window.
  void ParentWindowInPrimaryRootWindow(aura::Window* window);

  // Returns the raw pointer carried by `pixel_differ_`.
  AshPixelDiffer* GetPixelDiffer();

  // Stabilizes the variable UI components (such as the battery view). It should
  // be called after the active user changes since some UI components are
  // associated with the active account.
  void StabilizeUIForPixelTest();

  // Returns the EventGenerator that uses screen coordinates and works
  // across multiple displays. It creates a new generator if it
  // hasn't been created yet.
  ui::test::EventGenerator* GetEventGenerator();

  // Convenience method to return the DisplayManager.
  display::DisplayManager* display_manager();

  // Convenience method to return the FakePowerManagerClient.
  chromeos::FakePowerManagerClient* power_manager_client() const;

  // Test if moving a mouse to |point_in_screen| warps it to another
  // display.
  bool TestIfMouseWarpsAt(ui::test::EventGenerator* event_generator,
                          const gfx::Point& point_in_screen);

  // Presses and releases a key to simulate typing one character.
  void PressAndReleaseKey(ui::KeyboardCode key_code, int flags = ui::EF_NONE);

  // Moves the mouse to the center of the view and generates a left mouse button
  // click event.
  void LeftClickOn(const views::View* view);

  // Moves the mouse to the center of the view and generates a right mouse
  // button click event.
  void RightClickOn(const views::View* view);

  // Generates a tap event on the center of `view`.
  void GestureTapOn(const views::View* view);

  // Enters/Exits overview mode with the given animation type `type`.
  bool EnterOverview(
      OverviewEnterExitType type = OverviewEnterExitType::kNormal);
  bool ExitOverview(
      OverviewEnterExitType type = OverviewEnterExitType::kNormal);

  // Sets shelf animation duration for all displays.
  void SetShelfAnimationDuration(base::TimeDelta duration);

  // Waits for shelf animation in all displays.
  void WaitForShelfAnimation();

  // Execute a list of tasks during a drag and drop sequence in the apps grid.
  // This method should be called after the drag is initiated by long pressing
  // over an app but before actually moving the pointer to drag the item. When
  // the drag and drop sequence is not handled by DragDropController, the list
  // of tasks is just run sequentially outside the loop
  void MaybeRunDragAndDropSequenceForAppList(
      std::list<base::OnceClosure>* tasks,
      bool is_touch);

 protected:
  enum UserSessionBlockReason {
    FIRST_BLOCK_REASON,
    BLOCKED_BY_LOCK_SCREEN = FIRST_BLOCK_REASON,
    BLOCKED_BY_LOGIN_SCREEN,
    BLOCKED_BY_USER_ADDING_SCREEN,
    NUMBER_OF_BLOCK_REASONS
  };

  // Returns the rotation currentl active for the display |id|.
  static display::Display::Rotation GetActiveDisplayRotation(int64_t id);

  // Returns the rotation currently active for the internal display.
  static display::Display::Rotation GetCurrentInternalDisplayRotation();

  // Creates init params to set up a pixel test. If the test is not pixel
  // related, returns `std::nullopt`. This function should be overridden by ash
  // pixel tests.
  virtual std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const;

  void set_start_session(bool start_session) { start_session_ = start_session; }

  void set_create_global_cras_audio_handler(
      bool create_global_cras_audio_handler) {
    create_global_cras_audio_handler_ = create_global_cras_audio_handler;
  }

  void set_create_quick_pair_mediator(bool create_quick_pair_mediator) {
    create_quick_pair_mediator_ = create_quick_pair_mediator;
  }

  base::test::TaskEnvironment* task_environment() {
    return task_environment_.get();
  }
  TestingPrefServiceSimple* local_state() { return &local_state_; }
  AshTestHelper* ash_test_helper() { return ash_test_helper_.get(); }

  // Returns nullptr before SetUp() is called.
  ui::InProcessContextFactory* GetContextFactory() {
    return test_context_factories_
               ? test_context_factories_->GetContextFactory()
               : nullptr;
  }

  void SetUserPref(const std::string& user_email,
                   const std::string& path,
                   const base::Value& value);

  TestSessionControllerClient* GetSessionControllerClient();

  TestSystemTrayClient* GetSystemTrayClient();

  AppListTestHelper* GetAppListTestHelper();

  TestAppListClient* GetTestAppListClient();

  AmbientAshTestHelper* GetAmbientAshTestHelper();

  // Emulates an ash session that have |session_count| user sessions running.
  // Note that existing user sessions will be cleared.
  void CreateUserSessions(int session_count);

  // Simulates a user sign-in. It creates a new user session, adds it to
  // existing user sessions and makes it the active user session.
  //
  // For convenience |user_email| is used to create an |AccountId|. For testing
  // behavior where |AccountId|s are compared, prefer the method of the same
  // name that takes an |AccountId| created with a valid storage key instead.
  // See the documentation for|AccountId::GetUserEmail| for discussion.
  void SimulateUserLogin(
      const std::string& user_email,
      user_manager::UserType user_type = user_manager::UserType::kRegular);

  // Simulates a user sign-in. It creates a new user session, adds it to
  // existing user sessions and makes it the active user session.
  void SimulateUserLogin(
      const AccountId& account_id,
      user_manager::UserType user_type = user_manager::UserType::kRegular);

  // Simular to SimulateUserLogin but for a newly created user first ever login.
  void SimulateNewUserFirstLogin(const std::string& user_email);

  // Similar to SimulateUserLogin but for a guest user.
  void SimulateGuestLogin();

  // Simulates kiosk mode. |user_type| must correlate to a kiosk type user.
  void SimulateKioskMode(user_manager::UserType user_type);

  // Simulates setting height of the accessibility panel.
  // Note: Accessibility panel widget needs to be setup first.
  void SetAccessibilityPanelHeight(int panel_height);

  // Clears all user sessions and resets to the primary login screen state.
  void ClearLogin();

  // Emulates whether the active user can lock screen.
  void SetCanLockScreen(bool can_lock);

  // Emulates whether the screen should be locked automatically.
  void SetShouldLockScreenAutomatically(bool should_lock);

  // Emulates whether the user adding screen is running.
  void SetUserAddingScreenRunning(bool user_adding_screen_running);

  // Methods to emulate blocking and unblocking user session with given
  // |block_reason|.
  void BlockUserSession(UserSessionBlockReason block_reason);
  void UnblockUserSession();

  // Enable or disable the virtual on-screen keyboard and run the message loop
  // to allow observer operations to complete.
  void SetVirtualKeyboardEnabled(bool enabled);

  void DisableIME();

  // Swap the primary display with the secondary.
  void SwapPrimaryDisplay();

  display::Display GetPrimaryDisplay() const;
  display::Display GetSecondaryDisplay() const;

 private:
  void CreateWindowTreeIfNecessary();

  // Prepares for pixel tests by enabling related flags and building
  // `ash_test_helper_`.
  void PrepareForPixelDiffTest();

  bool setup_called_ = false;
  bool teardown_called_ = false;

  // SetUp() doesn't activate session if this is set to false.
  bool start_session_ = true;

  // `SetUp()` doesn't create a global `CrasAudioHandler` instance if this is
  // set to false.
  bool create_global_cras_audio_handler_ = true;

  // `SetUp()` doesn't create a global `QuickPairMediator` instance if this is
  // set to false.
  bool create_quick_pair_mediator_ = true;

  // |task_environment_| is initialized-once at construction time but
  // subclasses may elect to provide their own.
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

  // A pref service used for local state.
  TestingPrefServiceSimple local_state_;

  // A helper class to take screen shots then compare with benchmarks. Set by
  // `PrepareForPixelDiffTest()`.
  std::unique_ptr<AshPixelDiffer> pixel_differ_;

  std::unique_ptr<ui::TestContextFactories> test_context_factories_;

  // Must be constructed after |task_environment_|.
  std::unique_ptr<AshTestHelper> ash_test_helper_;

  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  std::unique_ptr<ScopedSensorDisabledNotificationDelegateForTest>
      scoped_disabled_notification_delegate_;
};

class NoSessionAshTestBase : public AshTestBase {
 public:
  NoSessionAshTestBase();
  explicit NoSessionAshTestBase(
      base::test::TaskEnvironment::TimeSource time_source);

  NoSessionAshTestBase(const NoSessionAshTestBase&) = delete;
  NoSessionAshTestBase& operator=(const NoSessionAshTestBase&) = delete;

  ~NoSessionAshTestBase() override;
};

}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_BASE_H_
