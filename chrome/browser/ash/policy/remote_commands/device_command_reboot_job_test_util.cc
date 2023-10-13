// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_reboot_job_test_util.h"

#include <utility>

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr RemoteCommandJob::UniqueIDType kUniqueID = 123456789;

// Test reboot command exposing testing interface.
class DeviceCommandRebootJobForTesting : public DeviceCommandRebootJob {
 public:
  using DeviceCommandRebootJob::GetBootTimeCallback;

  template <class... Args>
  explicit DeviceCommandRebootJobForTesting(Args... args)
      : DeviceCommandRebootJob(std::forward<Args>(args)...) {}
};

}  // namespace

// static
ScopedLoginState ScopedLoginState::CreateKiosk() {
  return ScopedLoginState(ash::LoginState::LOGGED_IN_NONE,
                          ash::LoginState::LOGGED_IN_USER_KIOSK);
}

// static
ScopedLoginState ScopedLoginState::CreateLoggedOut() {
  return ScopedLoginState(ash::LoginState::LOGGED_IN_NONE,
                          ash::LoginState::LOGGED_IN_USER_NONE);
}

// static
ScopedLoginState ScopedLoginState::CreateRegularUser() {
  return ScopedLoginState(ash::LoginState::LOGGED_IN_ACTIVE,
                          ash::LoginState::LOGGED_IN_USER_REGULAR);
}

ScopedLoginState::~ScopedLoginState() {
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_NONE, ash::LoginState::LOGGED_IN_USER_NONE);

  ash::LoginState::Shutdown();
}

ScopedLoginState::ScopedLoginState(ash::LoginState::LoggedInState state,
                                   ash::LoginState::LoggedInUserType type) {
  DCHECK(!ash::LoginState::IsInitialized());

  ash::LoginState::Initialize();
  ash::LoginState::Get()->set_always_logged_in(false);
  ash::LoginState::Get()->SetLoggedInState(state, type);
}

DeviceCommandRebootJobTestBase::DeviceCommandRebootJobTestBase() {
  chromeos::PowerManagerClient::InitializeFake();
  ash::CryptohomeMiscClient::InitializeFake();
  ash::SessionManagerClient::InitializeFake();
  chromeos::FakePowerManagerClient::Get()->set_tick_clock(
      task_environment_.GetMockTickClock());

  RebootNotificationsScheduler::RegisterProfilePrefs(prefs_->registry());
}

DeviceCommandRebootJobTestBase::~DeviceCommandRebootJobTestBase() {
  ash::SessionManagerClient::Shutdown();
  ash::CryptohomeMiscClient::Shutdown();
  chromeos::PowerManagerClient::Shutdown();
}

std::unique_ptr<DeviceCommandRebootJob>
DeviceCommandRebootJobTestBase::CreateAndInitializeCommand(
    base::TimeDelta age_of_command,
    base::TimeDelta user_session_reboot_delay) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_DEVICE_REBOOT);
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());

  constexpr char kPayloadDictionary[] =
      R"({"user_session_delay_seconds": %ld})";

  std::string delay_payload = base::StringPrintf(
      kPayloadDictionary, user_session_reboot_delay.InSeconds());
  command_proto.set_payload(std::move(delay_payload));

  return CreateAndInitializeCommand(std::move(command_proto));
}

std::unique_ptr<DeviceCommandRebootJob>
DeviceCommandRebootJobTestBase::CreateAndInitializeCommand(
    enterprise_management::RemoteCommand command_proto) {
  // `PowerManagerClient` might be nullptr, use `PowerManagerClient::Get`
  // instead of FakePowerManagerClient::Get which crashes.
  auto job = std::make_unique<DeviceCommandRebootJobForTesting>(
      chromeos::PowerManagerClient::Get(), ash::LoginState::Get(),
      &session_termination_manager_, fake_notifications_scheduler_.get(),
      task_environment_.GetMockClock(), task_environment_.GetMockTickClock(),
      /*get_boot_time_callback=*/base::BindLambdaForTesting([this]() {
        return start_ticks_;
      }));

  const bool is_initialized =
      job->Init(NowTicks(), command_proto, em::SignedData());
  DCHECK(is_initialized);

  return job;
}

}  // namespace policy
