// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include <algorithm>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/ambient/test/ambient_ash_test_helper.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/display_configuration_controller_test_api.h"
#include "ash/display/screen_ash.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/test_keyboard_ui.h"
#include "ash/public/cpp/test/test_keyboard_controller_observer.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/quick_pair/common/fake_quick_pair_browser_delegate.h"
#include "ash/quick_pair/keyed_service/fake_quick_pair_mediator_factory.h"
#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/geolocation/test_geolocation_url_loader_factory.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/notification_center/session_state_notification_blocker.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/test/ash_test_views_delegate.h"
#include "ash/test/login_info.h"
#include "ash/test/pixel/ash_pixel_test_helper.h"
#include "ash/test/toplevel_window.h"
#include "ash/test_shell_delegate.h"
#include "ash/wallpaper/test_wallpaper_controller_client.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/desks/templates/saved_desk_test_helper.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/system/system_monitor.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"
#include "chromeos/ash/components/dbus/typecd/typecd_client.h"
#include "chromeos/ash/components/fwupd/fake_fwupd_download_client.h"
#include "chromeos/ash/components/geolocation/cached_location_provider.h"
#include "chromeos/ash/components/geolocation/live_location_provider.h"
#include "chromeos/ash/components/geolocation/location_fetcher.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/services/bluetooth_config/in_process_instance.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/cros_hotspot_config_test_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/mock_input_method_manager_impl.h"
#include "ui/color/color_provider_manager.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/fake_display_delegate.h"
#include "ui/display/manager/util/display_manager_test_util.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/util/display_util.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/gfx/geometry/point.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#include "ui/views/test/views_test_helper_aura.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/focus_controller.h"

namespace ash {

namespace {
std::unique_ptr<views::TestViewsDelegate> MakeTestViewsDelegate() {
  return std::make_unique<AshTestViewsDelegate>();
}
}  // namespace

AshTestHelper::InitParams::InitParams() = default;
AshTestHelper::InitParams::InitParams(InitParams&&) = default;
AshTestHelper::InitParams::~InitParams() = default;

class AshTestHelper::BluezDBusManagerInitializer {
 public:
  BluezDBusManagerInitializer() { bluez::BluezDBusManager::InitializeFake(); }
  ~BluezDBusManagerInitializer() {
    device::BluetoothAdapterFactory::Shutdown();
    bluez::BluezDBusManager::Shutdown();
  }
};

class AshTestHelper::FlossDBusManagerInitializer {
 public:
  FlossDBusManagerInitializer() { floss::FlossDBusManager::InitializeFake(); }
  ~FlossDBusManagerInitializer() {
    device::BluetoothAdapterFactory::Shutdown();
    floss::FlossDBusManager::Shutdown();
  }
};

class AshTestHelper::PowerPolicyControllerInitializer {
 public:
  PowerPolicyControllerInitializer() {
    chromeos::PowerPolicyController::Initialize(
        chromeos::PowerManagerClient::Get());
  }
  ~PowerPolicyControllerInitializer() {
    chromeos::PowerPolicyController::Shutdown();
  }
};

AshTestHelper::AshTestHelper(ui::ContextFactory* context_factory)
    : AuraTestHelper(context_factory),
      system_monitor_(std::make_unique<base::SystemMonitor>()),
      session_manager_(
          !session_manager::SessionManager::Get()
              ? std::make_unique<session_manager::SessionManager>(
                    std::make_unique<
                        session_manager::FakeSessionManagerDelegate>())
              : nullptr) {
  views::ViewsTestHelperAura::SetFallbackTestViewsDelegateFactory(
      &MakeTestViewsDelegate);

  // TODO(jamescook): Can we do this without changing command line?
  // Use the origin (1,1) so that it doesn't overlap with the native mouse
  // cursor.
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      !command_line_->GetProcessCommandLine()->HasSwitch(
          ::switches::kHostWindowBounds)) {
    // TODO(oshima): Disable native events instead of adding offset.
    command_line_->GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kHostWindowBounds, "10+10-800x600");
  }

  TabletModeController::SetUseScreenshotForTest(false);

  display::ResetDisplayIdForTest();
  display::SetInternalDisplayIds({});

  // Reset the global state for the cursor manager. This includes the
  // last cursor visibility state, etc.
  wm::CursorManager::ResetCursorVisibilityStateForTest();

  // Clears the saved state so that test doesn't use on the wrong
  // default state.
  shell::ToplevelWindow::ClearSavedStateForTest();

  // SystemLocationProvider has to be initialized before
  // GeolocationController, which is constructed during Shell::Init().
  if (::chromeos::features::IsCachedLocationProviderEnabled()) {
    SystemLocationProvider::Initialize(std::make_unique<CachedLocationProvider>(
        std::make_unique<LocationFetcher>(
            base::MakeRefCounted<TestGeolocationUrlLoaderFactory>())));
  } else {
    SystemLocationProvider::Initialize(std::make_unique<LiveLocationProvider>(
        std::make_unique<LocationFetcher>(
            base::MakeRefCounted<TestGeolocationUrlLoaderFactory>())));
  }
}

