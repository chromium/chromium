// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/background_actuation_capability.h"

#include <optional>
#include <utility>

#include "base/android/application_status_listener.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "chrome/browser/glic/android/glic_helper_android.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace glic {

namespace {

std::optional<bool> g_can_actuate_in_background_for_testing;

bool QueryCanActuateInBackground() {
  if (g_can_actuate_in_background_for_testing.has_value()) {
    return *g_can_actuate_in_background_for_testing;
  }
  return CanActuateInBackground();
}

}  // namespace

BackgroundActuationCapability::BackgroundActuationCapability(
    PrefService* prefs,
    signin::IdentityManager* identity_manager,
    base::RepeatingClosure on_changed)
    : prefs_(prefs),
      identity_manager_(identity_manager),
      on_changed_(std::move(on_changed)) {
  if (!prefs_ || !identity_manager_) {
    return;
  }
  can_actuate_in_background_ =
      prefs_->GetBoolean(prefs::kGlicLastCanActuateInBackground);
  identity_manager_observation_.Observe(identity_manager_);
  ScheduleRefresh();
  // TODO(b/567791065): Also refresh on notification setting changes while
  // Chrome stays in the background.
  app_status_listener_ =
      base::android::ApplicationStatusListener::New(base::BindRepeating(
          &BackgroundActuationCapability::OnApplicationStateChanged,
          base::Unretained(this)));
}

BackgroundActuationCapability::~BackgroundActuationCapability() = default;

void BackgroundActuationCapability::RefreshForTesting() {
  // Cancels any pending scheduled refresh so it can't satisfy a later wait.
  weak_ptr_factory_.InvalidateWeakPtrs();
  refresh_pending_ = false;
  Refresh();
}

void BackgroundActuationCapability::Refresh() {
  CHECK(prefs_);
  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return;
  }
  const bool can_actuate_in_background = QueryCanActuateInBackground();
  prefs_->SetBoolean(prefs::kGlicLastCanActuateInBackground,
                     can_actuate_in_background);
  if (can_actuate_in_background == can_actuate_in_background_) {
    return;
  }
  can_actuate_in_background_ = can_actuate_in_background;
  on_changed_.Run();
}

void BackgroundActuationCapability::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    ScheduleRefresh();
  }
}

void BackgroundActuationCapability::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
}

// static
void BackgroundActuationCapability::SetCanActuateInBackgroundForTesting(
    std::optional<bool> can) {
  g_can_actuate_in_background_for_testing = can;
}

void BackgroundActuationCapability::OnApplicationStateChanged(
    base::android::ApplicationState state) {
  if (state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
    ScheduleRefresh();
  }
}

void BackgroundActuationCapability::ScheduleRefresh() {
  if (refresh_pending_) {
    return;
  }
  refresh_pending_ = true;
  // BEST_EFFORT UI-thread tasks don't run until browser startup completes,
  // which keeps the JNI and binder calls off startup.
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&BackgroundActuationCapability::OnRefreshTask,
                                weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundActuationCapability::OnRefreshTask() {
  refresh_pending_ = false;
  Refresh();
}

}  // namespace glic
