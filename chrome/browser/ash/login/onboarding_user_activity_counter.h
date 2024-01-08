// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ONBOARDING_USER_ACTIVITY_COUNTER_H_
#define CHROME_BROWSER_ASH_LOGIN_ONBOARDING_USER_ACTIVITY_COUNTER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace base {
class TickClock;
}  // namespace base

namespace ash {

// Helper class to track user activity time after user went through onboarding.
// Uses `kActivityTimeAfterOnboarding` pref to store the activity time. So it's
// persisted on Chrome restart. If more than a day passes since the onboarding -
// the class resets the whole state and does not run `closure`.
// Passed `prefs` must correspond to the passed `profile`.
class OnboardingUserActivityCounter
    : public session_manager::SessionManagerObserver {
 public:
  OnboardingUserActivityCounter(PrefService* prefs,
                                base::TimeDelta activity_time,
                                base::OnceClosure closure);

  OnboardingUserActivityCounter(PrefService* prefs,
                                base::TimeDelta activity_time,
                                base::OnceClosure closure,
                                const base::TickClock* tick_clock);
  OnboardingUserActivityCounter(const OnboardingUserActivityCounter&) = delete;
  OnboardingUserActivityCounter& operator=(
      const OnboardingUserActivityCounter&) = delete;

  ~OnboardingUserActivityCounter() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void MaybeMarkForStart(Profile* profile);
  static bool ShouldStart(PrefService* prefs);

 private:
  void SetActiveState(bool active);
  void ReportResult();
  void StopObserving();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  const raw_ptr<PrefService, DanglingUntriaged> prefs_;
  const base::TimeDelta required_activity_time_;

  raw_ptr<const base::TickClock> tick_clock_;
  base::OneShotTimer timer_;
  base::OnceClosure closure_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ONBOARDING_USER_ACTIVITY_COUNTER_H_
