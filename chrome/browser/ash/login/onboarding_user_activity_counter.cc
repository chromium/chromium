// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/onboarding_user_activity_counter.h"

#include "base/location.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"

namespace ash {
namespace {

constexpr base::TimeDelta kExpireAfterOnboarding = base::Days(1);

bool IsExpiredAfterOnboarding(PrefService* prefs) {
  return prefs->HasPrefPath(prefs::kOobeOnboardingTime) &&
         base::Time::Now() - prefs->GetTime(prefs::kOobeOnboardingTime) >
             kExpireAfterOnboarding;
}

bool GetActiveState() {
  return !session_manager::SessionManager::Get()->IsUserSessionBlocked();
}

}  // namespace

OnboardingUserActivityCounter::OnboardingUserActivityCounter(
    PrefService* prefs,
    base::TimeDelta activity_time,
    base::OnceClosure closure)
    : OnboardingUserActivityCounter(prefs,
                                    activity_time,
                                    std::move(closure),
                                    base::DefaultTickClock::GetInstance()) {}

OnboardingUserActivityCounter::OnboardingUserActivityCounter(
    PrefService* prefs,
    base::TimeDelta activity_time,
    base::OnceClosure closure,
    const base::TickClock* tick_clock)
    : prefs_(prefs),
      required_activity_time_(activity_time),
      tick_clock_(tick_clock),
      timer_(tick_clock_),
      closure_(std::move(closure)) {
  DCHECK(prefs_);
  DCHECK(ShouldStart(prefs_));
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  // Could be nullptr in tests
  if (primary_profile) {
    DCHECK_EQ(primary_profile->GetPrefs(), prefs);
  }

  auto* session_manager = session_manager::SessionManager::Get();
  // Can't use base::ScopedObservation because SessionManager might be destroyed
  // before OnboardingUserActivityCounter.
  session_manager->AddObserver(this);
  SetActiveState(GetActiveState());
}

void OnboardingUserActivityCounter::SetActiveState(bool active) {
  bool is_currently_active = timer_.IsRunning();
  if (is_currently_active == active)
    return;

  if (timer_.IsRunning()) {
    const base::TimeDelta activity_time_left =
        timer_.desired_run_time() - tick_clock_->NowTicks();
    DCHECK(activity_time_left.is_positive());
    const base::TimeDelta current_activity_time =
        required_activity_time_ - activity_time_left;
    DCHECK(current_activity_time.is_positive());
    prefs_->SetTimeDelta(prefs::kActivityTimeAfterOnboarding,
                         current_activity_time);
    timer_.Stop();
    return;
  }

  if (IsExpiredAfterOnboarding(prefs_)) {
    prefs_->ClearPref(prefs::kActivityTimeAfterOnboarding);
    StopObserving();
    return;
  }

  // Switch to active.
  const base::TimeDelta current_activity_time =
      prefs_->GetTimeDelta(prefs::kActivityTimeAfterOnboarding);
  const base::TimeDelta activity_time_left =
      required_activity_time_ - current_activity_time;
  if (activity_time_left.is_negative()) {
    ReportResult();
    return;
  }
  timer_.Start(FROM_HERE, activity_time_left, this,
               &OnboardingUserActivityCounter::ReportResult);
}

void OnboardingUserActivityCounter::ReportResult() {
  timer_.Stop();
  StopObserving();
  prefs_->ClearPref(prefs::kActivityTimeAfterOnboarding);
  if (IsExpiredAfterOnboarding(prefs_))
    return;
  std::move(closure_).Run();
}

OnboardingUserActivityCounter::~OnboardingUserActivityCounter() {
  SetActiveState(/*active=*/false);
  StopObserving();
}

void OnboardingUserActivityCounter::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimeDeltaPref(prefs::kActivityTimeAfterOnboarding,
                                  base::TimeDelta());
}

// static
void OnboardingUserActivityCounter::MaybeMarkForStart(Profile* profile) {
  // Skip for child and managed users.
  if (profile->IsChild() || profile->GetProfilePolicyConnector()->IsManaged())
    return;

  profile->GetPrefs()->SetTimeDelta(prefs::kActivityTimeAfterOnboarding,
                                    base::TimeDelta());
}

// static
bool OnboardingUserActivityCounter::ShouldStart(PrefService* prefs) {
  if (IsExpiredAfterOnboarding(prefs)) {
    // Skip if the day has passed since user went through Oobe onboarding.
    prefs->ClearPref(prefs::kActivityTimeAfterOnboarding);
    return false;
  }

  return prefs->HasPrefPath(prefs::kActivityTimeAfterOnboarding);
}

void OnboardingUserActivityCounter::StopObserving() {
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager)
    session_manager->RemoveObserver(this);
}

void OnboardingUserActivityCounter::OnSessionStateChanged() {
  TRACE_EVENT0("login", "OnboardingUserActivityCounter::OnSessionStateChanged");
  SetActiveState(GetActiveState());
}

}  // namespace ash
