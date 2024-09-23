// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/automatic_reboot_manager.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ash {
namespace system {

namespace {

constexpr base::TimeDelta kMinRebootUptime = base::Hours(1);  // 1 hour.
constexpr char kMinRebootUptimeMsSwitch[] =
    "min-reboot-uptime-ms";  // Switch to override |kMinRebootUptime| for
                             // testing
const int kLoginManagerIdleTimeoutMs = 60 * 1000;  // 60 seconds.
const int kGracePeriodMs = 24 * 60 * 60 * 1000;    // 24 hours.
const int kOneKilobyte = 1 << 10;                  // 1 kB in bytes.
const int kResumeRebootDelayMs = 100;

base::TimeDelta ReadTimeDeltaFromFile(const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedFD fd(
      HANDLE_EINTR(open(path.value().c_str(), O_RDONLY | O_NOFOLLOW)));
  if (!fd.is_valid())
    return base::TimeDelta();

  std::string contents;
  char buffer[kOneKilobyte];
  ssize_t length;
  while ((length = HANDLE_EINTR(read(fd.get(), buffer, sizeof(buffer)))) > 0)
    contents.append(buffer, length);

  double seconds;
  if (!base::StringToDouble(contents.substr(0, contents.find(' ')), &seconds) ||
      seconds < 0.0) {
    return base::TimeDelta();
  }
  return base::Milliseconds(seconds * 1000.0);
}

void SaveUpdateRebootNeededUptime() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath update_reboot_needed_uptime_file;
  CHECK(base::PathService::Get(FILE_UPDATE_REBOOT_NEEDED_UPTIME,
                               &update_reboot_needed_uptime_file));
  const base::TimeDelta last_update_reboot_needed_uptime =
      ReadTimeDeltaFromFile(update_reboot_needed_uptime_file);
  if (!last_update_reboot_needed_uptime.is_zero())
    return;

  base::FilePath uptime_file;
  CHECK(base::PathService::Get(FILE_UPTIME, &uptime_file));
  const base::TimeDelta uptime = ReadTimeDeltaFromFile(uptime_file);
  if (uptime.is_zero())
    return;

  base::ScopedFD fd(HANDLE_EINTR(
      open(update_reboot_needed_uptime_file.value().c_str(),
           O_CREAT | O_WRONLY | O_TRUNC | O_NOFOLLOW,
           0666)));
  if (!fd.is_valid())
    return;

  std::string update_reboot_needed_uptime =
      base::NumberToString(uptime.InSecondsF());
  base::WriteFileDescriptor(fd.get(), update_reboot_needed_uptime);
}

}  // namespace

namespace internal {

// The current uptime and the uptime at which an update was applied and a
// reboot became necessary (if any). Used to pass this information from the
// blocking thread pool to the UI thread.
struct SystemEventTimes {
  SystemEventTimes(const base::TimeDelta& uptime,
                   const base::TimeDelta& update_reboot_needed_uptime) {
    if (uptime.is_zero())
      return;
    boot_time = base::TimeTicks::Now() - uptime;
    if (update_reboot_needed_uptime.is_zero())
      return;
    // Calculate the time at which an update was applied and a reboot became
    // necessary in base::TimeTicks::Now() ticks.
    update_reboot_needed_time = *boot_time + update_reboot_needed_uptime;
  }

  SystemEventTimes() = default;

  std::optional<base::TimeTicks> boot_time;
  std::optional<base::TimeTicks> update_reboot_needed_time;
};

SystemEventTimes GetSystemEventTimes() {
  base::FilePath uptime_file;
  CHECK(base::PathService::Get(FILE_UPTIME, &uptime_file));
  base::FilePath update_reboot_needed_uptime_file;
  CHECK(base::PathService::Get(FILE_UPDATE_REBOOT_NEEDED_UPTIME,
                               &update_reboot_needed_uptime_file));
  return SystemEventTimes(
      ReadTimeDeltaFromFile(uptime_file),
      ReadTimeDeltaFromFile(update_reboot_needed_uptime_file));
}

}  // namespace internal

AutomaticRebootManager::AutomaticRebootManager(
    const base::Clock* clock,
    const base::TickClock* tick_clock)
    : clock_(clock), tick_clock_(tick_clock) {
  local_state_registrar_.Init(g_browser_process->local_state());
  local_state_registrar_.Add(
      prefs::kUptimeLimit,
      base::BindRepeating(&AutomaticRebootManager::Reschedule,
                          base::Unretained(this)));
  local_state_registrar_.Add(
      prefs::kRebootAfterUpdate,
      base::BindRepeating(&AutomaticRebootManager::Reschedule,
                          base::Unretained(this)));
  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
          &AutomaticRebootManager::OnAppTerminating, base::Unretained(this)));

  chromeos::PowerManagerClient::Get()->AddObserver(this);
  UpdateEngineClient::Get()->AddObserver(this);

  // If no user is logged in, a reboot may be performed whenever the user is
  // idle. Start listening for user activity to determine whether the user is
  // idle or not.
  if (!session_manager::SessionManager::Get()->IsSessionStarted()) {
    ui::UserActivityDetector::Get()->AddObserver(this);
    session_manager_observation_.Observe(
        session_manager::SessionManager::Get());
    login_screen_idle_timer_ = std::make_unique<base::OneShotTimer>();
    OnUserActivity(nullptr);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&internal::GetSystemEventTimes),
      base::BindOnce(&AutomaticRebootManager::Init,
                     weak_ptr_factory_.GetWeakPtr()));
}

