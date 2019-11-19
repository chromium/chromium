// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/accessibility/accessibility_panel_layout_manager.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/display/extended_mouse_warp_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/display/unified_mouse_warp_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell/toplevel_window.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_screenshot_delegate.h"
#include "ash/test_shell_delegate.h"
#include "ash/utility/screenshot_controller.h"
#include "ash/window_factory.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/work_area_insets.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

using session_manager::SessionState;

namespace ash {
namespace {

class AshEventGeneratorDelegate
    : public aura::test::EventGeneratorDelegateAura {
 public:
  AshEventGeneratorDelegate() = default;
  ~AshEventGeneratorDelegate() override = default;

  // aura::test::EventGeneratorDelegateAura overrides:
  ui::EventTarget* GetTargetAt(const gfx::Point& point_in_screen) override {
    display::Screen* screen = display::Screen::GetScreen();
    display::Display display = screen->GetDisplayNearestPoint(point_in_screen);
    return Shell::GetRootWindowForDisplayId(display.id())->GetHost()->window();
  }

  ui::EventDispatchDetails DispatchKeyEventToIME(ui::EventTarget* target,
                                                 ui::KeyEvent* event) override {
    // In Ash environment, the key event will be processed by event rewriters
    // first.
    return ui::EventDispatchDetails();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AshEventGeneratorDelegate);
};

// WidgetDelegate that is resizable and creates ash's NonClientFrameView
// implementation.
class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate() = default;
  ~TestWidgetDelegate() override = default;

  // views::WidgetDelegateView:
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    return Shell::Get()->CreateDefaultNonClientFrameView(widget);
  }
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

}  // namespace

/////////////////////////////////////////////////////////////////////////////

AshTestBase::AshTestBase(AshTestBase::SubclassManagesTaskEnvironment /* tag */)
    : task_environment_(base::nullopt) {}

AshTestBase::~AshTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called AshTestBase::SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called AshTestBase::TearDown";
}

void AshTestBase::SetUp() {
  // At this point, the task APIs should already be provided either by
  // |task_environment_| or by the subclass in the
  // SubclassManagesTaskEnvironment mode.
  CHECK(base::ThreadTaskRunnerHandle::IsSet());
  CHECK(base::ThreadPoolInstance::Get());

  setup_called_ = true;

  // Clears the saved state so that test doesn't use on the wrong
  // default state.
  shell::ToplevelWindow::ClearSavedStateForTest();

  AshTestHelper::InitParams params;
  params.start_session = start_session_;
  params.provide_local_state = provide_local_state_;
  params.config_type = AshTestHelper::kUnitTest;
  ash_test_helper_.SetUp(params);

  Shell::GetPrimaryRootWindow()->Show();
  Shell::GetPrimaryRootWindow()->GetHost()->Show();
  // Move the mouse cursor to far away so that native events doesn't
  // interfere test expectations.
  Shell::GetPrimaryRootWindow()->MoveCursorTo(gfx::Point(-1000, -1000));
  Shell::Get()->cursor_manager()->EnableMouseEvents();

  // Changing GestureConfiguration shouldn't make tests fail. These values
  // prevent unexpected events from being generated during tests. Such as
  // delayed events which create race conditions on slower tests.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_max_touch_down_duration_for_click_in_ms(800);
  gesture_config->set_long_press_time_in_ms(1000);
  gesture_config->set_max_touch_move_in_pixels_for_click(5);
}

void AshTestBase::TearDown() {
  teardown_called_ = true;
  // Make sure that we can exit tablet mode before shutdown correctly.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  Shell::Get()->session_controller()->NotifyChromeTerminating();

  // Flush the message loop to finish pending release tasks.
  base::RunLoop().RunUntilIdle();

  ash_test_helper_.TearDown();

  event_generator_.reset();
  // Some tests set an internal display id,
  // reset it here, so other tests will continue in a clean environment.
  display::Display::SetInternalDisplayId(display::kInvalidDisplayId);

  // Tests can add devices, so reset the lists for future tests.
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({});
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
}

