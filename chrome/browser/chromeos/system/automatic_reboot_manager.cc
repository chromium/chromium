// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace chromeos {
namespace system {

namespace {

const int kMinRebootUptimeMs = 60 * 60 * 1000;     // 1 hour.
const int kLoginManagerIdleTimeoutMs = 60 * 1000;  // 60 seconds.
const int kGracePeriodMs = 24 * 60 * 60 * 1000;    // 24 hours.
const int kOneKilobyte = 1 << 10;                  // 1 kB in bytes.

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
  return base::TimeDelta::FromMilliseconds(seconds * 1000.0);
}

void SaveUpdateRebootNeededUptime() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath update_reboot_needed_uptime_file;
  CHECK(base::PathService::Get(chromeos::FILE_UPDATE_REBOOT_NEEDED_UPTIME,
                               &update_reboot_needed_uptime_file));
  const base::TimeDelta last_update_reboot_needed_uptime =
      ReadTimeDeltaFromFile(update_reboot_needed_uptime_file);
  if (!last_update_reboot_needed_uptime.is_zero())
    return;

  base::FilePath uptime_file;
  CHECK(base::PathService::Get(chromeos::FILE_UPTIME, &uptime_file));
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
  base::WriteFileDescriptor(fd.get(), update_reboot_needed_uptime.c_str(),
                            update_reboot_needed_uptime.size());
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

  base::Optional<base::TimeTicks> boot_time;
  base::Optional<base::TimeTicks> update_reboot_needed_time;
};

SystemEventTimes GetSystemEventTimes() {
  base::FilePath uptime_file;
  CHECK(base::PathService::Get(chromeos::FILE_UPTIME, &uptime_file));
  base::FilePath update_reboot_needed_uptime_file;
  CHECK(base::PathService::Get(chromeos::FILE_UPDATE_REBOOT_NEEDED_UPTIME,
                               &update_reboot_needed_uptime_file));
  return SystemEventTimes(
      ReadTimeDeltaFromFile(uptime_file),
      ReadTimeDeltaFromFile(update_reboot_needed_uptime_file));
}

}  // namespace internal

AutomaticRebootManager::AutomaticRebootManager(const base::TickClock* clock)
    : clock_(clock) {
  local_state_registrar_.Init(g_browser_process->local_state());
  local_state_registrar_.Add(prefs::kUptimeLimit,
                             base::Bind(&AutomaticRebootManager::Reschedule,
                                        base::Unretained(this)));
  local_state_registrar_.Add(prefs::kRebootAfterUpdate,
                             base::Bind(&AutomaticRebootManager::Reschedule,
                                        base::Unretained(this)));
  notification_registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());

  PowerManagerClient::Get()->AddObserver(this);
  DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(this);

  // If no user is logged in, a reboot may be performed whenever the user is
  // idle. Start listening for user activity to determine whether the user is
  // idle or not.
  if (!session_manager::SessionManager::Get()->IsSessionStarted()) {
    if (ui::UserActivityDetector::Get())
      ui::UserActivityDetector::Get()->AddObserver(this);
    session_manager_observer_.Add(session_manager::SessionManager::Get());
    login_screen_idle_timer_ = std::make_unique<base::OneShotTimer>();
    OnUserActivity(nullptr);
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::MayBlock()},
      base::BindOnce(&internal::GetSystemEventTimes),
      base::BindOnce(&AutomaticRebootManager::Init,
                     weak_ptr_factory_.GetWeakPtr()));
}

AutomaticRebootManager::~AutomaticRebootManager() {
  for (auto& observer : observers_)
    observer.WillDestroyAutomaticRebootManager();

  PowerManagerClient::Get()->RemoveObserver(this);
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
  if (ui::UserActivityDetector::Get())
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

void AutomaticRebootManager::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  MaybeReboot(true);
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

  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&SaveUpdateRebootNeededUptime));

  update_reboot_needed_time_ = clock_->NowTicks();

  Reschedule();
}

