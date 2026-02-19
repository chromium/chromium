// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/startup/startup_launch_manager.h"

#include <optional>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/startup/startup_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

using auto_launch_util::StartupLaunchMode;

namespace {

// This method sets the pref to trial group value if user has not explicitly set
// it, and returns the infobar type to show.
std::optional<StartupLaunchInfoBarManager::InfoBarType>
UpdatePrefAndGetInfoBarType(PrefService* local_state) {
  if (!local_state->FindPreference(prefs::kForegroundLaunchOnLogin)
           ->IsDefaultValue()) {
    return std::nullopt;
  }

  // Update the pref's default value as this has lower priority than a user-set
  // value.
  const auto trial_group = features::GetLaunchOnStartupDefaultPreference();
  switch (trial_group) {
    case features::LaunchOnStartupDefaultPreference::kDisabled:
      local_state->SetDefaultPrefValue(prefs::kForegroundLaunchOnLogin,
                                       base::Value(false));
      return StartupLaunchInfoBarManager::InfoBarType::kForegroundOptIn;
    case features::LaunchOnStartupDefaultPreference::kEnabled:
      local_state->SetDefaultPrefValue(prefs::kForegroundLaunchOnLogin,
                                       base::Value(true));
      return StartupLaunchInfoBarManager::InfoBarType::kForegroundOptOut;
  }
}

bool ShouldShowInfoBars() {
  constexpr int kMaxPromptCount = 5;
  constexpr int kRepromptDurationDays = 21;

  PrefService* local_state = g_browser_process->local_state();

  const bool is_accepted =
      local_state->GetBoolean(prefs::kStartupLaunchInfobarAccepted);
  const int declined_count =
      local_state->GetInteger(prefs::kStartupLaunchInfobarDeclinedCount);
  const base::Time last_declined_time =
      local_state->GetTime(prefs::kStartupLaunchInfobarLastDeclinedTime);

  // If infobar is already accepted once, or declined more than maximum allowed
  // declines, we don't show the infobar.
  if (is_accepted || declined_count >= kMaxPromptCount) {
    return false;
  }

  // Show if the user has never declined the prompt.
  if (declined_count == 0) {
    return true;
  }

  // Show if it has been long enough since the last declined time
  return (base::Time::Now() - last_declined_time) >
         base::Days(kRepromptDurationDays);
}

}  // namespace

StartupLaunchManager::Client::Client(StartupLaunchReason launch_reason)
    : launch_reason_(launch_reason) {
  // Acquires a shared write lock to prevent StartupLaunchManager from
  // processing the final launch configuration until this client has
  // initialized.
  auto* launch_manager = StartupLaunchManager::From(g_browser_process);
  launch_manager->AcquireSharedWriteLock();
}

StartupLaunchManager::Client::~Client() {
  // If the client is destroyed before being fully initialized, release the lock
  // to prevent launch manager from hanging.
  auto* launch_manager = StartupLaunchManager::From(g_browser_process);
  if (launch_manager && !launch_enabled_.has_value()) {
    launch_manager->ReleaseSharedWriteLock();
  }
}

void StartupLaunchManager::Client::SetLaunchOnStartup(bool enable_launch) {
  // Do nothing if the state hasn't changed.
  if (launch_enabled_ == enable_launch) {
    return;
  }
  // If we are registering for the first time, release the startup launch
  // lock so that StartupLaunchManager can flush to the registry.
  const bool release_lock = !launch_enabled_.has_value();
  launch_enabled_.emplace(enable_launch);

  auto* launch_manager = StartupLaunchManager::From(g_browser_process);
  if (enable_launch) {
    launch_manager->RegisterLaunchOnStartup(launch_reason_);
  } else {
    launch_manager->UnregisterLaunchOnStartup(launch_reason_);
  }

  if (release_lock) {
    launch_manager->ReleaseSharedWriteLock();
  }
}

DEFINE_USER_DATA(StartupLaunchManager);

StartupLaunchManager::StartupLaunchManager(BrowserProcess* browser_process)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      scoped_unowned_user_data_(browser_process->GetUnownedUserDataHost(),
                                *this) {
  // Acquire a lock so that any writes to registry are deferred until `Init()`
  // is called.
  AcquireSharedWriteLock();
  if (features::IsForegroundLaunchEnabled()) {
    PrefService* local_state = g_browser_process->local_state();

    // Update the pref as per the trial group.
    infobar_type_ = UpdatePrefAndGetInfoBarType(local_state);

    // Register a callback that will run when this pref is changed.
    foreground_launch_on_login_.Init(
        prefs::kForegroundLaunchOnLogin, local_state,
        base::BindRepeating(&StartupLaunchManager::OnLaunchOnStartupPrefChanged,
                            base::Unretained(this)));

    // Initialize StartupLaunchManager to use current value of the pref.
    UpdateForegroundLaunchRegistration();
  } else {
    // Removes foreground launch if feature flag is disabled, but keeps the pref
    // unchanged. This allows us to resume the experiment if it needs to be
    // paused anytime.
    UnregisterLaunchOnStartup(StartupLaunchReason::kForeground);
  }
}

StartupLaunchManager::~StartupLaunchManager() = default;

