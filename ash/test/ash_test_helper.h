// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_HELPER_H_
#define ASH_TEST_ASH_TEST_HELPER_H_

#include <stdint.h>

#include <memory>
#include <utility>

#include "ash/assistant/test/test_assistant_service.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"
#include "ash/session/test_pref_service_provider.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell_delegate.h"
#include "ash/system/notification_center/test_notifier_settings_controller.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "chromeos/ash/services/federated/public/cpp/fake_service_connection.h"
#include "chromeos/ash/services/federated/public/cpp/service_connection.h"
#include "ui/aura/test/aura_test_helper.h"

class PrefService;

namespace aura {
class Window;
}  // namespace aura

namespace base {
class SystemMonitor;
}  // namespace base

namespace display {
class Display;
}  // namespace display

namespace ui {
class ContextFactory;
}  // namespace ui

namespace views {
class TestViewsDelegate;
}  // namespace views

namespace ash {

class AppListTestHelper;
class AmbientAshTestHelper;
class AshPixelTestHelper;
class FakeDlcserviceClient;
class FakeFwupdDownloadClient;
class SavedDeskTestHelper;
class TestKeyboardControllerObserver;
class TestNewWindowDelegate;
class TestWallpaperControllerClient;

namespace hotspot_config {
class CrosHotspotConfigTestHelper;
}  // namespace hotspot_config

namespace input_method {
class MockInputMethodManagerImpl;
}  // namespace input_method

// A helper class that does common initialization required for Ash. Creates a
// root window and an ash::Shell instance with a test delegate.
class AshTestHelper : public aura::test::AuraTestHelper {
 public:
  struct InitParams {
    InitParams();
    InitParams(InitParams&&);
    InitParams& operator=(InitParams&&) = default;
    ~InitParams();

    // True if the user should log in.
    bool start_session = true;
    // If this is not set, a TestShellDelegate will be used automatically.
    std::unique_ptr<ShellDelegate> delegate;
    raw_ptr<PrefService> local_state = nullptr;

    // Used only when setting up a pixel diff test.
    std::optional<pixel_test::InitParams> pixel_test_init_params;

    // True if a fake global `CrasAudioHandler` should be created.
    bool create_global_cras_audio_handler = true;

    // True if a global `QuickPairMediator` should be created.
    bool create_quick_pair_mediator = true;
  };

  // Instantiates/destroys an AshTestHelper. This can happen in a
  // single-threaded phase without a backing task environment or ViewsDelegate,
  // and must not create those lest the caller wish to do so.
  explicit AshTestHelper(ui::ContextFactory* context_factory = nullptr);

  AshTestHelper(const AshTestHelper&) = delete;
  AshTestHelper& operator=(const AshTestHelper&) = delete;

  ~AshTestHelper() override;

  // Calls through to SetUp() below, see comments there.
  void SetUp() override;

  // Tears down everything but the Screen instance, which some tests access
  // after this point.  This will be called automatically on destruction if it
  // is not called manually earlier.
  void TearDown() override;

  aura::Window* GetContext() override;
  aura::WindowTreeHost* GetHost() override;
  aura::TestScreen* GetTestScreen() override;
  aura::client::FocusClient* GetFocusClient() override;
  aura::client::CaptureClient* GetCaptureClient() override;

  // Creates the ash::Shell and performs associated initialization according
  // to |init_params|.  When this function returns it guarantees a task
  // environment and ViewsDelegate will exist, the shell will be started, and a
  // window will be showing.
  void SetUp(InitParams init_params);

  display::Display GetSecondaryDisplay() const;

  // Simulates a user sign-in. It creates a new user session, adds it to
  // existing user sessions and makes it the active user session.
  // `is_new_profile` indicates whether the logged-in account is new.
  void SimulateUserLogin(
      const AccountId& account_id,
      user_manager::UserType user_type = user_manager::UserType::kRegular,
      bool is_new_profile = false);

  // Stabilizes the variable UI components (such as the battery view).
  void StabilizeUIForPixelTest();

  TestSessionControllerClient* test_session_controller_client() {
    return session_controller_client_.get();
  }
  void set_test_session_controller_client(
      std::unique_ptr<TestSessionControllerClient> session_controller_client) {
    session_controller_client_ = std::move(session_controller_client);
  }
  TestNotifierSettingsController* notifier_settings_controller() {
    return notifier_settings_controller_.get();
  }
  TestSystemTrayClient* system_tray_client() {
    return system_tray_client_.get();
  }
  TestPrefServiceProvider* prefs_provider() { return prefs_provider_.get(); }

  AppListTestHelper* app_list_test_helper() {
    return app_list_test_helper_.get();
  }

