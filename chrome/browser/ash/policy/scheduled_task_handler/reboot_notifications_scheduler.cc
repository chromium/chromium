// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/reboot_notifications_scheduler.h"

#include "ash/constants/ash_pref_names.h"
#include "base/system/sys_info.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_prefs/user_prefs.h"

namespace policy {

namespace {
constexpr base::TimeDelta kNotificationDelay = base::Hours(1);
constexpr base::TimeDelta kDialogDelay = base::Minutes(5);
constexpr base::TimeDelta kGraceTime = base::Hours(1);
}  // namespace

RebootNotificationsScheduler* RebootNotificationsScheduler::instance = nullptr;

RebootNotificationsScheduler::RebootNotificationsScheduler()
    : RebootNotificationsScheduler(base::DefaultClock::GetInstance(),
                                   base::DefaultTickClock::GetInstance()) {}

RebootNotificationsScheduler::RebootNotificationsScheduler(
    const base::Clock* clock,
    const base::TickClock* tick_clock)
    : notification_timer_(clock, tick_clock), dialog_timer_(clock, tick_clock) {
  DCHECK(!RebootNotificationsScheduler::Get());
  RebootNotificationsScheduler::SetInstance(this);
  if (session_manager::SessionManager::Get())
    observation_.Observe(session_manager::SessionManager::Get());
}

RebootNotificationsScheduler::~RebootNotificationsScheduler() {
  DCHECK_EQ(instance, this);
  observation_.Reset();
  RebootNotificationsScheduler::SetInstance(nullptr);
}

// static
RebootNotificationsScheduler* RebootNotificationsScheduler::Get() {
  return RebootNotificationsScheduler::instance;
}

// static
void RebootNotificationsScheduler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(ash::prefs::kShowPostRebootNotification, false);
}

// static
bool RebootNotificationsScheduler::ShouldShowPostRebootNotification(
    Profile* profile) {
  DCHECK(profile);
  PrefService* prefs = user_prefs::UserPrefs::Get(profile);
  return IsPostRebootPrefSet(prefs);
}

void RebootNotificationsScheduler::SchedulePendingRebootNotifications(
    base::OnceClosure reboot_callback,
    const base::Time& reboot_time) {
  ResetState();
  if (ShouldApplyGraceTime(reboot_time)) {
    return;
  }

  reboot_time_ = reboot_time;
  reboot_callback_ = std::move(reboot_callback);
  base::TimeDelta delay = GetRebootDelay(reboot_time_);

  if (delay > kNotificationDelay) {
    base::Time timer_run_time = reboot_time_ - kNotificationDelay;
    notification_timer_.Start(
        FROM_HERE, timer_run_time,
        base::BindOnce(
            &RebootNotificationsScheduler::MaybeShowPendingRebootNotification,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    MaybeShowPendingRebootNotification();
  }

  if (delay > kDialogDelay) {
    base::Time timer_run_time = reboot_time_ - kDialogDelay;
    dialog_timer_.Start(
        FROM_HERE, timer_run_time,
        base::BindOnce(
            &RebootNotificationsScheduler::MaybeShowPendingRebootDialog,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  MaybeShowPendingRebootDialog();
}

void RebootNotificationsScheduler::SchedulePostRebootNotification() {
  PrefService* prefs = GetPrefsForActiveProfile();
  if (prefs) {
    prefs->SetBoolean(ash::prefs::kShowPostRebootNotification, true);
  }
}

void RebootNotificationsScheduler::OnUserSessionStarted(bool is_primary_user) {
  // Return if we need to wait for the initialization of full restore service.
  if (ShouldWaitFullRestoreInit())
    return;

  MaybeShowPostRebootNotification(true /*show_simple_notification*/);
}

void RebootNotificationsScheduler::MaybeShowPostRebootNotification(
    bool show_simple_notification) {
  PrefService* prefs = GetPrefsForActiveProfile();
  // Return if the pref is not set for the profile.
  if (!IsPostRebootPrefSet(prefs))
    return;

  if (show_simple_notification) {
    notification_controller_.MaybeShowPostRebootNotification();
  }
  prefs->SetBoolean(ash::prefs::kShowPostRebootNotification, false);
  // No need to observe any more, since we showed the post reboot notification,
  // either as a simple one or integrated with full restore.
  observation_.Reset();
}

void RebootNotificationsScheduler::ResetState() {
  if (notification_timer_.IsRunning())
    notification_timer_.Stop();
  if (dialog_timer_.IsRunning())
    dialog_timer_.Stop();
  CloseNotifications();
  reboot_callback_.Reset();
}

bool RebootNotificationsScheduler::ShouldApplyGraceTime(
    const base::Time& reboot_time) const {
  base::TimeDelta delay = GetRebootDelay(reboot_time);
  return ((delay + GetSystemUptime()) <= kGraceTime);
}

void RebootNotificationsScheduler::MaybeShowPendingRebootNotification() {
  notification_controller_.MaybeShowPendingRebootNotification(
      reboot_time_,
      base::BindRepeating(&RebootNotificationsScheduler::OnRebootButtonClicked,
                          base::Unretained(this)));
}

void RebootNotificationsScheduler::MaybeShowPendingRebootDialog() {
  notification_controller_.MaybeShowPendingRebootDialog(
      reboot_time_,
      base::BindOnce(&RebootNotificationsScheduler::OnRebootButtonClicked,
                     base::Unretained(this)));
}

PrefService* RebootNotificationsScheduler::GetPrefsForActiveProfile() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return nullptr;
  return user_prefs::UserPrefs::Get(profile);
}

void RebootNotificationsScheduler::OnRebootButtonClicked() {
  DCHECK(reboot_callback_);
  std::move(reboot_callback_).Run();
}

void RebootNotificationsScheduler::SetInstance(
    RebootNotificationsScheduler* reboot_notifications_scheduler) {
  RebootNotificationsScheduler::instance = reboot_notifications_scheduler;
}

const base::Time RebootNotificationsScheduler::GetCurrentTime() const {
  return base::Time::Now();
}

const base::TimeDelta RebootNotificationsScheduler::GetSystemUptime() const {
  return base::SysInfo::Uptime();
}

base::TimeDelta RebootNotificationsScheduler::GetRebootDelay(
    const base::Time& reboot_time) const {
  return (reboot_time - GetCurrentTime());
}

void RebootNotificationsScheduler::CloseNotifications() {
  notification_controller_.CloseRebootNotification();
  notification_controller_.CloseRebootDialog();
}

bool RebootNotificationsScheduler::ShouldWaitFullRestoreInit() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return ash::full_restore::FullRestoreServiceFactory::
      IsFullRestoreAvailableForProfile(profile);
}

bool RebootNotificationsScheduler::IsPostRebootPrefSet(PrefService* prefs) {
  if (!prefs)
    return false;
  return prefs->GetBoolean(ash::prefs::kShowPostRebootNotification);
}

}  // namespace policy