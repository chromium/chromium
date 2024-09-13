// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/policy/policy_recommendation_restorer.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ash {

namespace {

// The amount of idle time after which recommended values are restored.
constexpr base::TimeDelta kRestoreDelayInMinutes = base::Minutes(1);

}  // namespace

PolicyRecommendationRestorer::PolicyRecommendationRestorer() {
  Shell::Get()->session_controller()->AddObserver(this);
}

PolicyRecommendationRestorer::~PolicyRecommendationRestorer() {
  StopTimer();
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void PolicyRecommendationRestorer::ObservePref(const std::string& pref_name) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetSigninScreenPrefService();
  DCHECK(prefs);
  DCHECK(!base::Contains(pref_names_, pref_name));

  if (!pref_change_registrar_) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(prefs);
  }

  pref_change_registrar_->Add(
      pref_name, base::BindRepeating(&PolicyRecommendationRestorer::Restore,
                                     base::Unretained(this), true));
  pref_names_.insert(pref_name);
  Restore(false /* allow_delay */, pref_name);
}

void PolicyRecommendationRestorer::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_user_pref_connected_ = true;
  StopTimer();
  RestoreAll();
}

void PolicyRecommendationRestorer::OnUserActivity(const ui::Event* event) {
  if (restore_timer_.IsRunning())
    restore_timer_.Reset();
}

void PolicyRecommendationRestorer::DisableForTesting() {
  disabled_for_testing_ = true;
}

void PolicyRecommendationRestorer::Restore(bool allow_delay,
                                           const std::string& pref_name) {
  const PrefService::Preference* pref =
      pref_change_registrar_->prefs()->FindPreference(pref_name);
  CHECK(pref);

  if (!pref->GetRecommendedValue() || !pref->HasUserSetting())
    return;

  if (active_user_pref_connected_) {
    allow_delay = false;
  } else if (allow_delay) {
    // Skip the delay if there has been no user input since |pref_name| is
    // started observing recommended value.
    if (ui::UserActivityDetector::Get()->last_activity_time().is_null()) {
      allow_delay = false;
    }
  }

  if (allow_delay)
    StartTimer();
  else if (!disabled_for_testing_)
    pref_change_registrar_->prefs()->ClearPref(pref->name());
}

void PolicyRecommendationRestorer::RestoreAll() {
  for (const auto& pref_name : pref_names_)
    Restore(false, pref_name);
}

void PolicyRecommendationRestorer::StartTimer() {
  // Listen for user activity so that the timer can be reset while the user is
  // active, causing it to fire only when the user remains idle for
  // |kRestoreDelayInMinutes|.
  ui::UserActivityDetector* user_activity_detector =
      ui::UserActivityDetector::Get();
  if (!user_activity_detector->HasObserver(this)) {
    user_activity_detector->AddObserver(this);
  }

  // There should be a separate timer for each pref. However, in the common
  // case of the user changing settings, a single timer is sufficient. This is
  // because a change initiated by the user implies user activity, so that even
  // if there was a separate timer per pref, they would all be reset at that
  // point, causing them to fire at exactly the same time. In the much rarer
  // case of a recommended value changing, a single timer is a close
  // approximation of the behavior that would be obtained by resetting the timer
  // for the affected pref only.
  restore_timer_.Start(FROM_HERE, kRestoreDelayInMinutes,
                       base::BindOnce(&PolicyRecommendationRestorer::RestoreAll,
                                      base::Unretained(this)));
}

void PolicyRecommendationRestorer::StopTimer() {
  restore_timer_.Stop();
  ui::UserActivityDetector::Get()->RemoveObserver(this);
}

}  // namespace ash