  TestKeyboardControllerObserver* test_keyboard_controller_observer() {
    return test_keyboard_controller_observer_.get();
  }

  TestAssistantService* test_assistant_service() {
    return assistant_service_.get();
  }

  AmbientAshTestHelper* ambient_ash_test_helper() {
    return ambient_ash_test_helper_.get();
  }

  bluetooth_config::ScopedBluetoothConfigTestHelper*
  bluetooth_config_test_helper() {
    return &scoped_bluetooth_config_test_helper_;
  }

  SavedDeskTestHelper* saved_desk_test_helper() {
    return saved_desk_test_helper_.get();
  }

  input_method::MockInputMethodManagerImpl* input_method_manager() {
    return input_method_manager_;
  }

  hotspot_config::CrosHotspotConfigTestHelper*
  cros_hotspot_config_test_helper() {
    return cros_hotspot_config_test_helper_.get();
  }

  FakeDlcserviceClient* dlc_service_client() {
    return dlc_service_client_.get();
  }

 private:
  // Scoping objects to manage init/teardown of services.
  class BluezDBusManagerInitializer;
  class FlossDBusManagerInitializer;
  class PowerPolicyControllerInitializer;

  // Must be constructed so that `base::SystemMonitor::Get()` returns a valid
  // instance.
  std::unique_ptr<base::SystemMonitor> system_monitor_;

  std::unique_ptr<base::test::ScopedCommandLine> command_line_ =
      std::make_unique<base::test::ScopedCommandLine>();
  std::unique_ptr<system::ScopedFakeStatisticsProvider> statistics_provider_ =
      std::make_unique<system::ScopedFakeStatisticsProvider>();
  std::unique_ptr<TestPrefServiceProvider> prefs_provider_ =
      std::make_unique<TestPrefServiceProvider>();
  std::unique_ptr<TestNotifierSettingsController>
      notifier_settings_controller_ =
          std::make_unique<TestNotifierSettingsController>();
  std::unique_ptr<TestAssistantService> assistant_service_ =
      std::make_unique<TestAssistantService>();
  std::unique_ptr<TestSystemTrayClient> system_tray_client_ =
      std::make_unique<TestSystemTrayClient>();
  std::unique_ptr<AppListTestHelper> app_list_test_helper_;
  std::unique_ptr<BluezDBusManagerInitializer> bluez_dbus_manager_initializer_;
  std::unique_ptr<FlossDBusManagerInitializer> floss_dbus_manager_initializer_;
  std::unique_ptr<PowerPolicyControllerInitializer>
      power_policy_controller_initializer_;
  std::unique_ptr<TestNewWindowDelegate> new_window_delegate_;
  std::unique_ptr<views::TestViewsDelegate> test_views_delegate_;
  std::unique_ptr<FakeDlcserviceClient> dlc_service_client_;
  std::unique_ptr<TestSessionControllerClient> session_controller_client_;
  std::unique_ptr<TestKeyboardControllerObserver>
      test_keyboard_controller_observer_;
  std::unique_ptr<AmbientAshTestHelper> ambient_ash_test_helper_;
  std::unique_ptr<TestWallpaperControllerClient> wallpaper_controller_client_;
  std::unique_ptr<SavedDeskTestHelper> saved_desk_test_helper_;
  std::unique_ptr<FakeFwupdDownloadClient> fwupd_download_client_;
  std::unique_ptr<quick_pair::Mediator::Factory> quick_pair_mediator_factory_;
  std::unique_ptr<quick_pair::QuickPairBrowserDelegate>
      quick_pair_browser_delegate_;
  std::unique_ptr<hotspot_config::CrosHotspotConfigTestHelper>
      cros_hotspot_config_test_helper_;

  // Used only for pixel tests.
  std::unique_ptr<AshPixelTestHelper> pixel_test_helper_;

  bluetooth_config::ScopedBluetoothConfigTestHelper
      scoped_bluetooth_config_test_helper_;

  // InputMethodManager is not owned by this class. It is stored in a
  // global that is registered via InputMethodManager::Initialize().
  raw_ptr<input_method::MockInputMethodManagerImpl, DanglingUntriaged>
      input_method_manager_ = nullptr;

  federated::FakeServiceConnectionImpl fake_federated_service_connection_;
  federated::ScopedFakeServiceConnectionForTest
      scoped_fake_federated_service_connection_for_test_;

  // True if a fake global `CrasAudioHandler` should be created.
  bool create_global_cras_audio_handler_ = true;
  // True if a fake `QuickPairMediator` should be created.
  bool create_quick_pair_mediator_ = true;
};

}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_HELPER_H_
