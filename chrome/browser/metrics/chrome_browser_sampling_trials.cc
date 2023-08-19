// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_browser_sampling_trials.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/common/channel_info.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "components/version_info/channel.h"

namespace metrics {
namespace {

// Note that the trial name must be kept in sync with the server config
// controlling sampling. If they don't match, then clients will be shuffled into
// different groups when the server config takes over from the fallback trial.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
constexpr char kSamplingTrialName[] = "MetricsAndCrashSampling";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_ANDROID)
constexpr char kPostFREFixSamplingTrialName[] =
    "PostFREFixMetricsAndCrashSampling";
#endif  // BUILDFLAG(IS_ANDROID)
constexpr char kUkmSamplingTrialName[] = "UkmSamplingRate";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// Appends a group to the sampling controlling |trial|. The group will be
// associated with a variation param for reporting sampling |rate| in per mille.
void AppendSamplingTrialGroup(const std::string& group_name,
                              int rate,
                              base::FieldTrial* trial) {
  std::map<std::string, std::string> params = {
      {metrics::internal::kRateParamName, base::NumberToString(rate)}};
  base::AssociateFieldTrialParams(trial->trial_name(), group_name, params);
  trial->AppendGroup(group_name, rate);
}

// Unconditionally attempts to create a field trial to control client side
// metrics/crash sampling to use as a fallback when one hasn't been
// provided. This is expected to occur on first-run on platforms that don't
// have first-run variations support. This should only be called when there is
// no existing field trial controlling the sampling feature, and on the
// correct platform. |trial_name| is the name of the trial. |feature_name| is
// the name of the feature that determines sampling. |sampled_in_rate| is the
// sampling rate per mille.
void CreateFallbackSamplingTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    const std::string& trial_name,
    const std::string& feature_name,
    const int sampled_in_rate_per_mille,
    const bool starts_active,
    base::FeatureList* feature_list) {
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          trial_name, /*total_probability=*/1000, "Default", entropy_provider));

  // Like the trial name, the order that these two groups are added to the trial
  // must be kept in sync with the order that they appear in the server config.
  // The desired order is: OutOfReportingSample, InReportingSample.

  const char kSampledOutGroup[] = "OutOfReportingSample";
  const int sampled_out_rate_per_mille = 1000 - sampled_in_rate_per_mille;
  AppendSamplingTrialGroup(kSampledOutGroup, sampled_out_rate_per_mille,
                           trial.get());

  const char kInSampleGroup[] = "InReportingSample";
  AppendSamplingTrialGroup(kInSampleGroup, sampled_in_rate_per_mille,
                           trial.get());

  // Set up the feature. This must be done after all groups are added since
  // GetGroupNameWithoutActivation() will finalize the group choice.
  const std::string& group_name = trial->GetGroupNameWithoutActivation();

  feature_list->RegisterFieldTrialOverride(
      feature_name,
      group_name == kSampledOutGroup
          ? base::FeatureList::OVERRIDE_DISABLE_FEATURE
          : base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial.get());

  if (starts_active) {
    trial->Activate();
  }
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

// Unconditionally attempts to create a field trial to control client side
// UKM sampling to use as a fallback when one hasn't been provided. This is
// expected to occur on first-run on platforms that don't have first-run
// variations support. This should only be called when there is no existing
// field trial controlling the sampling feature.
void CreateFallbackUkmSamplingTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    bool is_stable_channel,
    base::FeatureList* feature_list) {
  static const char kSampledGroup_Stable[] = "Sampled_NoSeed_Stable";
  static const char kSampledGroup_Other[] = "Sampled_NoSeed_Other";
  const char* sampled_group = kSampledGroup_Other;
  int default_sampling = 1;  // Sampling is 1-in-N; this is N.

  // Nothing is sampled out except for "stable" which omits almost everything
  // in this configuration. This is done so that clients that fail to receive
  // a configuration from the server do not bias aggregated results because
  // of a relatively large number of records from them.
  if (is_stable_channel) {
    sampled_group = kSampledGroup_Stable;
    default_sampling = 1000000;
  }

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kUkmSamplingTrialName, /*total_probability=*/100, sampled_group,
          entropy_provider));

  // Everybody (100%) should have a sampling configuration.
  std::map<std::string, std::string> params = {
      {"_default_sampling", base::NumberToString(default_sampling)}};
  base::AssociateFieldTrialParams(trial->trial_name(), sampled_group, params);
  trial->AppendGroup(sampled_group, 100);

  // Setup the feature.
  feature_list->RegisterFieldTrialOverride(
      ukm::kUkmSamplingRateFeature.name,
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
}

}  // namespace

void CreateFallbackSamplingTrialsIfNeeded(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    base::FeatureList* feature_list) {
  [[maybe_unused]] const bool is_stable =
      chrome::GetChannel() == version_info::Channel::STABLE;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  if (!base::FieldTrialList::TrialExists(kSamplingTrialName)) {
    // On all channels except stable, we sample out at a minimal rate to ensure
    // the code paths are exercised in the wild before hitting stable.
    const int kPreStableSampledInRatePerMille = 990;  // 99%

    int kStableSampledInRatePerMille = 100;  // 10%

#if BUILDFLAG(IS_ANDROID)
    // We use 5.3% for this set of users to work around an old bug
    // (crbug/1306481). This should be ~10% in practice.
    kStableSampledInRatePerMille = 53;  // 5.3%

#endif  // BUILDFLAG(IS_ANDROID)

    // Note that the trial has to be activated immediately. Otherwise, it would
    // be possible for this session to crash before its feature was queried, and
    // the independent log produced would not contain the sampling trial.
    CreateFallbackSamplingTrial(
        entropy_provider, kSamplingTrialName,
        metrics::internal::kMetricsReportingFeature.name,
        is_stable ? kStableSampledInRatePerMille
                  : kPreStableSampledInRatePerMille,
        /*starts_active=*/true, feature_list);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_ANDROID)
  if (!base::FieldTrialList::TrialExists(kPostFREFixSamplingTrialName)) {
    // On all channels except stable, we sample out at a minimal rate to ensure
    // the code paths are exercised in the wild before hitting stable.
    const int kPreStableSampledInRatePerMille = 990;  // 99%

    // This is meant to be 10%, and this population, unlike the set of users
    // under the kSamplingTrialName trial should correctly be 10% in practice.
    const int kStableSampledInRatePerMille = 100;  // 10%

    // Note that as per the serverside config, this trial does not start active
    // (so that it is possible to determine from the serverside whether the
    // client used the old or new trial to determine sampling). So if Chrome
    // crashes before its feature is queried, the independent log produced will
    // not contain this trial, even if the client normally uses this trial to
    // determine sampling.
    CreateFallbackSamplingTrial(
        entropy_provider, kPostFREFixSamplingTrialName,
        metrics::internal::kPostFREFixMetricsReportingFeature.name,
        is_stable ? kStableSampledInRatePerMille
                  : kPreStableSampledInRatePerMille,
        /*starts_active=*/false, feature_list);
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void CreateFallbackUkmSamplingTrialIfNeeded(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    base::FeatureList* feature_list) {
  if (!base::FieldTrialList::TrialExists(kUkmSamplingTrialName)) {
    const bool is_stable =
        chrome::GetChannel() == version_info::Channel::STABLE;
    CreateFallbackUkmSamplingTrial(entropy_provider, is_stable, feature_list);
  }
}

}  // namespace metrics
