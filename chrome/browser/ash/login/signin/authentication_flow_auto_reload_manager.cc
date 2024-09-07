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

base::Clock* AuthenticationFlowAutoReloadManager::clock_for_testing_ = nullptr;
base::TickClock* AuthenticationFlowAutoReloadManager::tick_clock_for_testing_ =
    nullptr;

AuthenticationFlowAutoReloadManager::AuthenticationFlowAutoReloadManager() {
  local_state_registrar_.Init(g_browser_process->local_state());
  local_state_registrar_.Add(
      prefs::kAuthenticationFlowAutoReloadInterval,
      base::BindRepeating(&AuthenticationFlowAutoReloadManager::OnPolicyUpdated,
                          base::Unretained(this)));
  if (clock_for_testing_ && tick_clock_for_testing_) {
    auto_reload_timer_ = std::make_unique<base::WallClockTimer>(
        clock_for_testing_, tick_clock_for_testing_);
  } else {
    auto_reload_timer_ = std::make_unique<base::WallClockTimer>();
  }
}

AuthenticationFlowAutoReloadManager::~AuthenticationFlowAutoReloadManager() {
  clock_for_testing_ = nullptr;
  tick_clock_for_testing_ = nullptr;
}

void AuthenticationFlowAutoReloadManager::Activate(base::OnceClosure callback) {
  std::optional<base::TimeDelta> reload_interval_ = GetAutoReloadInterval();
  if (!reload_interval_.has_value()) {
    return;
  }

  callback_ = std::move(callback);

  // Start timer for automatic reload of authentication flow
  const base::Time now =
      clock_for_testing_ ? clock_for_testing_->Now() : base::Time::Now();
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
  // TODO(b/352023469): postpone reload if user started typing
  ++auto_reload_attempts_;
  std::move(callback_).Run();
}

void AuthenticationFlowAutoReloadManager::Terminate() {
  auto_reload_timer_->Stop();
  auto_reload_attempts_ = 0;
}

// TODO(b/353937966): Restart timer on policy being updated
void AuthenticationFlowAutoReloadManager::OnPolicyUpdated() {
  std::optional<base::TimeDelta> reload_interval_ = GetAutoReloadInterval();
  if (!reload_interval_.has_value() && auto_reload_timer_->IsRunning()) {
    auto_reload_timer_->Stop();
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
    base::TickClock* tick_clock) {
  clock_for_testing_ = clock;
  tick_clock_for_testing_ = tick_clock;
}

void AuthenticationFlowAutoReloadManager::ResumeTimerForTesting() {
  if (auto_reload_timer_ && auto_reload_timer_->IsRunning()) {
    auto_reload_timer_->OnResume();
  }
}

bool AuthenticationFlowAutoReloadManager::IsTimerActiveForTesting() {
  return auto_reload_timer_->IsRunning();
}

int AuthenticationFlowAutoReloadManager::GetAttemptsCount() const {
  return auto_reload_attempts_;
}

}  // namespace ash