AshTestHelper::~AshTestHelper() {
  if (app_list_test_helper_) {
    TearDown();
  }

  SystemLocationProvider::DestroyForTesting();

  if (destroy_screen_) {
    // Ensure the next test starts with a null display::Screen.  This must be
    // done here instead of in TearDown() since some tests test access to the
    // Screen after the shell shuts down (which they use TearDown() to trigger).
    ScreenAsh::DeleteScreenForShutdown();
  }

  // This should never have a meaningful effect, since either there is no
  // ViewsTestHelperAura instance or the instance is currently in its
  // destructor.
  views::ViewsTestHelperAura::SetFallbackTestViewsDelegateFactory(nullptr);
}

void AshTestHelper::SetUp() {
  SetUp(InitParams());
}

void AshTestHelper::TearDown() {
  fwupd_download_client_.reset();
  saved_desk_test_helper_->Shutdown();

  ambient_ash_test_helper_.reset();

  // The AppListTestHelper holds a pointer to the AppListController the Shell
  // owns, so shut the test helper down first.
  app_list_test_helper_.reset();

  // Stop event dispatch like we do in ChromeBrowserMainExtraPartsAsh.
  Shell::Get()->ShutdownEventDispatch();

  Shell::DeleteInstance();
  // Suspend the tear down until all resources are returned via
  // CompositorFrameSinkClient::ReclaimResources()
  base::RunLoop().RunUntilIdle();

  LoginState::Shutdown();

  TypecdClient::Shutdown();

  if (create_global_cras_audio_handler_) {
    CrasAudioHandler::Shutdown();
    CrasAudioClient::Shutdown();
  }

  // The PowerPolicyController holds a pointer to the PowerManagementClient, so
  // shut the controller down first.
  power_policy_controller_initializer_.reset();
  chromeos::PowerManagerClient::Shutdown();
  RgbkbdClient::Shutdown();

  TabletModeController::SetUseScreenshotForTest(true);

  // Destroy all owned objects to prevent tests from depending on their state
  // after this returns.
  cros_hotspot_config_test_helper_.reset();
  test_keyboard_controller_observer_.reset();
  session_controller_client_.reset();
  dlc_service_client_.reset();
  test_views_delegate_.reset();
  new_window_delegate_.reset();
  bluez_dbus_manager_initializer_.reset();
  floss_dbus_manager_initializer_.reset();
  system_tray_client_.reset();
  notifier_settings_controller_.reset();
  prefs_provider_.reset();
  statistics_provider_.reset();
  command_line_.reset();
  quick_pair_browser_delegate_.reset();
  saved_desk_test_helper_.reset();

  // Purge ColorProviderManager between tests so that we don't accumulate
  // ColorProviderInitializers. crbug.com/1349232.
  ui::ColorProviderManager::ResetForTesting();

  AuraTestHelper::TearDown();

  // Cleanup the global state for InputMethodManager, but only if
  // it was setup by this test helper. This allows tests to implement
  // their own override, and in that case we shouldn't call Shutdown
  // otherwise the global state will be deleted twice.
  if (input_method_manager_) {
    input_method::InputMethodManager::Shutdown();
    input_method_manager_ = nullptr;
  }
}

aura::Window* AshTestHelper::GetContext() {
  aura::Window* root_window = Shell::GetRootWindowForNewWindows();
  if (!root_window) {
    root_window = Shell::GetPrimaryRootWindow();
  }
  DCHECK(root_window);
  return root_window;
}

aura::WindowTreeHost* AshTestHelper::GetHost() {
  auto* manager = Shell::Get()->window_tree_host_manager();
  const int64_t id = manager->GetPrimaryDisplayId();
  return manager->GetAshWindowTreeHostForDisplayId(id)->AsWindowTreeHost();
}

aura::TestScreen* AshTestHelper::GetTestScreen() {
  // If a test needs this, we may need to refactor TestScreen such that its
  // methods can operate atop some sort of real screen/host/display, and hook
  // them to the ones provided by the shell.  For now, not bothering.
  NOTIMPLEMENTED();
  return nullptr;
}