AutomaticRebootManager::~AutomaticRebootManager() {
  for (auto& observer : observers_)
    observer.WillDestroyAutomaticRebootManager();

  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  UpdateEngineClient::Get()->RemoveObserver(this);
  ui::UserActivityDetector::Get()->RemoveObserver(this);
}

void AutomaticRebootManager::AddObserver(
    AutomaticRebootManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void AutomaticRebootManager::RemoveObserver(
    AutomaticRebootManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool AutomaticRebootManager::WaitForInitForTesting(
    const base::TimeDelta& timeout) {
  return initialized_.TimedWait(timeout);
}

void AutomaticRebootManager::SuspendDone(base::TimeDelta sleep_duration) {
  // Ignore session to allow rebooting kiosk apps on resume. In case the session
  // is a user session, there is an additional check in the Reboot method below.
  // We post a delayed task to ensure that we run any due grace timers and
  // update |reboot_requested_| flag before we try to reboot.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AutomaticRebootManager::MaybeReboot,
                     base::Unretained(this), true),
      base::Milliseconds(kResumeRebootDelayMs));
}

void AutomaticRebootManager::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  // Ignore repeated notifications that a reboot is necessary. This is important
  // so that only the time of the first notification is taken into account and
  // repeated notifications do not postpone the reboot request and grace period.
  if (status.current_operation() !=
          update_engine::Operation::UPDATED_NEED_REBOOT ||
      !boot_time_ || update_reboot_needed_time_) {
    return;
  }

  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
                             base::BindOnce(&SaveUpdateRebootNeededUptime));

  update_reboot_needed_time_ = tick_clock_->NowTicks();

  Reschedule();
}

void AutomaticRebootManager::OnUserActivity(const ui::Event* event) {
  if (!login_screen_idle_timer_)
    return;

  // Destroying and re-creating the timer ensures that Start() posts a fresh
  // task with a delay of exactly |kLoginManagerIdleTimeoutMs|, ensuring that
  // the timer fires predictably in tests.
  login_screen_idle_timer_ = std::make_unique<base::OneShotTimer>();
  login_screen_idle_timer_->Start(
      FROM_HERE, base::Milliseconds(kLoginManagerIdleTimeoutMs),
      base::BindOnce(&AutomaticRebootManager::MaybeReboot,
                     base::Unretained(this), false));
}

void AutomaticRebootManager::OnUserSessionStarted(bool is_primary_user) {
  if (!is_primary_user)
    return;

  // A session is starting. Stop listening for user activity as it no longer is
  // a relevant criterion.
  ui::UserActivityDetector::Get()->RemoveObserver(this);
  session_manager_observation_.Reset();
  login_screen_idle_timer_.reset();
}

// static
void AutomaticRebootManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kUptimeLimit, 0);
  registry->RegisterBooleanPref(prefs::kRebootAfterUpdate, false);
}

void AutomaticRebootManager::Init(
    const internal::SystemEventTimes& system_event_times) {
  initialized_.Signal();

  const base::TimeDelta offset =
      tick_clock_->NowTicks() - base::TimeTicks::Now();
  if (system_event_times.boot_time) {
    // Convert the time at which the device was booted to |tick_clock_| ticks.
    boot_time_ = *system_event_times.boot_time + offset;
  }
  if (system_event_times.update_reboot_needed_time) {
    // Convert the time at which a reboot became necessary to |tick_clock_|
    // ticks.
    update_reboot_needed_time_ =
        *system_event_times.update_reboot_needed_time + offset;
  } else {
    UpdateStatusChanged(UpdateEngineClient::Get()->GetLastStatus());
  }

  Reschedule();
}

