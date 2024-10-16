// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/authentication_flow_auto_reload_manager.h"

#include <functional>

#include "chrome/browser/ash/login/login_constants.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

base::Clock* clock_for_testing = nullptr;
const base::TickClock* tick_clock_for_testing = nullptr;

}  // namespace

static constexpr base::TimeDelta kIdleThreshold = base::Seconds(15);

AuthenticationFlowAutoReloadManager::AuthenticationFlowAutoReloadManager() {
  local_state_registrar_.Init(g_browser_process->local_state());
  local_state_registrar_.Add(
      prefs::kAuthenticationFlowAutoReloadInterval,
      base::BindRepeating(&AuthenticationFlowAutoReloadManager::OnPolicyUpdated,
                          base::Unretained(this)));
  if (clock_for_testing && tick_clock_for_testing) {
    auto_reload_timer_ = std::make_unique<base::WallClockTimer>(
        clock_for_testing, tick_clock_for_testing);
  } else {
    auto_reload_timer_ = std::make_unique<base::WallClockTimer>();
  }
}

AuthenticationFlowAutoReloadManager::~AuthenticationFlowAutoReloadManager() {
  clock_for_testing = nullptr;
  tick_clock_for_testing = nullptr;
}

void AuthenticationFlowAutoReloadManager::Activate(base::OnceClosure callback) {
  // This `ReloadGaia` callback will be needed if the policy value gets
  // updated, so it is being moved here regardless of the current policy value
  // to be able to reload the gaia page if the policy value is changed.
  callback_ = std::move(callback);

  std::optional<base::TimeDelta> reload_interval_ = GetAutoReloadInterval();
  if (!reload_interval_.has_value()) {
    return;
  }

  idle_state_observer_.Reset();
  idle_state_observer_.Observe(ui::IdlePollingService::GetInstance());
  reload_postponed_ = false;

  // Start timer for automatic reload of authentication flow
  const base::Time now =
      clock_for_testing ? clock_for_testing->Now() : base::Time::Now();
  const base::Time desired_run_time = now + reload_interval_.value();

  // Start will override previously scheduled reload if Activate was already
  // called in the past
  auto_reload_timer_->Start(
      FROM_HERE, desired_run_time,
      base::BindOnce(
          &AuthenticationFlowAutoReloadManager::ReloadAuthenticationFlow,
          base::Unretained(this)));
}

void AuthenticationFlowAutoReloadManager::ReloadAuthenticationFlow() {
  // If user was not active in the last `kIdleThreshold` seconds or the user is
  // active but the automatic reload has already been postponed once before,
  // reload the auth flow.
  if (is_idle_ || reload_postponed_) {
    ++auto_reload_attempts_;
    std::move(callback_).Run();
  } else {
    auto_reload_timer_->Stop();
    reload_postponed_ = true;

    const base::Time now =
        clock_for_testing ? clock_for_testing->Now() : base::Time::Now();

    auto_reload_timer_->Start(
        FROM_HERE, now + kPostponeInterval,
        base::BindOnce(
            &AuthenticationFlowAutoReloadManager::ReloadAuthenticationFlow,
            base::Unretained(this)));
  }
}

void AuthenticationFlowAutoReloadManager::OnIdleStateChange(
    const ui::IdlePollingService::State& state) {
  if (state.idle_time < kIdleThreshold) {
    // User activity detected less than `kIdleThreshold` seconds ago.
    is_idle_ = false;
  } else {
    // No user activity detected in the last `kIdleThreshold` seconds.
    is_idle_ = true;
  }
}

void AuthenticationFlowAutoReloadManager::Terminate() {
  idle_state_observer_.Reset();
  auto_reload_timer_->Stop();
  auto_reload_attempts_ = 0;
}

void AuthenticationFlowAutoReloadManager::OnPolicyUpdated() {
  std::optional<base::TimeDelta> reload_interval_ = GetAutoReloadInterval();
  if (!reload_interval_.has_value() && auto_reload_timer_->IsRunning()) {
    auto_reload_timer_->Stop();
  } else if (reload_interval_.has_value() && callback_) {
    // Immediately reload the authentication flow to restart the timer with the
    // new reload interval. Restarting avoids the complexity of extending the
    // existing timer, which would require calculating the remaining time from
    // the current interval and adjusting for the new interval. Accumulating the
    // new interval on the current one by calling `Activate` would delay the
    // reload beyond the configured time, potentially causing the login page to
    // expire without an automatic reload for some time.
    ReloadAuthenticationFlow();
  }
}

std::optional<base::TimeDelta>
AuthenticationFlowAutoReloadManager::GetAutoReloadInterval() {
  int pref_reload_interval =
      constants::kDefaultAuthenticationFlowAutoReloadInterval;

  PrefService* local_state = g_browser_process->local_state();
  pref_reload_interval =
      local_state->GetInteger(prefs::kAuthenticationFlowAutoReloadInterval);

  // auto reload disabled
  if (pref_reload_interval == 0) {
    return std::nullopt;
  }

  return std::make_optional<base::TimeDelta>(
      base::Minutes(pref_reload_interval));
}

// static
void AuthenticationFlowAutoReloadManager::SetClockForTesting(
    base::Clock* clock,
    const base::TickClock* tick_clock) {
  clock_for_testing = clock;
  tick_clock_for_testing = tick_clock;
}

void AuthenticationFlowAutoReloadManager::ResumeTimerForTesting() {
  if (auto_reload_timer_ && auto_reload_timer_->IsRunning()) {
    auto_reload_timer_->OnResume();
  }
}

bool AuthenticationFlowAutoReloadManager::IsActiveForTesting() {
  return auto_reload_timer_->IsRunning() && idle_state_observer_.IsObserving();
}

int AuthenticationFlowAutoReloadManager::GetAttemptsCount() const {
  return auto_reload_attempts_;
}

}  // namespace ash