aura::client::FocusClient* AshTestHelper::GetFocusClient() {
  return Shell::Get()->focus_controller();
}

aura::client::CaptureClient* AshTestHelper::GetCaptureClient() {
  return wm::CaptureController::Get();
}

void AshTestHelper::SetUp(InitParams init_params) {
  create_global_cras_audio_handler_ =
      init_params.create_global_cras_audio_handler;
  create_quick_pair_mediator_ = init_params.create_quick_pair_mediator;
  destroy_screen_ = init_params.destroy_screen;

  if (create_global_cras_audio_handler_) {
    // Create `CrasAudioHandler` for testing since `g_browser_process` is not
    // created in `AshTestBase` tests.
    CrasAudioClient::InitializeFake();
    CrasAudioHandler::InitializeForTesting();
  }

  // This block of objects are conditionally initialized here rather than in the
  // constructor to make it easier for test classes to override them.
  if (!input_method::InputMethodManager::Get()) {
    // |input_method_manager_| is not owned and is cleaned up in TearDown()
    // by calling InputMethodManager::Shutdown().
    input_method_manager_ = new input_method::MockInputMethodManagerImpl();
    input_method::InputMethodManager::Initialize(input_method_manager_);
  }
  if (floss::features::IsFlossEnabled()) {
    if (!floss::FlossDBusManager::IsInitialized()) {
      floss_dbus_manager_initializer_ =
          std::make_unique<FlossDBusManagerInitializer>();
    }
  } else {
    if (!bluez::BluezDBusManager::IsInitialized()) {
      bluez_dbus_manager_initializer_ =
          std::make_unique<BluezDBusManagerInitializer>();
    }
  }

  if (!RgbkbdClient::Get()) {
    RgbkbdClient::InitializeFake();
  }
  if (!chromeos::PowerManagerClient::Get()) {
    chromeos::PowerManagerClient::InitializeFake();
  }
  if (!chromeos::PowerPolicyController::IsInitialized()) {
    power_policy_controller_initializer_ =
        std::make_unique<PowerPolicyControllerInitializer>();
  }

  if (!TypecdClient::Get()) {
    TypecdClient::InitializeFake();
  }

  if (!NewWindowDelegate::GetInstance()) {
    new_window_delegate_ = std::make_unique<TestNewWindowDelegate>();
  }
  if (!views::ViewsDelegate::GetInstance()) {
    test_views_delegate_ = MakeTestViewsDelegate();
  }
  if (!DlcserviceClient::Get()) {
    dlc_service_client_ = std::make_unique<FakeDlcserviceClient>();
  }

  cros_hotspot_config_test_helper_ =
      std::make_unique<hotspot_config::CrosHotspotConfigTestHelper>(
          /*use_fake_implementation=*/true);

  LoginState::Initialize();

  ambient_ash_test_helper_ = std::make_unique<AmbientAshTestHelper>();
  quick_pair_browser_delegate_ =
      std::make_unique<quick_pair::FakeQuickPairBrowserDelegate>();

  ShellInitParams shell_init_params;
  shell_init_params.delegate = std::move(init_params.delegate);
  if (!shell_init_params.delegate) {
    shell_init_params.delegate = std::make_unique<TestShellDelegate>();
  }
  shell_init_params.context_factory = GetContextFactory();
  shell_init_params.local_state = init_params.local_state;
  shell_init_params.keyboard_ui_factory =
      std::make_unique<TestKeyboardUIFactory>();
  if (create_quick_pair_mediator_) {
    shell_init_params.quick_pair_mediator_factory =
        std::make_unique<quick_pair::FakeQuickPairMediatorFactory>();
  }
  shell_init_params.native_display_delegate =
      std::make_unique<display::FakeDisplayDelegate>();
  Shell::CreateInstance(std::move(shell_init_params));
  Shell* shell = Shell::Get();

  chromeos::MultitaskMenuNudgeController::SetSuppressNudgeForTesting(true);

  // Set up a test wallpaper controller client before signing in any users. At
  // the time a user logs in, Wallpaper controller relies on
  // WallpaperControllerClient to check if user data should be synced.
  wallpaper_controller_client_ =
      std::make_unique<TestWallpaperControllerClient>();
  shell->wallpaper_controller()->SetClient(wallpaper_controller_client_.get());

  // Disable the notification delay timer used to prevent non system
  // notifications from showing up right after login. This needs to be done
  // before any user sessions are added since the delay timer starts right
  // after that.
  SessionStateNotificationBlocker::SetUseLoginNotificationDelayForTest(false);

  // Cursor is visible by default in tests.
  shell->cursor_manager()->ShowCursor();

  shell->system_tray_model()->SetClient(system_tray_client_.get());
  prefs_provider_ = std::make_unique<TestPrefServiceProvider>();

  // Requires the AppListController the Shell creates.
  app_list_test_helper_ = std::make_unique<AppListTestHelper>();

  // SavedDeskTestHelper depends on account.
  saved_desk_test_helper_ = std::make_unique<SavedDeskTestHelper>();

  Shell::GetPrimaryRootWindow()->Show();
  Shell::GetPrimaryRootWindow()->GetHost()->Show();

  // Don't change the display size due to host size resize.
  display::test::DisplayManagerTestApi(shell->display_manager())
      .DisableChangeDisplayUponHostResize();

  // Create the test keyboard controller observer to respond to
  // OnLoadKeyboardContentsRequested().
  test_keyboard_controller_observer_ =
      std::make_unique<TestKeyboardControllerObserver>(
          shell->keyboard_controller());

  // Tests that change the display configuration generally don't care about the
  // notifications and the popup UI can interfere with things like cursors.
  shell->screen_layout_observer()->set_show_notifications_for_testing(false);

  // Disable display change animations in unit tests.
  DisplayConfigurationControllerTestApi(
      shell->display_configuration_controller())
      .SetDisplayAnimator(false);

  // Remove the app dragging animations delay for testing purposes.
  shell->overview_controller()->set_delayed_animation_task_delay_for_test(
      base::TimeDelta());

  // Tests expect empty wallpaper.
  shell->wallpaper_controller()->CreateEmptyWallpaperForTesting();

  // Native events and mouse movements are disabled by
  // `ui::DisableNativeUiEventDispatchDisabled()`. Just make sure that the the
  // mouse cursour is not on the screen by default.
  aura::Env::GetInstance()->SetLastMouseLocation(gfx::Point(-1000, -1000));
  shell->cursor_manager()->EnableMouseEvents();

  // Changing GestureConfiguration shouldn't make tests fail. These values
  // prevent unexpected events from being generated during tests. Such as
  // delayed events which create race conditions on slower tests.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_max_touch_down_duration_for_click_in_ms(800);
  gesture_config->set_long_press_time_in_ms(1000);
  gesture_config->set_max_touch_move_in_pixels_for_click(5);

  // Fake the |ec_lid_angle_driver_status_| in the unittests.
  AccelerometerReader::GetInstance()->SetECLidAngleDriverStatusForTesting(
      ECLidAngleDriverStatus::NOT_SUPPORTED);

  if (TabletMode::IsBoardTypeMarkedAsTabletCapable()) {
    shell->tablet_mode_controller()->OnDeviceListsComplete();
  }

  fwupd_download_client_ = std::make_unique<FakeFwupdDownloadClient>();

  session_controller_client_ = std::make_unique<TestSessionControllerClient>(
      shell->session_controller(), prefs_provider_.get(),
      init_params.create_signin_pref_service);
  session_controller_client_->set_pref_service_must_exist(
      !init_params.auto_create_prefs_services);
  session_controller_client_->InitializeAndSetClient();

  // Sign-in after UI is shown.
  if (init_params.start_session) {
    // TODO(crbug.com/383441831): Remove Reset();
    session_controller_client_->Reset();

    SimulateUserLogin({}, AccountId::FromUserEmail("user0@tray"));
  }
}

display::Display AshTestHelper::GetSecondaryDisplay() const {
  return display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .GetSecondaryDisplay();
}

AccountId AshTestHelper::SimulateUserLogin(
    LoginInfo login_info,
    std::optional<AccountId> opt_account_id,
    std::unique_ptr<PrefService> pref_service) {
  AccountId account_id = session_controller_client_->AddUserSession(
      login_info, opt_account_id, std::move(pref_service));

  // Taken some concept from User::CanLock(). Kiosk/Guest accounts are
  // disallowed to lock screen here. Other accounts are allowed by default.
  // We may need to consider the pref following the production behavior.
  switch (login_info.user_type) {
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
    case user_manager::UserType::kPublicAccount:
      break;
    case user_manager::UserType::kKioskChromeApp:
    case user_manager::UserType::kKioskWebApp:
    case user_manager::UserType::kKioskIWA:
    case user_manager::UserType::kGuest:
    case user_manager::UserType::kKioskArcvmApp:
      session_controller_client_->SetCanLockScreen(false);
      break;
  }

  session_controller_client_->SwitchActiveUser(account_id);

  if (login_info.activate_session) {
    session_controller_client_->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

  return account_id;
}

}  // namespace ash
