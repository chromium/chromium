// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/idle_service.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/enterprise/idle/browser_closer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/idle_polling_service.h"

namespace enterprise_idle {

IdleService::IdleService(Profile* profile) : profile_(profile) {
  DCHECK_EQ(profile_->GetOriginalProfile(), profile_);
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kIdleProfileCloseTimeout,
      base::BindRepeating(&IdleService::OnIdleProfileCloseTimeoutPrefChanged,
                          base::Unretained(this)));
  OnIdleProfileCloseTimeoutPrefChanged();
}

IdleService::~IdleService() = default;

void IdleService::OnIdleProfileCloseTimeoutPrefChanged() {
  int minutes =
      profile_->GetPrefs()->GetInteger(prefs::kIdleProfileCloseTimeout);
  if (minutes > 0) {
    // TODO(crbug.com/1316551): Validate the policy value (e.g. clamp to a
    // minimum) in a PolicyHandler, instead of here.
    minutes = std::max(5, minutes);
    // `is_idle_` will auto-update in 1 second, no need to set it here.
    idle_threshold_ = base::Minutes(minutes);
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
      // Profile just became idle. Show the dialog.
      is_idle_ = true;
      browser_close_subscription_ =
          BrowserCloser::GetInstance()->ShowDialogAndCloseBrowsers(
              profile_, idle_threshold_,
              base::BindOnce(&IdleService::OnCloseFinished,
                             weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void IdleService::OnCloseFinished(BrowserCloser::CloseResult result) {
  switch (result) {
    case BrowserCloser::CloseResult::kSuccess:
      // Technically, this shows the ProfilePicker once per profile. However,
      // all IdleServices run OnCloseFinished() in succession (once they're
      // *all* closed), and there's only one ProfilePicker.
      //
      // Calling ProfilePicker::Show() multiple times like
      // this is a no-op, so we don't need to bother de-duping work.
      ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
          ProfilePicker::EntryPoint::kProfileIdle));
      break;

    case BrowserCloser::CloseResult::kAborted:
    case BrowserCloser::CloseResult::kSkip:
      break;

    default:
      NOTREACHED();
  }
}

}  // namespace enterprise_idle