void StartupLaunchManager::SetInfoBarManager(
    std::unique_ptr<StartupLaunchInfoBarManager> manager) {
  infobar_manager_observation_.Reset();
  if (is_showing_infobar_ && infobar_manager_) {
    infobar_manager_->CloseAllInfoBars();
    is_showing_infobar_ = false;
  }
  infobar_manager_ = std::move(manager);
  if (infobar_manager_) {
    infobar_manager_observation_.Observe(infobar_manager_.get());
  }
}

void StartupLaunchManager::MaybeShowInfoBars() {
  if (infobar_type_.has_value() && ShouldShowInfoBars()) {
    infobar_manager_->ShowInfoBars(*infobar_type_);
    is_showing_infobar_ = true;
  }
}

void StartupLaunchManager::OnInfoBarDismissed() {
  is_showing_infobar_ = false;
}

// static
StartupLaunchManager* StartupLaunchManager::From(
    BrowserProcess* browser_process) {
  return browser_process ? Get(browser_process->GetUnownedUserDataHost())
                         : nullptr;
}

void StartupLaunchManager::AcquireSharedWriteLock() {
  ++lock_counter_;
}

void StartupLaunchManager::ReleaseSharedWriteLock() {
  // No-op if locks were already forcefully released.
  if (lock_counter_ == 0) {
    return;
  }
  // After releasing the last lock, cancel the force release lock task and write
  // the final configuration to registry.
  if (--lock_counter_ == 0) {
    fallback_timer_.Stop();
    UpdateLaunchOnStartup(GetStartupLaunchMode());
  }
}

void StartupLaunchManager::CommitLaunchOnStartupState() {
  // Release the lock acquired in the constructor.
  ReleaseSharedWriteLock();

  // Set up a task to release all shared write locks, and writing the pending
  // changes to the registry if any client fails to do their initial
  // registration for one minute.
  fallback_timer_.Start(FROM_HERE, base::Minutes(1), this,
                        &StartupLaunchManager::ForceReleaseAllLocks);
}

// TODO(crbug.com/467376419): Record count of such occurrences.
void StartupLaunchManager::ForceReleaseAllLocks() {
  // No-op if the locks are already released.
  if (lock_counter_ == 0) {
    return;
  }
  lock_counter_ = 0;
  UpdateLaunchOnStartup(GetStartupLaunchMode());
}

void StartupLaunchManager::OnLaunchOnStartupPrefChanged() {
  UpdateForegroundLaunchRegistration();

  constexpr char histogram_name[] =
      "Startup.Launch.Foreground.PreferenceChanged";
  base::UmaHistogramBoolean(histogram_name,
                            foreground_launch_on_login_.GetValue());
}

void StartupLaunchManager::UpdateForegroundLaunchRegistration() {
  if (foreground_launch_on_login_.GetValue()) {
    RegisterLaunchOnStartup(StartupLaunchReason::kForeground);
  } else {
    UnregisterLaunchOnStartup(StartupLaunchReason::kForeground);
  }

  if (is_showing_infobar_) {
    infobar_manager_->CloseAllInfoBars();
    is_showing_infobar_ = false;
  }
}

std::optional<StartupLaunchMode> StartupLaunchManager::GetStartupLaunchMode()
    const {
  if (registered_launch_reasons_.empty()) {
    return std::nullopt;
  }
  // Foreground launch takes precedence as - having foreground launch enabled is
  // the same as having both foreground and background launches enabled
  // simultaneously.
  if (registered_launch_reasons_.Has(StartupLaunchReason::kForeground)) {
    return StartupLaunchMode::kForeground;
  }
  return StartupLaunchMode::kBackground;
}

void StartupLaunchManager::RegisterLaunchOnStartup(StartupLaunchReason reason) {
  if (lock_counter_ > 0) {
    registered_launch_reasons_.Put(reason);
    return;
  }
  const auto previous_startup_mode = GetStartupLaunchMode();
  registered_launch_reasons_.Put(reason);
  const auto current_startup_mode = GetStartupLaunchMode();

  if (previous_startup_mode != current_startup_mode) {
    UpdateLaunchOnStartup(current_startup_mode);
  }
}

void StartupLaunchManager::UnregisterLaunchOnStartup(
    StartupLaunchReason reason) {
  if (lock_counter_ > 0) {
    registered_launch_reasons_.Remove(reason);
    return;
  }
  const auto previous_startup_mode = GetStartupLaunchMode();
  registered_launch_reasons_.Remove(reason);
  const auto current_startup_mode = GetStartupLaunchMode();

  if (!current_startup_mode.has_value() ||
      previous_startup_mode != current_startup_mode) {
    UpdateLaunchOnStartup(current_startup_mode);
  }
}

void StartupLaunchManager::UpdateLaunchOnStartup(
    std::optional<StartupLaunchMode> startup_launch_mode) {
  // This functionality is only defined for default profile, currently.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUserDataDir)) {
    return;
  }
  task_runner_->PostTask(
      FROM_HERE, startup_launch_mode.has_value()
                     ? base::BindOnce(auto_launch_util::EnableStartAtLogin,
                                      *startup_launch_mode)
                     : base::BindOnce(auto_launch_util::DisableStartAtLogin));
}
