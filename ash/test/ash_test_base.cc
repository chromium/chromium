// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/accessibility/ui/accessibility_panel_layout_manager.h"
#include "ash/ambient/test/ambient_ash_test_helper.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/extended_mouse_warp_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/display/unified_mouse_warp_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/pixel/ash_pixel_diff_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/test/test_widget_builder.h"
#include "ash/test/test_window_builder.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/work_area_insets.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
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
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

using session_manager::SessionState;

namespace ash {
namespace {

// Constants -------------------------------------------------------------------

constexpr char kKioskUserEmail[] =
    "fake_kiosk@kioks-apps.device-local.localhost";

// AshEventGeneratorDelegate ---------------------------------------------------

class AshEventGeneratorDelegate
    : public aura::test::EventGeneratorDelegateAura {
 public:
  AshEventGeneratorDelegate() = default;

  AshEventGeneratorDelegate(const AshEventGeneratorDelegate&) = delete;
  AshEventGeneratorDelegate& operator=(const AshEventGeneratorDelegate&) =
      delete;

  ~AshEventGeneratorDelegate() override = default;

  // aura::test::EventGeneratorDelegateAura overrides:
  ui::EventTarget* GetTargetAt(const gfx::Point& point_in_screen) override {
    display::Screen* screen = display::Screen::GetScreen();
    display::Display display = screen->GetDisplayNearestPoint(point_in_screen);
    if (current_display_id_ != display.id()) {
      Shell::Get()->cursor_manager()->SetDisplay(display);
      current_display_id_ = display.id();
    }
    return Shell::GetRootWindowForDisplayId(display.id())->GetHost()->window();
  }

 private:
  int64_t current_display_id_ = display::kInvalidDisplayId;
};

}  // namespace

// AshTestBase -----------------------------------------------------------------

AshTestBase::AshTestBase(
    std::unique_ptr<base::test::TaskEnvironment> task_environment)
    : task_environment_(std::move(task_environment)) {
  RegisterLocalStatePrefs(local_state_.registry(), true);
}

AshTestBase::~AshTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called AshTestBase::SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called AshTestBase::TearDown";
}

void AshTestBase::SetUp() {
  SetUp(nullptr);
}

void AshTestBase::SetUp(std::unique_ptr<TestShellDelegate> delegate) {
  // At this point, the task APIs should already be provided by
  // |task_environment_|.
  CHECK(base::SingleThreadTaskRunner::HasCurrentDefault());
  CHECK(base::ThreadPoolInstance::Get());

  setup_called_ = true;

  AshTestHelper::InitParams params;
  params.start_session = start_session_;
  params.create_global_cras_audio_handler = create_global_cras_audio_handler_;
  params.create_quick_pair_mediator = create_quick_pair_mediator_;
  params.delegate = std::move(delegate);
  params.local_state = local_state();

  // Prepare for a pixel test if having pixel init params.
  std::optional<pixel_test::InitParams> pixel_test_init_params =
      CreatePixelTestInitParams();
  if (pixel_test_init_params) {
    PrepareForPixelDiffTest();
    params.pixel_test_init_params = std::move(pixel_test_init_params);
  }

  test_context_factories_ =
      std::make_unique<ui::TestContextFactories>(/*enable_pixel_output=*/false);
  ash_test_helper_ = std::make_unique<AshTestHelper>(
      test_context_factories_->GetContextFactory());
  ash_test_helper_->SetUp(std::move(params));

  // Creates a dummy `SensorDisabledNotificationDelegate` to avoid a crash due
  // to it missing in tests.
  class DummyDelegate : public SensorDisabledNotificationDelegate {
    std::vector<std::u16string> GetAppsAccessingSensor(Sensor sensor) override {
      return {};
    }
  };
  scoped_disabled_notification_delegate_ =
      std::make_unique<ScopedSensorDisabledNotificationDelegateForTest>(
          std::make_unique<DummyDelegate>());
}

