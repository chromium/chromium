// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/idle_service.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/idle_polling_service.h"

namespace enterprise_idle {

IdleService::IdleService(Profile* profile)
    : profile_(profile),
      action_runner_(
          std::make_unique<ActionRunner>(profile_,
                                         ActionFactory::GetInstance())) {
  DCHECK_EQ(profile_->GetOriginalProfile(), profile_);
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kIdleTimeout,
      base::BindRepeating(&IdleService::OnIdleTimeoutPrefChanged,
                          base::Unretained(this)));
  OnIdleTimeoutPrefChanged();
}

IdleService::~IdleService() = default;

void IdleService::OnIdleTimeoutPrefChanged() {
  base::TimeDelta timeout =
      profile_->GetPrefs()->GetTimeDelta(prefs::kIdleTimeout);
  if (timeout.is_positive()) {
    // `is_idle_` will auto-update in 1 second, no need to set it here.
    idle_threshold_ = timeout;
    if (!polling_service_observation_.IsObserving()) {
      polling_service_observation_.Observe(
          ui::IdlePollingService::GetInstance());
    }
  } else {
    is_idle_ = false;
    idle_threshold_ = base::TimeDelta();
    polling_service_observation_.Reset();
  }
}

void IdleService::OnIdleStateChange(
    const ui::IdlePollingService::State& polled_state) {
  if (is_idle_) {
    if (polled_state.idle_time < idle_threshold_) {
      // Profile just stopped being idle.
      is_idle_ = false;
    }
  } else {
    if (polled_state.idle_time >= idle_threshold_) {
      // Profile just became idle. Run actions.
      is_idle_ = true;
      action_runner_->Run();
    }
  }
}

}  // namespace enterprise_idle
