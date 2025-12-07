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
#include "ash/display/screen_ash.h"
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
#include "ash/test/login_info.h"
#include "ash/test/pixel/ash_pixel_diff_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_helper.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/test/test_widget_delegates.h"
#include "ash/test/test_window_builder.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/work_area_insets.h"
#include "base/check_deref.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
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
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/os_settings_provider_ash.h"
#include "ui/views/test/test_widget_builder.h"
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
    display::Screen* screen = display::Screen::Get();
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
    : task_environment_(std::move(task_environment)),
      owned_local_state_(std::make_unique<TestingPrefServiceSimple>()),
      local_state_(owned_local_state_.get()) {
  CHECK(local_state_);
  RegisterLocalStatePrefs(owned_local_state_->registry(), true);
}

AshTestBase::AshTestBase(
    std::unique_ptr<base::test::TaskEnvironment> task_environment,
    TestingPrefServiceSimple* local_state)
    : task_environment_(std::move(task_environment)),
      local_state_(local_state) {
  CHECK(local_state_);
}

AshTestBase::~AshTestBase() {
  // Ensure the next test starts with a null display::Screen.  This must be done
  // here instead of in TearDown() since some tests test access to the Screen
  // after the shell shuts down (which they use TearDown() to trigger).
  ScreenAsh::DeleteScreenForShutdown();

  CHECK(setup_called_)
      << "You have overridden SetUp but never called AshTestBase::SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called AshTestBase::TearDown";
}

void AshTestBase::SetUp() {
  // At this point, the task APIs should already be provided by
  // |task_environment_|.
  CHECK(base::SingleThreadTaskRunner::HasCurrentDefault());
  CHECK(base::ThreadPoolInstance::Get());

  setup_called_ = true;
  CHECK(!init_params_->local_state) << "local state can not be overridden";
  init_params_->local_state = local_state();
  // AshTestBase destroys the Screen instance at the destructor,
  // because some of the tests verifies the screen instance
  // after the ash::Shell destroyed in AshTestHelper::TearDown().
  init_params_->destroy_screen = false;

  // Prepare for a pixel test if having pixel init params.
  std::optional<pixel_test::InitParams> pixel_test_init_params =
      CreatePixelTestInitParams();
  if (pixel_test_init_params) {
    PrepareForPixelDiffTest();
    pixel_test_helper_ = std::make_unique<AshPixelTestHelper>(
        std::move(*pixel_test_init_params));
  }

  const bool enable_pixel_output =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnablePixelOutputInTests);
  test_context_factories_ = std::make_unique<ui::TestContextFactories>(
      /*enable_pixel_output=*/enable_pixel_output,
      /*output_to_window=*/enable_pixel_output);
  ash_test_helper_ = std::make_unique<AshTestHelper>(
      test_context_factories_->GetContextFactory());
  ash_test_helper_->SetUp(std::move(*init_params_));
  init_params_.reset();

  // Call `StabilizeUI()` after the user session is activated (if any) in the
  // test setup.
  if (pixel_test_helper_) {
    pixel_test_helper_->StabilizeUi();
  }

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

  // Must be deleted before ash_test_helper. AshPixelTestHelper manages a
  // ScopedFeatureList, and for the correct order of destruction of feature
  // lists, AshPixelTestHelper needs to be deleted earlier.
  pixel_test_helper_.reset();

  ash_test_helper_->TearDown();
  OnHelperWillBeDestroyed();
  ash_test_helper_.reset();

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

std::string AshTestBase::GenerateScreenshotName(const std::string& title) {
  CHECK(CreatePixelTestInitParams());
  return pixel_test_helper()->GenerateScreenshotName(title);
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
  views::test::TestWidgetBuilder builder;
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
  views::test::TestWidgetBuilder builder;
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
    views::WidgetDelegate* delegate,
    bool show) {
  views::test::TestWidgetBuilder builder;
  if (delegate) {
    builder.SetDelegate(delegate);
  } else {
    builder.SetDelegate(CreateTestWidgetBuilderDelegate());
  }
  builder.SetWindowTitle(u"Window " + base::NumberToString16(shell_window_id));
  if (app_type != chromeos::AppType::NON_APP) {
    builder.SetWindowProperty(chromeos::kAppTypeKey, app_type);
  }

  // |widget| is configured to be owned by the underlying window.
  views::Widget* widget =
      builder
          .SetBounds(bounds_in_screen.IsEmpty() ? gfx::Rect(0, 0, 300, 300)
                                                : bounds_in_screen)
          .SetContext(Shell::GetPrimaryRootWindow())
          .SetShow(show)
          .SetWindowId(shell_window_id)
          .BuildOwnedByNativeWidget();
  return base::WrapUnique(widget->GetNativeWindow());
}