void AshTestBase::TearDown() {
  teardown_called_ = true;

  // We need to destroy the delegate while the Ash still exists.
  scoped_disabled_notification_delegate_.reset();

  // Make sure that we can exit tablet mode before shutdown correctly.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  Shell::Get()->session_controller()->NotifyChromeTerminating();

  // Flush the message loop to finish pending release tasks.
  base::RunLoop().RunUntilIdle();

  ash_test_helper_->TearDown();

  event_generator_.reset();
  // Some tests set an internal display id,
  // reset it here, so other tests will continue in a clean environment.
  display::SetInternalDisplayIds({display::kInvalidDisplayId});

  // Tests can add devices, so reset the lists for future tests.
  ui::DeviceDataManager::GetInstance()->ResetDeviceListsForTest();
}

// static
NotificationCenterTray* AshTestBase::GetPrimaryNotificationCenterTray() {
  return GetPrimaryShelf()->GetStatusAreaWidget()->notification_center_tray();
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

std::optional<pixel_test::InitParams> AshTestBase::CreatePixelTestInitParams()
    const {
  return std::nullopt;
}

void AshTestBase::UpdateDisplay(const std::string& display_specs,
                                bool from_native_platform,
                                bool generate_new_ids) {
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .UpdateDisplay(display_specs, from_native_platform, generate_new_ids);
  ScreenOrientationControllerTestApi(
      Shell::Get()->screen_orientation_controller())
      .UpdateNaturalOrientation();
}

aura::Window* AshTestBase::GetContext() {
  return ash_test_helper_->GetContext();
}

// static
std::unique_ptr<views::Widget> AshTestBase::CreateTestWidget(
    views::Widget::InitParams::Ownership ownership,
    views::WidgetDelegate* delegate,
    int container_id,
    const gfx::Rect& bounds,
    bool show) {
  TestWidgetBuilder builder;
  builder.SetDelegate(delegate)
      .SetBounds(bounds)
      .SetParent(Shell::GetPrimaryRootWindow()->GetChildById(container_id))
      .SetShow(show);
  if (ownership == views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET) {
    return builder.BuildOwnsNativeWidget();
  } else {
    DCHECK_EQ(ownership, views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    return builder.BuildClientOwnsWidget();
  }
}

// static
std::unique_ptr<views::Widget> AshTestBase::CreateFramelessTestWidget(
    views::Widget::InitParams::Ownership ownership) {
  TestWidgetBuilder builder;
  builder.SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  if (ownership == views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET) {
    return builder.BuildOwnsNativeWidget();
  } else {
    DCHECK_EQ(ownership, views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    return builder.BuildClientOwnsWidget();
  }
}

std::unique_ptr<aura::Window> AshTestBase::CreateAppWindow(
    const gfx::Rect& bounds_in_screen,
    chromeos::AppType app_type,
    int shell_window_id,
    views::WidgetDelegate* delegate) {
  TestWidgetBuilder builder;
  builder.SetWindowTitle(u"Window " + base::NumberToString16(shell_window_id));
  if (app_type != chromeos::AppType::NON_APP) {
    builder.SetWindowProperty(chromeos::kAppTypeKey, app_type);
  }

  if (delegate) {
    builder.SetDelegate(delegate);
  } else {
    builder.SetTestWidgetDelegate();
  }

  // |widget| is configured to be owned by the underlying window.
  views::Widget* widget =
      builder
          .SetBounds(bounds_in_screen.IsEmpty() ? gfx::Rect(0, 0, 300, 300)
                                                : bounds_in_screen)
          .SetContext(Shell::GetPrimaryRootWindow())
          .SetWindowId(shell_window_id)
          .BuildOwnedByNativeWidget();
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

  return CreateAppWindow(bounds_in_screen, chromeos::AppType::NON_APP,
                         shell_window_id);
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
  return TestWindowBuilder()
      .SetBounds(bounds)
      .SetDelegate(delegate)
      .SetWindowType(type)
      .SetWindowId(id)
      .SetWindowTitle(u"Window " + base::NumberToString16(id))
      .AllowAllWindowStates()
      .Build()
      .release();
}

void AshTestBase::ParentWindowInPrimaryRootWindow(aura::Window* window) {
  aura::client::ParentWindowWithContext(window, Shell::GetPrimaryRootWindow(),
                                        gfx::Rect(),
                                        display::kInvalidDisplayId);
}

AshPixelDiffer* AshTestBase::GetPixelDiffer() {
  DCHECK(pixel_differ_);
  return pixel_differ_.get();
}

void AshTestBase::StabilizeUIForPixelTest() {
  ash_test_helper_->StabilizeUIForPixelTest();
}

void AshTestBase::SetUserPref(const std::string& user_email,
                              const std::string& path,
                              const base::Value& value) {
  AccountId accountId = AccountId::FromUserEmail(user_email);
  PrefService* prefs =
      GetSessionControllerClient()->GetUserPrefService(accountId);
  prefs->Set(path, value);
}

TestSessionControllerClient* AshTestBase::GetSessionControllerClient() {
  return ash_test_helper_->test_session_controller_client();
}

TestSystemTrayClient* AshTestBase::GetSystemTrayClient() {
  return ash_test_helper_->system_tray_client();
}

AppListTestHelper* AshTestBase::GetAppListTestHelper() {
  return ash_test_helper_->app_list_test_helper();
}

TestAppListClient* AshTestBase::GetTestAppListClient() {
  return GetAppListTestHelper()->app_list_client();
}

AmbientAshTestHelper* AshTestBase::GetAmbientAshTestHelper() {
  return ash_test_helper_->ambient_ash_test_helper();
}

void AshTestBase::CreateUserSessions(int n) {
  GetSessionControllerClient()->CreatePredefinedUserSessions(n);
}

void AshTestBase::SimulateUserLogin(const std::string& user_email,
                                    user_manager::UserType user_type) {
  SimulateUserLogin(AccountId::FromUserEmail(user_email), user_type);
}

void AshTestBase::SimulateUserLogin(const AccountId& account_id,
                                    user_manager::UserType user_type) {
  ash_test_helper_->SimulateUserLogin(account_id, user_type);
}

void AshTestBase::SimulateNewUserFirstLogin(const std::string& user_email) {
  ash_test_helper_->SimulateUserLogin(AccountId::FromUserEmail(user_email),
                                      user_manager::UserType::kRegular,
                                      /*is_new_profile=*/true);
}

void AshTestBase::SimulateGuestLogin() {
  SimulateUserLogin(AccountId::FromUserEmail(user_manager::kGuestUserName),
                    user_manager::UserType::kGuest);
}

void AshTestBase::SimulateKioskMode(user_manager::UserType user_type) {
  DCHECK(user_type == user_manager::UserType::kKioskApp ||
         user_type == user_manager::UserType::kWebKioskApp);

  GetSessionControllerClient()->SetIsRunningInAppMode(true);
  SimulateUserLogin(AccountId::FromUserEmail(kKioskUserEmail), user_type);
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
  }
}

void AshTestBase::UnblockUserSession() {
  CreateUserSessions(1);
  GetSessionControllerClient()->UnlockScreen();
}

void AshTestBase::SetVirtualKeyboardEnabled(bool enabled) {
  // Note there are a lot of flags that can be set to control whether the
  // keyboard is shown or not. You can see the logic in
  // |KeyboardUIController::IsKeyboardEnableRequested|.
  // The |kTouchEnabled| flag seems like a logical candidate to pick, but it
  // does not work because the flag will automatically be toggled off once the
  // |DeviceDataManager| detects there is a physical keyboard present. That's
  // why I picked the |kPolicyEnabled| and |kPolicyDisabled| flags instead.
  auto enable_flag = keyboard::KeyboardEnableFlag::kPolicyEnabled;
  auto disable_flag = keyboard::KeyboardEnableFlag::kPolicyDisabled;
  auto* keyboard_controller = Shell::Get()->keyboard_controller();

  if (enabled) {
    keyboard_controller->SetEnableFlag(enable_flag);
    keyboard_controller->ClearEnableFlag(disable_flag);
  } else {
    keyboard_controller->ClearEnableFlag(enable_flag);
    keyboard_controller->SetEnableFlag(disable_flag);
  }
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

void AshTestBase::PressAndReleaseKey(ui::KeyboardCode key_code, int flags) {
  GetEventGenerator()->PressAndReleaseKey(key_code, flags);
}

void AshTestBase::LeftClickOn(const views::View* view) {
  DCHECK(view);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
}

void AshTestBase::RightClickOn(const views::View* view) {
  DCHECK(view);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator->ClickRightButton();
}

void AshTestBase::GestureTapOn(const views::View* view) {
  DCHECK(view);
  auto* event_generator = GetEventGenerator();
  event_generator->GestureTapAt(view->GetBoundsInScreen().CenterPoint());
}

bool AshTestBase::EnterOverview(OverviewEnterExitType type) {
  if (OverviewController::Get()->StartOverview(OverviewStartAction::kTests,
                                               type)) {
    // After entering overview mode, the views created for the desk bar require
    // an immediate layout. Layout is normally driven by the compositor, but
    // this does not occur in unit tests. Therefore,
    // `views::test::RunScheduledLayout()` must be called manually.
    RunScheduledLayoutForAllOverviewDeskBars();
    return true;
  }
  return false;
}

bool AshTestBase::ExitOverview(OverviewEnterExitType type) {
  return OverviewController::Get()->EndOverview(OverviewEndAction::kTests,
                                                type);
}

void AshTestBase::SetShelfAnimationDuration(base::TimeDelta duration) {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    ShelfViewTestAPI(root_window_controller->shelf()->GetShelfViewForTesting())
        .SetAnimationDuration(duration);
  }
}

void AshTestBase::WaitForShelfAnimation() {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    ShelfViewTestAPI(root_window_controller->shelf()->GetShelfViewForTesting())
        .RunMessageLoopUntilAnimationsDone();
  }
}

void AshTestBase::MaybeRunDragAndDropSequenceForAppList(
    std::list<base::OnceClosure>* tasks,
    bool is_touch) {
  // The app list drag and drop require this extra step since drag actually
  // starts when the cursor is moved. In the case of the drag and drop refactor,
  // this movement is done outside of the LoopClosure, but a second one is
  // required since OnDragEnter() is invoked when the drag is updated.
  tasks->push_front(base::BindLambdaForTesting([&]() {
    if (is_touch) {
      GetEventGenerator()->MoveTouchBy(10, 10);
      return;
    }
    GetEventGenerator()->MoveMouseBy(10, 10);
  }));

  ShellTestApi().drag_drop_controller()->SetLoopClosureForTesting(
      base::BindLambdaForTesting([&]() {
        std::move(tasks->front()).Run();
        tasks->pop_front();
      }),
      base::DoNothing());

  if (is_touch) {
    GetEventGenerator()->MoveTouchBy(10, 10);
    return;
  }
  GetEventGenerator()->MoveMouseBy(10, 10);
}

void AshTestBase::SwapPrimaryDisplay() {
  if (display::Screen::GetScreen()->GetNumDisplays() <= 1)
    return;
  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id());
}