// static
Shelf* AshTestBase::GetPrimaryShelf() {
  return Shell::GetPrimaryRootWindowController()->shelf();
}

// static
UnifiedSystemTray* AshTestBase::GetPrimaryUnifiedSystemTray() {
  return GetPrimaryShelf()->GetStatusAreaWidget()->unified_system_tray();
}

// static
WorkAreaInsets* AshTestBase::GetPrimaryWorkAreaInsets() {
  return Shell::GetPrimaryRootWindowController()->work_area_insets();
}

ui::test::EventGenerator* AshTestBase::GetEventGenerator() {
  if (!event_generator_) {
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        std::make_unique<AshEventGeneratorDelegate>());
  }
  return event_generator_.get();
}

// static
display::Display::Rotation AshTestBase::GetActiveDisplayRotation(int64_t id) {
  return Shell::Get()
      ->display_manager()
      ->GetDisplayInfo(id)
      .GetActiveRotation();
}

// static
display::Display::Rotation AshTestBase::GetCurrentInternalDisplayRotation() {
  return GetActiveDisplayRotation(display::Display::InternalDisplayId());
}

void AshTestBase::UpdateDisplay(const std::string& display_specs) {
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .UpdateDisplay(display_specs);
  ScreenOrientationControllerTestApi(
      Shell::Get()->screen_orientation_controller())
      .UpdateNaturalOrientation();
}

aura::Window* AshTestBase::CurrentContext() {
  return ash_test_helper_.CurrentContext();
}

// static
std::unique_ptr<views::Widget> AshTestBase::CreateTestWidget(
    views::WidgetDelegate* delegate,
    int container_id,
    const gfx::Rect& bounds,
    bool show) {
  std::unique_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params;
  params.delegate = delegate;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = bounds;
  params.parent = Shell::GetPrimaryRootWindow()->GetChildById(container_id);
  widget->Init(std::move(params));
  if (show)
    widget->Show();
  return widget;
}

std::unique_ptr<aura::Window> AshTestBase::CreateAppWindow(
    const gfx::Rect& bounds_in_screen,
    AppType app_type,
    int shell_window_id) {
  // |widget| is configured to be owned by the underlying window.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  // TestWidgetDelegate is owned by |widget|.
  params.delegate = new TestWidgetDelegate();
  params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  params.bounds =
      bounds_in_screen.IsEmpty() ? gfx::Rect(0, 0, 300, 300) : bounds_in_screen;
  params.context = Shell::GetPrimaryRootWindow();
  if (app_type != AppType::NON_APP) {
    params.init_properties_container.SetProperty(aura::client::kAppType,
                                                 static_cast<int>(app_type));
  }
  widget->Init(std::move(params));
  widget->GetNativeWindow()->set_id(shell_window_id);
  widget->Show();
  return base::WrapUnique(widget->GetNativeWindow());
}

std::unique_ptr<aura::Window> AshTestBase::CreateTestWindow(
    const gfx::Rect& bounds_in_screen,
    aura::client::WindowType type,
    int shell_window_id) {
  if (type != aura::client::WINDOW_TYPE_NORMAL) {
    return base::WrapUnique(CreateTestWindowInShellWithDelegateAndType(
        nullptr, type, shell_window_id, bounds_in_screen));
  }

  return CreateAppWindow(bounds_in_screen, AppType::NON_APP, shell_window_id);
}

std::unique_ptr<aura::Window> AshTestBase::CreateToplevelTestWindow(
    const gfx::Rect& bounds_in_screen,
    int shell_window_id) {
  aura::test::TestWindowDelegate* delegate =
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
  return base::WrapUnique<aura::Window>(
      CreateTestWindowInShellWithDelegateAndType(
          delegate, aura::client::WINDOW_TYPE_NORMAL, shell_window_id,
          bounds_in_screen));
}

aura::Window* AshTestBase::CreateTestWindowInShellWithId(int id) {
  return CreateTestWindowInShellWithDelegate(NULL, id, gfx::Rect());
}

