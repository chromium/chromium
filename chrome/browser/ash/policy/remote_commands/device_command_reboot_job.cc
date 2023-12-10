// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_reboot_job.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/reboot_notifications_scheduler.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/cros_system_api/dbus/power_manager/dbus-constants.h"

namespace policy {

namespace {

const char kKioskRebootDescription[] = "Reboot remote command (kiosk)";
const char kLoginScreenRebootDescription[] =
    "Reboot remote command (login screen)";
const char kUserSessionRebootDescription[] =
    "Reboot remote command (user session)";

const char kPayloadUserSessionRebootDelayField[] = "user_session_delay_seconds";

constexpr base::TimeDelta kDefaultUserSessionRebootDelay = base::Minutes(5);

std::optional<base::TimeDelta> ExtractUserSessionDelayFromCommandLine() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  const std::string delay_string = command_line->GetSwitchValueASCII(
      ash::switches::kRemoteRebootCommandDelayInSecondsForTesting);

  if (delay_string.empty()) {
    return std::nullopt;
  }

  int delay_in_seconds;
  if (!base::StringToInt(delay_string, &delay_in_seconds) ||
      delay_in_seconds < 0) {
    LOG(ERROR) << "Ignored "
               << ash::switches::kRemoteRebootCommandDelayInSecondsForTesting
               << " = " << delay_string;
    return std::nullopt;
  }

  return base::Seconds(delay_in_seconds);
}

std::optional<base::TimeDelta> ExtractUserSessionDelayFromPayload(
    const std::string& command_payload) {
  const std::optional<base::Value> root =
      base::JSONReader::Read(command_payload);
  if (!root || !root->is_dict()) {
    return std::nullopt;
  }

  std::optional<int> delay_in_seconds =
      root->GetDict().FindInt(kPayloadUserSessionRebootDelayField);
  if (!delay_in_seconds || delay_in_seconds.value() < 0) {
    return std::nullopt;
  }

  return base::Seconds(delay_in_seconds.value());
}

base::TimeTicks GetBootTime() {
  return base::TimeTicks::Now() - base::SysInfo::Uptime();
}

}  // namespace

DeviceCommandRebootJob::DeviceCommandRebootJob()
    : DeviceCommandRebootJob(chromeos::PowerManagerClient::Get(),
                             ash::LoginState::Get(),
                             ash::SessionTerminationManager::Get(),
                             RebootNotificationsScheduler::Get(),
                             base::DefaultClock::GetInstance(),
                             base::DefaultTickClock::GetInstance(),
                             base::BindRepeating(GetBootTime)) {}

DeviceCommandRebootJob::DeviceCommandRebootJob(
    chromeos::PowerManagerClient* power_manager_client,
    ash::LoginState* loging_state,
    ash::SessionTerminationManager* session_termination_manager,
    RebootNotificationsScheduler* in_session_notifications_scheduler,
    const base::Clock* clock,
    const base::TickClock* tick_clock,
    GetBootTimeCallback get_boot_time_callback)
    : power_manager_client_(power_manager_client),
      login_state_(loging_state),
      session_termination_manager_(session_termination_manager),
      in_session_notifications_scheduler_(in_session_notifications_scheduler),
      in_session_reboot_timer_(clock, tick_clock),
      clock_(clock),
      get_boot_time_callback_(std::move(get_boot_time_callback)),
      user_session_delay_(kDefaultUserSessionRebootDelay) {
  DCHECK(get_boot_time_callback_);
}

DeviceCommandRebootJob::~DeviceCommandRebootJob() = default;

enterprise_management::RemoteCommand_Type DeviceCommandRebootJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_DEVICE_REBOOT;
}

bool DeviceCommandRebootJob::ParseCommandPayload(
    const std::string& command_payload) {
  const std::optional<base::TimeDelta> commandline_delay =
      ExtractUserSessionDelayFromCommandLine();

  // Ignore payload if delay is set in command line.
  if (commandline_delay) {
    user_session_delay_ = commandline_delay.value();
    return true;
  }

  const std::optional<base::TimeDelta> payload_delay =
      ExtractUserSessionDelayFromPayload(command_payload);
  if (payload_delay) {
    user_session_delay_ = payload_delay.value();
    return true;
  }

  // Don't fail even if delay is not supplied. Use default one.
  return true;
}

