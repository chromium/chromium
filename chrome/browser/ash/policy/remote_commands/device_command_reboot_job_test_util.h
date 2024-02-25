// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REBOOT_JOB_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REBOOT_JOB_TEST_UTIL_H_

#include <memory>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_reboot_job.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_reboot_notifications_scheduler.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/testing_pref_service.h"

namespace policy {

// Fakes login state for the duration of the instance's lifetime.
class ScopedLoginState {
 public:
  static ScopedLoginState CreateKiosk();

  static ScopedLoginState CreateLoggedOut();

  static ScopedLoginState CreateRegularUser();

  ScopedLoginState(const ScopedLoginState& other) = delete;
  ScopedLoginState& operator=(const ScopedLoginState& other) = delete;

  ~ScopedLoginState();

 private:
  ScopedLoginState(ash::LoginState::LoggedInState state,
                   ash::LoginState::LoggedInUserType type);
};

// Base class for reboot command unit tests.
class DeviceCommandRebootJobTestBase {
 public:
  DeviceCommandRebootJobTestBase(const DeviceCommandRebootJobTestBase&) =
      delete;
  DeviceCommandRebootJobTestBase& operator=(
      const DeviceCommandRebootJobTestBase&) = delete;

 protected:
  DeviceCommandRebootJobTestBase();

  virtual ~DeviceCommandRebootJobTestBase();

  base::Time Now() const { return task_environment_.GetMockClock()->Now(); }

  base::TimeTicks NowTicks() const {
    return task_environment_.GetMockTickClock()->NowTicks();
  }

  std::unique_ptr<DeviceCommandRebootJob> CreateAndInitializeCommand(
      base::TimeDelta age_of_command,
      base::TimeDelta user_session_reboot_delay);

  std::unique_ptr<DeviceCommandRebootJob> CreateAndInitializeCommand(
      enterprise_management::RemoteCommand command_proto);

  // IO thread is required by `chromeos::NativeTimer`.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  const base::TimeTicks start_ticks_{NowTicks()};

  ash::SessionTerminationManager session_termination_manager_;

  std::unique_ptr<TestingPrefServiceSimple> prefs_{
      std::make_unique<TestingPrefServiceSimple>()};
  std::unique_ptr<FakeRebootNotificationsScheduler>
      fake_notifications_scheduler_{
          std::make_unique<FakeRebootNotificationsScheduler>(
              task_environment_.GetMockClock(),
              task_environment_.GetMockTickClock(),
              prefs_.get())};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_REBOOT_JOB_TEST_UTIL_H_
