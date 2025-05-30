// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_browser_sampling_trials.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/persistent_histograms.h"
#include "components/version_info/version_info.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/background_thread_pool_field_trial.h"
#include "base/android/build_info.h"
#include "base/android/bundle_utils.h"
#include "base/task/thread_pool/environment_config.h"
#include "chrome/browser/android/flags/chrome_cached_flags.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/common/chrome_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/channel_info.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/first_run_field_trial.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "base/nix/xdg_util.h"
#include "ui/base/ui_base_features.h"
#endif  // BUILDFLAG(IS_LINUX)

ChromeBrowserFieldTrials::ChromeBrowserFieldTrials(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

ChromeBrowserFieldTrials::~ChromeBrowserFieldTrials() = default;

void ChromeBrowserFieldTrials::SetUpClientSideFieldTrials(
    bool has_seed,
    const variations::EntropyProviders& entropy_providers,
    base::FeatureList* feature_list) {
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

#if BUILDFLAG(IS_CHROMEOS)
  if (!has_seed) {
    ash::multidevice_setup::CreateFirstRunFieldTrial(feature_list);
  }
#endif
}

void ChromeBrowserFieldTrials::RegisterSyntheticTrials() {
#if BUILDFLAG(IS_ANDROID)
  {
    auto trial_info =
        base::android::BackgroundThreadPoolFieldTrial::GetTrialInfo();
    if (trial_info.has_value()) {
      // The annotation mode is set to |kCurrentLog| since the field trial has
      // taken effect at process startup.
      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
          trial_info->trial_name, trial_info->group_name,
          variations::SyntheticTrialAnnotationMode::kCurrentLog);
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_LINUX)
// On Linux/Desktop platform variants, such as ozone/wayland, some features
// might need to be disabled as per OzonePlatform's runtime properties.
// OzonePlatform selection and initialization, in turn, depend on Chrome flags
// processing, namely 'ozone-platform-hint', so do it here.
//
// TODO(nickdiego): Move it back to ChromeMainDelegate::PostEarlyInitialization
// once ozone-platform-hint flag is dropped.
void ChromeBrowserFieldTrials::RegisterFeatureOverrides(
    base::FeatureList* feature_list) {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string xdg_session_type =
      env->GetVar(base::nix::kXdgSessionTypeEnvVar).value_or(std::string());

  if (xdg_session_type == "wayland") {
    feature_list->RegisterExtraFeatureOverrides(
        {{features::kEyeDropper, base::FeatureList::OVERRIDE_DISABLE_FEATURE}});
  }
}
#endif  // BUILDFLAG(IS_LINUX)
