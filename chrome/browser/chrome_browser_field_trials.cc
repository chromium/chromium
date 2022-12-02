// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/chrome_browser_sampling_trials.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/persistent_histograms.h"
#include "components/version_info/version_info.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "base/android/bundle_utils.h"
#include "base/task/thread_pool/environment_config.h"
#include "chrome/browser/android/signin/fre_mobile_identity_consistency_field_trial.h"
#include "chrome/browser/flags/android/cached_feature_flags.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/common/chrome_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/consolidated_consent_field_trial.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/first_run_field_trial.h"
#endif

ChromeBrowserFieldTrials::ChromeBrowserFieldTrials(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

ChromeBrowserFieldTrials::~ChromeBrowserFieldTrials() {
}

void ChromeBrowserFieldTrials::OnVariationsSetupComplete() {
  // Persistent histograms must be enabled ASAP, but depends on Features.
  base::FilePath metrics_dir;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &metrics_dir)) {
    InstantiatePersistentHistograms(metrics_dir);
  }
}

void ChromeBrowserFieldTrials::SetUpClientSideFieldTrials(
    bool has_seed,
    const variations::EntropyProviders& entropy_providers,
    base::FeatureList* feature_list) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::consolidated_consent_field_trial::Create(
      entropy_providers.default_entropy(), feature_list, local_state_);
#endif

  // Only create the fallback trials if there isn't already a variations seed
  // being applied. This should occur during first run when first-run variations
  // isn't supported. It's assumed that, if there is a seed, then it either
  // contains the relevant studies, or is intentionally omitted, so no fallback
  // is needed. The exception is for sampling trials. Fallback trials are
  // created even if no variations seed was applied. This allows testing the
  // fallback code by intentionally omitting the sampling trial from a
  // variations seed.
  metrics::CreateFallbackSamplingTrialsIfNeeded(
      entropy_providers.default_entropy(), feature_list);
  metrics::CreateFallbackUkmSamplingTrialIfNeeded(
      entropy_providers.default_entropy(), feature_list);
  if (!has_seed) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::multidevice_setup::CreateFirstRunFieldTrial(feature_list);
#endif
  }

#if BUILDFLAG(IS_ANDROID)
  // RegisterSyntheticTrials doesn't have access to entropy providers which are
  // needed to verify group consistency for
  // FREMobileIdentityConsistencySynthetic and decide whether to assign
  // a variation ID to that study. To work around that - grab the variation ID
  // here and perform the actual registration in RegisterSyntheticTrials().
  fre_consistency_trial_variation_id_ =
      fre_mobile_identity_consistency_field_trial::GetFREFieldTrialVariationId(
          entropy_providers.low_entropy_value(),
          entropy_providers.low_entropy_domain());
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeBrowserFieldTrials::RegisterSyntheticTrials() {
#if BUILDFLAG(IS_ANDROID)
  static constexpr char kReachedCodeProfilerTrial[] =
      "ReachedCodeProfilerSynthetic2";
  std::string reached_code_profiler_group =
      chrome::android::GetReachedCodeProfilerTrialGroup();
  if (!reached_code_profiler_group.empty()) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        kReachedCodeProfilerTrial, reached_code_profiler_group);
  }

  {
    // BackgroundThreadPoolSynthetic field trial.
    const char* group_name;
    // Target group as indicated by finch feature.
    bool feature_enabled =
        base::FeatureList::IsEnabled(chrome::android::kBackgroundThreadPool);
    // Whether the feature was overridden by either the commandline or Finch.
    bool feature_overridden =
        base::FeatureList::GetInstance()->IsFeatureOverridden(
            chrome::android::kBackgroundThreadPool.name);
    // Whether the feature was overridden manually via the commandline.
    bool cmdline_overridden =
        feature_overridden &&
        base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
            chrome::android::kBackgroundThreadPool.name);
    // The finch feature value is cached by Java in a setting and applied via a
    // command line flag. Check if this has happened -- it may not have happened
    // if this is the first startup after the feature is enabled.
    bool actually_enabled =
        base::internal::CanUseBackgroundThreadTypeForWorkerThread();
    // Use the default group if either the feature wasn't overridden or if the
    // feature target state and actual state don't agree. Also separate users
    // that override the feature via the commandline into separate groups.
    if (actually_enabled != feature_enabled || !feature_overridden) {
      group_name = "Default";
    } else if (cmdline_overridden && feature_enabled) {
      group_name = "ForceEnabled";
    } else if (cmdline_overridden && !feature_enabled) {
      group_name = "ForceDisabled";
    } else if (feature_enabled) {
      group_name = "Enabled";
    } else {
      group_name = "Disabled";
    }
    static constexpr char kBackgroundThreadPoolTrial[] =
        "BackgroundThreadPoolSynthetic";
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        kBackgroundThreadPoolTrial, group_name);
  }

  // MobileIdentityConsistencyFRESynthetic field trial.
  static constexpr char kFREMobileIdentityConsistencyTrial[] =
      "FREMobileIdentityConsistencySynthetic";
  const std::string group =
      fre_mobile_identity_consistency_field_trial::GetFREFieldTrialGroup();
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kFREMobileIdentityConsistencyTrial, group,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
  if (fre_consistency_trial_variation_id_ != variations::EMPTY_ID) {
    variations::AssociateGoogleVariationID(
        variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
        kFREMobileIdentityConsistencyTrial, group,
        fre_consistency_trial_variation_id_);
  }
#endif  // BUILDFLAG(IS_ANDROID)
}