void AutomaticRebootManager::OnUserActivity(const ui::Event* event) {
  if (!login_screen_idle_timer_)
    return;

  // Destroying and re-creating the timer ensures that Start() posts a fresh
  // task with a delay of exactly |kLoginManagerIdleTimeoutMs|, ensuring that
  // the timer fires predictably in tests.
  login_screen_idle_timer_.reset(new base::OneShotTimer);
  login_screen_idle_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLoginManagerIdleTimeoutMs),
      base::Bind(&AutomaticRebootManager::MaybeReboot,
                 base::Unretained(this),
                 false));
}

void AutomaticRebootManager::OnUserSessionStarted(bool is_primary_user) {
  if (!is_primary_user)
    return;

  // A session is starting. Stop listening for user activity as it no longer is
  // a relevant criterion.
  if (ui::UserActivityDetector::Get())
    ui::UserActivityDetector::Get()->RemoveObserver(this);
  session_manager_observer_.RemoveAll();
  login_screen_idle_timer_.reset();
}

void AutomaticRebootManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_APP_TERMINATING);
  if (session_manager::SessionManager::Get()->IsSessionStarted()) {
    // The browser is terminating during a session, either because the session
    // is ending or because the browser is being restarted.
    MaybeReboot(true);
  }
}

// static
void AutomaticRebootManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kUptimeLimit, 0);
  registry->RegisterBooleanPref(prefs::kRebootAfterUpdate, false);
}

void AutomaticRebootManager::Init(
    const internal::SystemEventTimes& system_event_times) {
  initialized_.Signal();

  const base::TimeDelta offset = clock_->NowTicks() - base::TimeTicks::Now();
  if (system_event_times.boot_time) {
    // Convert the time at which the device was booted to |clock_| ticks.
    boot_time_ = *system_event_times.boot_time + offset;
  }
  if (system_event_times.update_reboot_needed_time) {
    // Convert the time at which a reboot became necessary to |clock_| ticks.
    update_reboot_needed_time_ =
        *system_event_times.update_reboot_needed_time + offset;
  } else {
    UpdateStatusChanged(
        DBusThreadManager::Get()->GetUpdateEngineClient()->GetLastStatus());
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
  const base::TimeDelta uptime_limit = base::TimeDelta::FromSeconds(
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
  // |kMinRebootUptimeMs|.
  const base::TimeTicks now = clock_->NowTicks();
  const base::TimeTicks grace_start_time = std::max(
      reboot_request_time,
      *boot_time_ + base::TimeDelta::FromMilliseconds(kMinRebootUptimeMs));

  // Set up a timer for the start of the grace period. If the grace period
  // started in the past, the timer is still used with its delay set to zero.
  if (!grace_start_timer_)
    grace_start_timer_.reset(new base::OneShotTimer);
  VLOG(1) << "Scheduling reboot attempt in " << (grace_start_time - now);
  grace_start_timer_->Start(FROM_HERE,
                            std::max(grace_start_time - now, base::TimeDelta()),
                            base::Bind(&AutomaticRebootManager::RequestReboot,
                                       base::Unretained(this)));

  const base::TimeTicks grace_end_time = grace_start_time +
      base::TimeDelta::FromMilliseconds(kGracePeriodMs);
  // Set up a timer for the end of the grace period. If the grace period ended
  // in the past, the timer is still used with its delay set to zero.
  if (!grace_end_timer_)
    grace_end_timer_.reset(new base::OneShotTimer);
  VLOG(1) << "Scheduling unconditional reboot in " << (grace_end_time - now);
  grace_end_timer_->Start(
      FROM_HERE, std::max(grace_end_time - now, base::TimeDelta()),
      base::Bind(&AutomaticRebootManager::Reboot, base::Unretained(this)));
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
  PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER, "automatic reboot manager");
}

}  // namespace system
}  // namespace chromeos