std::unique_ptr<aura::Window> AshTestBase::CreateTestWindow(
    const gfx::Rect& bounds_in_screen,
    aura::client::WindowType type,
    int shell_window_id) {
  if (type != aura::client::WINDOW_TYPE_NORMAL) {
    return base::WrapUnique(
        CreateTestWindowInShell({.bounds = bounds_in_screen,
                                 .window_type = type,
                                 .window_id = shell_window_id}));
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
      CreateTestWindowInShell({.delegate = delegate,
                               .bounds = bounds_in_screen,
                               .window_id = shell_window_id}));
}

aura::Window* AshTestBase::CreateTestWindowInShell(
    aura::test::WindowBuilderParams params) {
  return TestWindowBuilder(params)
      .SetWindowTitle(u"Window " + base::NumberToString16(params.window_id))
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

void AshTestBase::SetUserPref(const std::string& user_email,
                              const std::string& path,
                              const base::Value& value) {
  AccountId accountId = AccountId::FromUserEmail(user_email);
  PrefService* prefs =
      GetSessionControllerClient()->GetUserPrefService(accountId);
  prefs->Set(path, value);
}

TestSessionControllerClient* AshTestBase::GetSessionControllerClient() {
  return ash_test_helper_->test_session_controller_client(
      base::PassKey<AshTestBase>());
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

AccountId AshTestBase::SimulateUserLogin(
    LoginInfo info,
    std::optional<AccountId> opt_account_id,
    std::unique_ptr<PrefService> pref_service) {
  auto account_id = ash_test_helper_->SimulateUserLogin(
      std::move(info), std::move(opt_account_id), std::move(pref_service));
  if (pixel_test_helper_) {
    pixel_test_helper_->StabilizeUi();
  }
  return account_id;
}

void AshTestBase::SimulateUserLogin(const AccountId& account_id) {
  ash_test_helper_->SimulateUserLogin({}, std::move(account_id), nullptr);
  if (pixel_test_helper_) {
    pixel_test_helper_->StabilizeUi();
  }
}

AccountId AshTestBase::SimulateNewUserFirstLogin(
    const std::string& user_email) {
  auto account_id = ash_test_helper_->SimulateUserLogin(
      {.display_email = user_email, .is_new_profile = true}, std::nullopt);

  if (pixel_test_helper_) {
    pixel_test_helper_->StabilizeUi();
  }
  return account_id;
}

AccountId AshTestBase::SimulateGuestLogin() {
  return SimulateUserLogin(
      {user_manager::kGuestUserName, user_manager::UserType::kGuest});
}

AccountId AshTestBase::SimulateKioskMode(user_manager::UserType user_type) {
  DCHECK(user_type == user_manager::UserType::kKioskChromeApp ||
         user_type == user_manager::UserType::kKioskWebApp);

  GetSessionControllerClient()->SetIsRunningInAppMode(true);
  return SimulateUserLogin({kKioskUserEmail, user_type});
}

void AshTestBase::SwitchActiveUser(const AccountId& account_id) {
  Shell::Get()->session_controller()->SwitchActiveUser(account_id);
}

bool AshTestBase::IsInSessionState(session_manager::SessionState state) const {
  return Shell::Get()->session_controller()->GetSessionState() == state;
}

void AshTestBase::SetAccessibilityPanelHeight(int panel_height) {
  Shell::GetPrimaryRootWindowController()
      ->GetAccessibilityPanelLayoutManagerForTest()
      ->SetPanelBounds(gfx::Rect(0, 0, 0, panel_height),
                       AccessibilityPanelState::FULL_WIDTH);
}

void AshTestBase::ClearLogin() {
  GetSessionControllerClient()->Reset();
  Shell::Get()->RecreateMultiUserWindowManagerForTesting();
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
  display::Screen* screen = display::Screen::Get();
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
  if (display::Screen::Get()->GetNumDisplays() <= 1) {
    return;
  }
  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id());
}

display::Display AshTestBase::GetPrimaryDisplay() const {
  return display::Screen::Get()->GetDisplayNearestWindow(
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

  // Use dark mode by default, which is what many gold images expect.
  ui::OsSettingsProvider::Get();  // Ensure Ash instance is constructed
  auto* const os_settings_provider = ui::OsSettingsProviderAsh::GetInstance();
  CHECK(os_settings_provider);
  os_settings_provider->SetColorPaletteData(
      ui::NativeTheme::PreferredColorScheme::kDark,
      os_settings_provider->AccentColor(),
      os_settings_provider->SchemeVariant());

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
