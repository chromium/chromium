// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_HELPER_H_
#define ASH_TEST_ASH_TEST_HELPER_H_

#include <stdint.h>

#include <memory>
#include <utility>

#include "ash/session/test_session_controller_client.h"
#include "base/macros.h"
#include "base/test/scoped_command_line.h"

class PrefService;

namespace aura {
class Window;
namespace test {
class EnvWindowTreeClientSetter;
}
}

namespace chromeos {
namespace system {
class ScopedFakeStatisticsProvider;
}  // namespace system
}  // namespace chromeos

namespace display {
class Display;
}

namespace service_manager {
class Connector;
}

namespace ui {
class ScopedAnimationDurationScaleMode;
}

namespace wm {
class WMState;
}

namespace ash {

class AppListTestHelper;
class AshTestEnvironment;
class AshTestViewsDelegate;
class TestConnector;
class TestShellDelegate;

// A helper class that does common initialization required for Ash. Creates a
// root window and an ash::Shell instance with a test delegate.
class AshTestHelper {
 public:
  explicit AshTestHelper(AshTestEnvironment* ash_test_environment);
  ~AshTestHelper();

  // Creates the ash::Shell and performs associated initialization.  Set
  // |start_session| to true if the user should log in before the test is run.
  // Set |provide_local_state| to true to inject local-state PrefService into
  // the Shell before the test is run.
  void SetUp(bool start_session, bool provide_local_state = true);

  // Destroys the ash::Shell and performs associated cleanup.
  void TearDown();

  // Call this only if this code is being run outside of ash, for example, in
  // browser tests that use AshTestBase. This disables CHECKs that are
  // applicable only when used inside ash.
  // TODO: remove this and ban usage of AshTestHelper outside of ash.
  void SetRunningOutsideAsh();

  // Returns a root Window. Usually this is the active root Window, but that
  // method can return NULL sometimes, and in those cases, we fall back on the
  // primary root Window.
  aura::Window* CurrentContext();

  PrefService* GetLocalStatePrefService();

  TestShellDelegate* test_shell_delegate() { return test_shell_delegate_; }
  void set_test_shell_delegate(TestShellDelegate* test_shell_delegate) {
    test_shell_delegate_ = test_shell_delegate;
  }
  AshTestViewsDelegate* test_views_delegate() {
    return test_views_delegate_.get();
  }

  AshTestEnvironment* ash_test_environment() { return ash_test_environment_; }

  display::Display GetSecondaryDisplay();

  TestSessionControllerClient* test_session_controller_client() {
    return session_controller_client_.get();
  }
  void set_test_session_controller_client(
      std::unique_ptr<TestSessionControllerClient> session_controller_client) {
    session_controller_client_ = std::move(session_controller_client);
  }

  AppListTestHelper* app_list_test_helper() {
    return app_list_test_helper_.get();
  }

  void reset_commandline() { command_line_.reset(); }

  // Gets a Connector that talks directly to the WindowService.
  service_manager::Connector* GetWindowServiceConnector();

 private:
  // Forces creation of the WindowService. The WindowService is normally created
  // on demand, this force the creation.
  void CreateWindowService();

  // Called when running in ash to create Shell.
  void CreateShell();

  std::unique_ptr<chromeos::system::ScopedFakeStatisticsProvider>
      statistics_provider_;

  std::unique_ptr<aura::test::EnvWindowTreeClientSetter>
      env_window_tree_client_setter_;
  AshTestEnvironment* ash_test_environment_;  // Not owned.
  TestShellDelegate* test_shell_delegate_ = nullptr;  // Owned by ash::Shell.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  std::unique_ptr<::wm::WMState> wm_state_;
  std::unique_ptr<AshTestViewsDelegate> test_views_delegate_;

  // Check if DBus Thread Manager was initialized here.
  bool dbus_thread_manager_initialized_ = false;
  // Check if Bluez DBus Manager was initialized here.
  bool bluez_dbus_manager_initialized_ = false;
  // Check if PowerPolicyController was initialized here.
  bool power_policy_controller_initialized_ = false;

  std::unique_ptr<TestSessionControllerClient> session_controller_client_;

  std::unique_ptr<base::test::ScopedCommandLine> command_line_;

  std::unique_ptr<AppListTestHelper> app_list_test_helper_;

  std::unique_ptr<TestConnector> test_connector_;

  std::unique_ptr<service_manager::Connector> window_service_connector_;

  DISALLOW_COPY_AND_ASSIGN(AshTestHelper);
};

}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_HELPER_H_
