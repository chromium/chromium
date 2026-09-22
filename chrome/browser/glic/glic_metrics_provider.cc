// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics_provider.h"

#include <algorithm>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_promotion_source_navigation_observer.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/service/glic_onboarding_status.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/synthetic_trials.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace glic {

namespace {

constexpr char kGlicOnboardingStatusSyntheticTrialName[] =
    "GlicOnboardingStatus";

std::string_view GetOnboardingStatusGroupName(OnboardingStatus status) {
  switch (status) {
    case OnboardingStatus::kNoInteraction:
      return "NoInteraction";
    case OnboardingStatus::kOptedInButNotInvoked:
      return "OptedInButNotInvoked";
    case OnboardingStatus::kNotOptedInButInvoked:
      return "NotOptedInButInvoked";
    case OnboardingStatus::kOptedInAndInvoked:
      return "OptedInAndInvoked";
    case OnboardingStatus::kPromptWithNoOptIn:
      return "PromptWithNoOptIn";
    case OnboardingStatus::kPromptAndOptIn:
      return "PromptAndOptIn";
  }
}

}  // namespace

GlicMetricsProvider::GlicMetricsProvider() = default;
GlicMetricsProvider::~GlicMetricsProvider() = default;

// static
void GlicMetricsProvider::RegisterPromotionSourceSyntheticTrial(
    Profile* initializing_profile) {
  std::vector<Profile*> profiles;
  if (g_browser_process && g_browser_process->profile_manager()) {
    profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  }
  if (initializing_profile &&
      !std::ranges::contains(profiles, initializing_profile)) {
    profiles.push_back(initializing_profile);
  }

  std::string reconciled_cohort;
  for (Profile* profile : profiles) {
    if (!profile || !profile->GetPrefs()) {
      continue;
    }
    const std::string cohort =
        profile->GetPrefs()->GetString(prefs::kGlicPromotionSourceCohort);
    if (cohort.empty()) {
      continue;
    }
    if (reconciled_cohort.empty()) {
      reconciled_cohort = cohort;
    } else if (reconciled_cohort != cohort) {
      reconciled_cohort = kGlicPromotionSourceMultiProfileDetected;
      break;
    }
  }

  if (!reconciled_cohort.empty()) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        kGlicPromotionSourceTrialName, reconciled_cohort,
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
  }
}

void GlicMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  RegisterPromotionSourceSyntheticTrial();
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager) {
    return;
  }

  std::vector<Profile*> profile_list = profile_manager->GetLoadedProfiles();
  int tiered_rollout_count = 0;
  int enabled_count = 0;
  OnboardingStatus onboarding_progression = OnboardingStatus::kNoInteraction;
  int invoked_count = 0;
  int opt_in_count = 0;
  int user_submit_count = 0;
  for (auto* profile : profile_list) {
    GlicEnabling::EnablementForProfile(profile).RecordSteadyStateMetrics();

    if (GlicEnabling::HasConsentedForProfile(profile)) {
      base::UmaHistogramSparse(
          "Glic.ZoomLevel.SteadyState",
          profile->GetPrefs()->GetInteger(glic::prefs::kGlicZoomLevel));
    }

    if (GlicEnabling::IsEnabledForProfile(profile)) {
      enabled_count++;
      if (GlicEnabling::IsEligibleForGlicTieredRollout(profile)) {
        tiered_rollout_count++;
      }

      GlicOnboardingStatus onboarding_status(profile->GetPrefs());
      OnboardingStatus status = onboarding_status.GetStatus();

      onboarding_progression = std::max(onboarding_progression, status);
      if (onboarding_status.IsInvoked()) {
        invoked_count++;
      }
      if (onboarding_status.IsOptedIn()) {
        opt_in_count++;
      }
      if (onboarding_status.HasPrompt()) {
        user_submit_count++;
      }
    }
  }

  // No profiles enabled.
  if (enabled_count == 0) {
    return;
  }

  base::UmaHistogramEnumeration("Glic.Onboarding.Profiles.Status",
                                onboarding_progression);
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kGlicOnboardingStatusSyntheticTrialName,
      GetOnboardingStatusGroupName(onboarding_progression),
      variations::SyntheticTrialAnnotationMode::kCurrentLog);

  auto to_none_some_all = [enabled_count](int count) {
    if (count == 0) {
      return GlicProfilesAllSomeNone::kNone;
    }
    if (count == enabled_count) {
      return GlicProfilesAllSomeNone::kAll;
    }
    return GlicProfilesAllSomeNone::kSome;
  };

  base::UmaHistogramEnumeration("Glic.Onboarding.Profiles.Invoked",
                                to_none_some_all(invoked_count));
  base::UmaHistogramEnumeration("Glic.Onboarding.Profiles.OptIn",
                                to_none_some_all(opt_in_count));
  base::UmaHistogramEnumeration("Glic.Onboarding.Profiles.UserSubmit",
                                to_none_some_all(user_submit_count));
  base::UmaHistogramEnumeration("Glic.TieredRolloutEnablementStatus",
                                to_none_some_all(tiered_rollout_count));
}

}  // namespace glic