display::Display AshTestBase::GetPrimaryDisplay() const {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(
      Shell::GetPrimaryRootWindow());
}

display::Display AshTestBase::GetSecondaryDisplay() const {
  return ash_test_helper_->GetSecondaryDisplay();
}

void AshTestBase::PrepareForPixelDiffTest() {
  // In pixel tests, we want to take screenshots then compare them with the
  // benchmark images. Therefore, enable pixel output in tests.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnablePixelOutputInTests);

  // Enable the switch so that the time dependent views (such as the time view)
  // are stable.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kStabilizeTimeDependentViewForTests);

  // Enable the dark mode switch to maintain the dark mode before user login.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kForceDarkMode);

  DCHECK(!pixel_differ_);
  pixel_differ_ =
      std::make_unique<AshPixelDiffer>(GetScreenshotPrefixForCurrentTestInfo());
}

// ============================================================================
// NoSessionAshTestBase:

NoSessionAshTestBase::NoSessionAshTestBase() {
  set_start_session(false);
}

NoSessionAshTestBase::NoSessionAshTestBase(
    base::test::TaskEnvironment::TimeSource time_source)
    : AshTestBase(time_source) {
  set_start_session(false);
}

NoSessionAshTestBase::~NoSessionAshTestBase() = default;

}  // namespace ash
