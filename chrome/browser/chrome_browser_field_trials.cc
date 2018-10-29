// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/metrics/persistent_histograms.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "components/version_info/version_info.h"

#if defined(OS_ANDROID)
#include "chrome/browser/chrome_browser_field_trials_mobile.h"
#else
#include "chrome/browser/chrome_browser_field_trials_desktop.h"
#endif

#if defined(OS_CHROMEOS)
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

ChromeBrowserFieldTrials::ChromeBrowserFieldTrials() {}

ChromeBrowserFieldTrials::~ChromeBrowserFieldTrials() {
}

void ChromeBrowserFieldTrials::SetupFieldTrials() {
  // Field trials that are shared by all platforms.
  InstantiateDynamicTrials();

#if defined(OS_ANDROID)
  chrome::SetupMobileFieldTrials();
#else
  chrome::SetupDesktopFieldTrials();
#endif
}

void ChromeBrowserFieldTrials::SetupFeatureControllingFieldTrials(
    bool has_seed,
    base::FeatureList* feature_list) {
  // Only create the fallback trials if there isn't already a variations seed
  // being applied. This should occur during first run when first-run variations
  // isn't supported. It's assumed that, if there is a seed, then it either
  // contains the relavent studies, or is intentionally omitted, so no fallback
  // is needed.
  if (!has_seed) {
    CreateFallbackSamplingTrialIfNeeded(feature_list);
    CreateFallbackUkmSamplingTrialIfNeeded(feature_list);
#if defined(OS_CHROMEOS)
    chromeos::multidevice_setup::CreateFirstRunFieldTrial(feature_list);
#endif
  }
}

void ChromeBrowserFieldTrials::InstantiateDynamicTrials() {
  // Persistent histograms must be enabled as soon as possible.
  InstantiatePersistentHistograms();
}
