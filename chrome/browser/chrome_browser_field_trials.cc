// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/persistent_histograms.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "components/version_info/version_info.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "base/android/bundle_utils.h"
#include "base/task/thread_pool/environment_config.h"
#include "chrome/browser/chrome_browser_field_trials_mobile.h"
#include "chrome/browser/flags/android/cached_feature_flags.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/common/chrome_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/sync/split_settings_sync_field_trial.h"
#include "chromeos/services/multidevice_setup/public/cpp/first_run_field_trial.h"
#endif

namespace {

// Create a field trial to control metrics/crash sampling for Stable on
// Windows/Android if no variations seed was applied.
void CreateFallbackSamplingTrialIfNeeded(base::FeatureList* feature_list) {
#if defined(OS_WIN) || defined(OS_ANDROID)
  ChromeMetricsServicesManagerClient::CreateFallbackSamplingTrial(
      chrome::GetChannel(), feature_list);
#endif  // defined(OS_WIN) || defined(OS_ANDROID)
}

// Create a field trial to control UKM sampling for Stable if no variations
// seed was applied.
void CreateFallbackUkmSamplingTrialIfNeeded(base::FeatureList* feature_list) {
  ukm::UkmRecorderImpl::CreateFallbackSamplingTrial(
      chrome::GetChannel() == version_info::Channel::STABLE, feature_list);
}

}  // namespace

ChromeBrowserFieldTrials::ChromeBrowserFieldTrials(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

ChromeBrowserFieldTrials::~ChromeBrowserFieldTrials() {
}

void ChromeBrowserFieldTrials::SetupFieldTrials() {
  // Field trials that are shared by all platforms.
  InstantiateDynamicTrials();

#if defined(OS_ANDROID)
  chrome::SetupMobileFieldTrials();
#endif
}

void ChromeBrowserFieldTrials::SetupFeatureControllingFieldTrials(
    bool has_seed,
    const base::FieldTrial::EntropyProvider* low_entropy_provider,
    base::FeatureList* feature_list) {
  // Only create the fallback trials if there isn't already a variations seed
  // being applied. This should occur during first run when first-run variations
  // isn't supported. It's assumed that, if there is a seed, then it either
  // contains the relavent studies, or is intentionally omitted, so no fallback
  // is needed.
  if (!has_seed) {
    CreateFallbackSamplingTrialIfNeeded(feature_list);
    CreateFallbackUkmSamplingTrialIfNeeded(feature_list);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::multidevice_setup::CreateFirstRunFieldTrial(feature_list);
#endif
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This trial is fully client controlled and must be configured whether or
  // not a seed is available.
  split_settings_sync_field_trial::Create(feature_list, local_state_);
#endif
}

void ChromeBrowserFieldTrials::RegisterSyntheticTrials() {
#if defined(OS_ANDROID)
  static constexpr char kReachedCodeProfilerTrial[] =
      "ReachedCodeProfilerSynthetic2";
  std::string reached_code_profiler_group =
      chrome::android::GetReachedCodeProfilerTrialGroup();
  if (!reached_code_profiler_group.empty()) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        kReachedCodeProfilerTrial, reached_code_profiler_group);
  }

  {
    // EarlyLibraryLoadSynthetic field trial.
    const char* group_name;
    bool java_feature_enabled = chrome::android::IsJavaDrivenFeatureEnabled(
        features::kEarlyLibraryLoad);
    bool feature_enabled =
        base::FeatureList::IsEnabled(features::kEarlyLibraryLoad);
    // Use the default group if cc and java feature values don't agree (can
    // happen on first startup after feature is enabled by Finch), or the
    // feature is not overridden by Finch.
    if (feature_enabled != java_feature_enabled ||
        !base::FeatureList::GetInstance()->IsFeatureOverridden(
            features::kEarlyLibraryLoad.name)) {
      group_name = "Default";
    } else if (java_feature_enabled) {
      group_name = "Enabled";
    } else {
      group_name = "Disabled";
    }
    static constexpr char kEarlyLibraryLoadTrial[] =
        "EarlyLibraryLoadSynthetic";
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        kEarlyLibraryLoadTrial, group_name);
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
        base::internal::CanUseBackgroundPriorityForWorkerThread();
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
#endif  // defined(OS_ANDROID)
}

void ChromeBrowserFieldTrials::InstantiateDynamicTrials() {
  // Persistent histograms must be enabled as soon as possible.
  base::FilePath metrics_dir;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &metrics_dir)) {
    InstantiatePersistentHistograms(metrics_dir);
  }
}