void DeviceCommandRebootJob::RunImpl(CallbackWithResult result_callback) {
  result_callback_ = std::move(result_callback);

  // Determines the time delta between the command having been issued and the
  // boot time of the system.
  const base::TimeDelta delta = get_boot_time_callback_.Run() - issued_time();
  // If the reboot command was issued before the system booted, we inform the
  // server that the reboot succeeded. Otherwise, the reboot must still be
  // performed and we invoke it.
  if (delta.is_positive()) {
    LOG(WARNING) << "Ignoring reboot command issued " << delta
                 << " before current boot time";
    return RunAsyncCallback(std::move(result_callback_), ResultType::kSuccess,
                            FROM_HERE);
  }

  if (!power_manager_client_) {
    LOG(ERROR) << "Power manager is not initialized. Cannot reboot.";
    return RunAsyncCallback(std::move(result_callback_), ResultType::kFailure,
                            FROM_HERE);
  }

  // Make sure `power_manager_client_` is available before requesting reboot.
  // Continue from `PowerManagerBecameAvailable`. If availability state is
  // known, call is immediate and synchronous.
  power_manager_availability_observation_.Observe(power_manager_client_);
}

void DeviceCommandRebootJob::PowerManagerBecameAvailable(bool available) {
  power_manager_availability_observation_.Reset();

  if (!available) {
    LOG(ERROR) << "Power manager is not available. Cannot reboot.";
    return RunAsyncCallback(std::move(result_callback_), ResultType::kFailure,
                            FROM_HERE);
  }

  // The device is able to reboot immediately if it has no ongoing user session:
  // if it runs in kiosk mode or is on login screen.
  if (login_state_->IsKioskSession()) {
    return DoReboot(kKioskRebootDescription);
  }

  if (!login_state_->IsUserLoggedIn()) {
    return DoReboot(kLoginScreenRebootDescription);
  }

  RebootUserSession();
}

void DeviceCommandRebootJob::RebootUserSession() {
  if (user_session_delay_.is_zero()) {
    // Do not show the reboot notification if no delay.
    return OnRebootTimeoutExpired();
  }

  const auto reboot_time = clock_->Now() + user_session_delay_;
  in_session_notifications_scheduler_->SchedulePendingRebootNotifications(
      base::BindOnce(&DeviceCommandRebootJob::OnRebootButtonClicked,
                     weak_factory_.GetWeakPtr()),
      reboot_time, RebootNotificationsScheduler::Requester::kRebootCommand);
  in_session_reboot_timer_.Start(
      FROM_HERE, reboot_time,
      base::BindOnce(&DeviceCommandRebootJob::OnRebootTimeoutExpired,
                     weak_factory_.GetWeakPtr()));

  // TODO(b/265784089): Make reboot on user logout robust. If the browser
  // crashes, all the reboot information is gone while it should be preserved.
  session_termination_manager_->SetDeviceRebootOnSignoutForRemoteCommand(
      base::BindOnce(&DeviceCommandRebootJob::OnSignout,
                     weak_factory_.GetWeakPtr()));
}

void DeviceCommandRebootJob::OnSignout() {
  // `session_termination_manager_` will initiate the reboot, just report the
  // command finished.
  RunAsyncCallback(std::move(result_callback_), ResultType::kSuccess,
                   FROM_HERE);
}

void DeviceCommandRebootJob::OnRebootButtonClicked() {
  ResetTriggeringEvents();
  DoReboot(kUserSessionRebootDescription);
}

void DeviceCommandRebootJob::OnRebootTimeoutExpired() {
  ResetTriggeringEvents();
  in_session_notifications_scheduler_->SchedulePostRebootNotification();
  DoReboot(kUserSessionRebootDescription);
}

void DeviceCommandRebootJob::ResetTriggeringEvents() {
  in_session_notifications_scheduler_->CancelRebootNotifications(
      RebootNotificationsScheduler::Requester::kRebootCommand);
  in_session_reboot_timer_.Stop();
}

void DeviceCommandRebootJob::DoReboot(const std::string& reason) {
  DCHECK(result_callback_);

  // Posting the task with a callback just before reboot request does not
  // guarantee the callback reaching `RemoteCommandsService` and is very
  // unlikely to be reported to DMServer. So the callback is mostly used for
  // testing purposes.
  // The implementation relies on `RemoteCommandsQueue` running one command at
  // the time: two reboot commands cannot exist simultaneously and will not
  // compete for the notification. The
  // callback is called at the very end of execution in order to not to have two
  // commands executed simultaneously .
  // TODO(b/252980103): Come up with a mechanism to deliver the execution result
  // to DMServer.
  RunAsyncCallback(std::move(result_callback_), ResultType::kSuccess,
                   FROM_HERE);
  power_manager_client_->RequestRestart(
      power_manager::REQUEST_RESTART_REMOTE_ACTION_REBOOT, reason);
}

// static
void DeviceCommandRebootJob::RunAsyncCallback(CallbackWithResult callback,
                                              ResultType result,
                                              base::Location from_where) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      from_where, base::BindOnce(std::move(callback), result, std::nullopt));
}

}  // namespace policy