void AutomaticRebootManager::Reschedule() {
  VLOG(1) << "Rescheduling reboot";
  // Safeguard against reboot loops under error conditions: If the boot time is
  // unavailable because /proc/uptime could not be read, do nothing.
  if (!boot_time_)
    return;

  // Assume that no reboot has been requested.
  reboot_requested_ = false;

  // If an uptime limit is set, calculate the time at which it should cause a
  // reboot to be requested.
  const base::TimeDelta uptime_limit = base::Seconds(
      local_state_registrar_.prefs()->GetInteger(prefs::kUptimeLimit));
  base::TimeTicks reboot_request_time = *boot_time_ + uptime_limit;
  bool have_reboot_request_time = !uptime_limit.is_zero();
  if (have_reboot_request_time)
    reboot_reason_ = AutomaticRebootManagerObserver::REBOOT_REASON_PERIODIC;

  // If the policy to automatically reboot after an update is enabled and an
  // update has been applied, set the time at which a reboot should be
  // requested to the minimum of its current value and the time when the reboot
  // became necessary.
  if (update_reboot_needed_time_ &&
      local_state_registrar_.prefs()->GetBoolean(prefs::kRebootAfterUpdate) &&
      (!have_reboot_request_time ||
       *update_reboot_needed_time_ < reboot_request_time)) {
    VLOG(1) << "Scheduling reboot because of OS update";
    reboot_request_time = *update_reboot_needed_time_;
    have_reboot_request_time = true;
    reboot_reason_ = AutomaticRebootManagerObserver::REBOOT_REASON_OS_UPDATE;
  }

  // If no reboot should be requested, remove any grace period.
  if (!have_reboot_request_time) {
    grace_start_timer_.reset();
    grace_end_timer_.reset();
    return;
  }

  // Safeguard against reboot loops: Ensure that the uptime after which a reboot
  // is actually requested and the grace period begins is never less than
  // |kMinRebootUptime| or the value passed in |kMinRebootUptimeMsSwitch|.
  base::TimeDelta minRebootUptime = kMinRebootUptime;

  if (auto* command_line = base::CommandLine::ForCurrentProcess();
      command_line && command_line->HasSwitch(kMinRebootUptimeMsSwitch)) {
    int parsed_value = 0;
    std::string switch_value =
        command_line->GetSwitchValueASCII(kMinRebootUptimeMsSwitch);

    if (base::StringToInt(switch_value, &parsed_value)) {
      minRebootUptime = base::Milliseconds(parsed_value);
    } else {
      LOG(WARNING) << "Failed to parse kMinRebootUptimeMsSwitch's value "
                   << switch_value;
    }
  }

  const base::TimeTicks now = tick_clock_->NowTicks();
  const base::Time wall_clock_now = clock_->Now();
  const base::TimeTicks grace_start_time =
      std::max(reboot_request_time, *boot_time_ + minRebootUptime);

  // Set up a timer for the start of the grace period. If the grace period
  // started in the past, the timer is still used with its delay set to zero.
  if (!grace_start_timer_)
    grace_start_timer_ =
        std::make_unique<base::WallClockTimer>(clock_, tick_clock_);
  VLOG(1) << "Scheduling reboot attempt at "
          << wall_clock_now + (grace_start_time - now);
  grace_start_timer_->Start(
      FROM_HERE,
      wall_clock_now + std::max(grace_start_time - now, base::TimeDelta()),
      base::BindOnce(&AutomaticRebootManager::RequestReboot,
                     base::Unretained(this)));

  const base::TimeTicks grace_end_time =
      grace_start_time + base::Milliseconds(kGracePeriodMs);
  // Set up a timer for the end of the grace period. If the grace period ended
  // in the past, the timer is still used with its delay set to zero.
  if (!grace_end_timer_)
    grace_end_timer_ =
        std::make_unique<base::WallClockTimer>(clock_, tick_clock_);
  VLOG(1) << "Scheduling unconditional reboot at "
          << wall_clock_now + (grace_end_time - now);
  grace_end_timer_->Start(
      FROM_HERE,
      wall_clock_now + std::max(grace_end_time - now, base::TimeDelta()),
      base::BindOnce(&AutomaticRebootManager::Reboot, base::Unretained(this)));
}

void AutomaticRebootManager::RequestReboot() {
  VLOG(1) << "Reboot requested, reason: " << reboot_reason_;
  reboot_requested_ = true;
  DCHECK_NE(AutomaticRebootManagerObserver::REBOOT_REASON_UNKNOWN,
            reboot_reason_);
  for (auto& observer : observers_)
    observer.OnRebootRequested(reboot_reason_);
  MaybeReboot(false);
}

void AutomaticRebootManager::MaybeReboot(bool ignore_session) {
  // Do not reboot if any of the following applies:
  // * No reboot has been requested.
  // * A user is interacting with the login screen.
  // * A session is in progress and |ignore_session| is not set.
  if (!reboot_requested_ ||
      (login_screen_idle_timer_ && login_screen_idle_timer_->IsRunning()) ||
      (!ignore_session &&
       session_manager::SessionManager::Get()->IsSessionStarted())) {
    return;
  }

  Reboot();
}

void AutomaticRebootManager::Reboot() {
  // If a non-kiosk-app session is in progress, do not reboot.
  if (user_manager::UserManager::Get()->IsUserLoggedIn() &&
      !user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    VLOG(1) << "Skipping reboot because non-kiosk session is active";
    return;
  }

  login_screen_idle_timer_.reset();
  grace_start_timer_.reset();
  grace_end_timer_.reset();
  VLOG(1) << "Rebooting immediately.";
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER, "automatic reboot manager");
}

void AutomaticRebootManager::OnAppTerminating() {
  if (session_manager::SessionManager::Get()->IsSessionStarted()) {
    // The browser is terminating during a session, either because the session
    // is ending or because the browser is being restarted.
    MaybeReboot(true);
  }
}

}  // namespace system
}  // namespace ash
