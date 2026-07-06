// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_onboarding_tracker.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace glic {

GlicOnboardingTracker::GlicOnboardingTracker(Profile* profile,
                                             GlicEnabling* enabling)
    : onboarding_status_(profile ? profile->GetPrefs() : nullptr),
      pref_service_(profile ? profile->GetPrefs() : nullptr),
      enabling_(enabling) {
  CHECK(profile);
  CHECK(pref_service_);

  MigrateInitialOnboardingStatus(profile);

  if (enabling) {
    consent_subscription_ =
        enabling->RegisterOnConsentChanged(base::BindRepeating(
            &GlicOnboardingTracker::OnConsentChanged, base::Unretained(this)));
  }
}

void GlicOnboardingTracker::OnConsentChanged() {
  if (!enabling_ || !enabling_->HasConsented()) {
    return;
  }
  OnboardingStatus current_status = GetStatus();
  if (current_status == OnboardingStatus::kNoInteraction) {
    onboarding_status_.SetStatus(OnboardingStatus::kOptedInButNotInvoked);
  } else if (current_status == OnboardingStatus::kNotOptedInButInvoked) {
    onboarding_status_.SetStatus(OnboardingStatus::kOptedInAndInvoked);
  } else if (current_status == OnboardingStatus::kPromptWithNoOptIn) {
    onboarding_status_.SetStatus(OnboardingStatus::kPromptAndOptIn);
  }
}

GlicOnboardingTracker::~GlicOnboardingTracker() = default;

OnboardingStatus GlicOnboardingTracker::GetStatus() const {
  return onboarding_status_.GetStatus();
}

void GlicOnboardingTracker::MigrateInitialOnboardingStatus(Profile* profile) {
  if (GetStatus() != OnboardingStatus::kNoInteraction) {
    return;
  }
  prefs::FreStatus consent = GlicEnabling::GetCompletedFre(profile);
  if (consent == prefs::FreStatus::kCompleted) {
    onboarding_status_.SetStatus(OnboardingStatus::kOptedInAndInvoked);
  } else if (consent == prefs::FreStatus::kIncomplete) {
    onboarding_status_.SetStatus(OnboardingStatus::kNotOptedInButInvoked);
  }
}

void GlicOnboardingTracker::OnInvoke() {
  pref_service_->SetTime(prefs::kGlicLastInvokedTime, base::Time::Now());
  OnboardingStatus current_status = GetStatus();
  base::RecordAction(base::UserMetricsAction("Glic.Onboarding.Invoked"));
  base::UmaHistogramEnumeration("Glic.Onboarding.Invoked.Status",
                                current_status);
  if (current_status == OnboardingStatus::kNoInteraction) {
    onboarding_status_.SetStatus(OnboardingStatus::kNotOptedInButInvoked);
  } else if (current_status == OnboardingStatus::kOptedInButNotInvoked) {
    onboarding_status_.SetStatus(OnboardingStatus::kOptedInAndInvoked);
  }
}

void GlicOnboardingTracker::OnPrompt() {
  pref_service_->SetTime(prefs::kGlicLastPromptTime, base::Time::Now());
  OnboardingStatus current_status = GetStatus();
  base::RecordAction(
      base::UserMetricsAction("Glic.Onboarding.PromptSubmitted"));
  if (current_status == OnboardingStatus::kNotOptedInButInvoked) {
    onboarding_status_.SetStatus(OnboardingStatus::kPromptWithNoOptIn);
  } else if (current_status == OnboardingStatus::kOptedInAndInvoked) {
    onboarding_status_.SetStatus(OnboardingStatus::kPromptAndOptIn);
  }
}

}  // namespace glic