aura::Window* AshTestBase::CreateTestWindowInShellWithBounds(
    const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegate(NULL, 0, bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShell(SkColor color,
                                                   int id,
                                                   const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegate(
      new aura::test::ColorTestWindowDelegate(color), id, bounds);
}

std::unique_ptr<aura::Window> AshTestBase::CreateChildWindow(
    aura::Window* parent,
    const gfx::Rect& bounds,
    int shell_window_id) {
  std::unique_ptr<aura::Window> window =
      window_factory::NewWindow(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetBounds(bounds);
  window->set_id(shell_window_id);
  parent->AddChild(window.get());
  window->Show();
  return window;
}

aura::Window* AshTestBase::CreateTestWindowInShellWithDelegate(
    aura::WindowDelegate* delegate,
    int id,
    const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegateAndType(
      delegate, aura::client::WINDOW_TYPE_NORMAL, id, bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShellWithDelegateAndType(
    aura::WindowDelegate* delegate,
    aura::client::WindowType type,
    int id,
    const gfx::Rect& bounds) {
  aura::Window* window = window_factory::NewWindow(delegate).release();
  window->set_id(id);
  window->SetType(type);
  window->Init(ui::LAYER_TEXTURED);

  if (bounds.IsEmpty()) {
    ParentWindowInPrimaryRootWindow(window);
  } else {
    display::Display display =
        display::Screen::GetScreen()->GetDisplayMatching(bounds);
    aura::Window* root = Shell::GetRootWindowForDisplayId(display.id());
    gfx::Point origin = bounds.origin();
    ::wm::ConvertPointFromScreen(root, &origin);
    window->SetBounds(gfx::Rect(origin, bounds.size()));
    aura::client::ParentWindowWithContext(window, root, bounds);
  }
  window->Show();

  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize |
                          aura::client::kResizeBehaviorCanMinimize |
                          aura::client::kResizeBehaviorCanResize);
  return window;
}

void AshTestBase::ParentWindowInPrimaryRootWindow(aura::Window* window) {
  aura::client::ParentWindowWithContext(window, Shell::GetPrimaryRootWindow(),
                                        gfx::Rect());
}

TestScreenshotDelegate* AshTestBase::GetScreenshotDelegate() {
  return static_cast<TestScreenshotDelegate*>(
      Shell::Get()->screenshot_controller()->screenshot_delegate_.get());
}

TestSessionControllerClient* AshTestBase::GetSessionControllerClient() {
  return ash_test_helper_.test_session_controller_client();
}

TestSystemTrayClient* AshTestBase::GetSystemTrayClient() {
  return ash_test_helper_.system_tray_client();
}

AppListTestHelper* AshTestBase::GetAppListTestHelper() {
  return ash_test_helper_.app_list_test_helper();
}

void AshTestBase::CreateUserSessions(int n) {
  GetSessionControllerClient()->CreatePredefinedUserSessions(n);
}

void AshTestBase::SimulateUserLogin(const std::string& user_email,
                                    user_manager::UserType user_type) {
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->AddUserSession(user_email, user_type);
  session->SwitchActiveUser(AccountId::FromUserEmail(user_email));
  session->SetSessionState(SessionState::ACTIVE);
}

void AshTestBase::SimulateNewUserFirstLogin(const std::string& user_email) {
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->AddUserSession(
      user_email, user_manager::USER_TYPE_REGULAR, true /* enable_settings */,
      true /* provide_pref_service */, true /* is_new_profile */);
  session->SwitchActiveUser(AccountId::FromUserEmail(user_email));
  session->SetSessionState(session_manager::SessionState::ACTIVE);
}

void AshTestBase::SimulateGuestLogin() {
  const std::string guest = user_manager::kGuestUserName;
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->AddUserSession(guest, user_manager::USER_TYPE_GUEST);
  session->SwitchActiveUser(AccountId::FromUserEmail(guest));
  session->SetSessionState(SessionState::ACTIVE);
}

void AshTestBase::SimulateKioskMode(user_manager::UserType user_type) {
  DCHECK(user_type == user_manager::USER_TYPE_ARC_KIOSK_APP ||
         user_type == user_manager::USER_TYPE_KIOSK_APP);

  const std::string user_email = "fake_kiosk@kioks-apps.device-local.localhost";
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->SetIsRunningInAppMode(true);
  session->AddUserSession(user_email, user_type);
  session->SwitchActiveUser(AccountId::FromUserEmail(user_email));
  session->SetSessionState(SessionState::ACTIVE);
}

void AshTestBase::SetAccessibilityPanelHeight(int panel_height) {
  Shell::GetPrimaryRootWindowController()
      ->GetAccessibilityPanelLayoutManagerForTest()
      ->SetPanelBounds(gfx::Rect(0, 0, 0, panel_height),
                       AccessibilityPanelState::FULL_WIDTH);
}

void AshTestBase::ClearLogin() {
  GetSessionControllerClient()->Reset();
}

void AshTestBase::SetCanLockScreen(bool can_lock) {
  GetSessionControllerClient()->SetCanLockScreen(can_lock);
}

void AshTestBase::SetShouldLockScreenAutomatically(bool should_lock) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(should_lock);
}

void AshTestBase::SetUserAddingScreenRunning(bool user_adding_screen_running) {
  GetSessionControllerClient()->SetSessionState(
      user_adding_screen_running ? SessionState::LOGIN_SECONDARY
                                 : SessionState::ACTIVE);
}

void AshTestBase::BlockUserSession(UserSessionBlockReason block_reason) {
  switch (block_reason) {
    case BLOCKED_BY_LOCK_SCREEN:
      CreateUserSessions(1);
      GetSessionControllerClient()->LockScreen();
      break;
    case BLOCKED_BY_LOGIN_SCREEN:
      ClearLogin();
      break;
    case BLOCKED_BY_USER_ADDING_SCREEN:
      SetUserAddingScreenRunning(true);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AshTestBase::UnblockUserSession() {
  CreateUserSessions(1);
  GetSessionControllerClient()->UnlockScreen();
}

void AshTestBase::SetTouchKeyboardEnabled(bool enabled) {
  auto flag = keyboard::KeyboardEnableFlag::kTouchEnabled;
  if (enabled)
    Shell::Get()->keyboard_controller()->SetEnableFlag(flag);
  else
    Shell::Get()->keyboard_controller()->ClearEnableFlag(flag);
  // Ensure that observer methods and mojo calls between KeyboardControllerImpl,
  // keyboard::KeyboardUIController*, and AshKeyboardUI complete.
  base::RunLoop().RunUntilIdle();
}

void AshTestBase::DisableIME() {
  aura::test::DisableIME(Shell::GetPrimaryRootWindow()->GetHost());
}

display::DisplayManager* AshTestBase::display_manager() {
  return Shell::Get()->display_manager();
}

chromeos::FakePowerManagerClient* AshTestBase::power_manager_client() const {
  return chromeos::FakePowerManagerClient::Get();
}

bool AshTestBase::TestIfMouseWarpsAt(ui::test::EventGenerator* event_generator,
                                     const gfx::Point& point_in_screen) {
  DCHECK(!Shell::Get()->display_manager()->IsInUnifiedMode());
  static_cast<ExtendedMouseWarpController*>(
      Shell::Get()->mouse_cursor_filter()->mouse_warp_controller_for_test())
      ->allow_non_native_event_for_test();
  display::Screen* screen = display::Screen::GetScreen();
  display::Display original_display =
      screen->GetDisplayNearestPoint(point_in_screen);
  event_generator->MoveMouseTo(point_in_screen);
  return original_display.id() !=
         screen
             ->GetDisplayNearestPoint(
                 aura::Env::GetInstance()->last_mouse_location())
             .id();
}

void AshTestBase::SwapPrimaryDisplay() {
  if (display::Screen::GetScreen()->GetNumDisplays() <= 1)
    return;
  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
      display_manager()->GetSecondaryDisplay().id());
}

display::Display AshTestBase::GetPrimaryDisplay() const {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(
      Shell::GetPrimaryRootWindow());
}

display::Display AshTestBase::GetSecondaryDisplay() const {
  return ash_test_helper_.GetSecondaryDisplay();
}

}  // namespace ash
