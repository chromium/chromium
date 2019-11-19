// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_features.h"

#include "base/metrics/field_trial_params.h"
#include "base/numerics/ranges.h"
#include "chrome/common/chrome_features.h"

namespace {

constexpr char kTabLoadTimeoutInMsParameterName[] = "tabLoadTimeoutInMs";

}  // namespace

namespace features {

// Enables using customized value for tab load timeout. This is used by both
// staggered background tab opening and session restore in finch experiment to
// see what timeout value is better. The default timeout is used when this
// feature is disabled.
const base::Feature kCustomizedTabLoadTimeout{
    "CustomizedTabLoadTimeout", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables proactive tab freezing and discarding.
const base::Feature kProactiveTabFreezeAndDiscard{
    resource_coordinator::kProactiveTabFreezeAndDiscardFeatureName,
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the site characteristics database.
const base::Feature kSiteCharacteristicsDatabase{
    "SiteCharacteristicsDatabase", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables delaying the navigation of background tabs in order to improve
// foreground tab's user experience.
const base::Feature kStaggeredBackgroundTabOpening{
    "StaggeredBackgroundTabOpening", base::FEATURE_DISABLED_BY_DEFAULT};

// This controls whether we are running experiment with staggered background
// tab opening feature. For control group, this should be disabled. This depends
// on |kStaggeredBackgroundTabOpening| above.
const base::Feature kStaggeredBackgroundTabOpeningExperiment{
    "StaggeredBackgroundTabOpeningExperiment",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables using the Tab Ranker to score tabs for discarding instead of relying
// on last focused time.
const base::Feature kTabRanker{"TabRanker", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace resource_coordinator {

namespace {

// Determines the moderate threshold for tab discarding based on system memory,
// and enforces the constraint that it must be in the interval
// [low_loaded_tab_count, high_loaded_tab_count].
int GetModerateThresholdTabCountBasedOnSystemMemory(
    ProactiveTabFreezeAndDiscardParams* params,
    int memory_in_gb) {
  int moderate_loaded_tab_count_per_gb =
      ProactiveTabFreezeAndDiscardParams::kModerateLoadedTabsPerGbRam.Get();

  int moderate_level = moderate_loaded_tab_count_per_gb * memory_in_gb;

  moderate_level =
      base::ClampToRange(moderate_level, params->low_loaded_tab_count,
                         params->high_loaded_tab_count);

  return moderate_level;
}

}  // namespace

const char kProactiveTabFreezeAndDiscardFeatureName[] =
    "ProactiveTabFreezeAndDiscard";
const char kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardParam[] =
    "ShouldProactivelyDiscard";
const char kProactiveTabFreezeAndDiscard_ShouldPeriodicallyUnfreezeParam[] =
    "ShouldPeriodicallyUnfreeze";
const char kProactiveTabFreezeAndDiscard_DisableHeuristicsParam[] =
    "DisableHeuristicsProtections";

// Instantiate the feature parameters for proactive tab discarding.
constexpr base::FeatureParam<bool>
    ProactiveTabFreezeAndDiscardParams::kShouldProactivelyDiscard;
constexpr base::FeatureParam<bool>
    ProactiveTabFreezeAndDiscardParams::kShouldPeriodicallyUnfreeze;
constexpr base::FeatureParam<bool> ProactiveTabFreezeAndDiscardParams::
    kShouldProtectTabsSharingBrowsingInstance;
constexpr base::FeatureParam<int>
    ProactiveTabFreezeAndDiscardParams::kLowLoadedTabCount;
constexpr base::FeatureParam<int>
    ProactiveTabFreezeAndDiscardParams::kModerateLoadedTabsPerGbRam;
constexpr base::FeatureParam<int>
    ProactiveTabFreezeAndDiscardParams::kHighLoadedTabCount;
constexpr base::FeatureParam<int>
    ProactiveTabFreezeAndDiscardParams::kLowOccludedTimeout;
constexpr base::FeatureParam<int>
    ProactiveTabFreezeAndDiscardParams::kModerateOccludedTimeout;
constexpr base::FeatureParam<int>
    ProactiveTabFreezeAndDiscardParams::kHighOccludedTimeout;
constexpr base::FeatureParam<int>
    ProactiveTabFreezeAndDiscardParams::kFreezeTimeout;
constexpr base::FeatureParam<int>
    ProactiveTabFreezeAndDiscardParams::kUnfreezeTimeout;
constexpr base::FeatureParam<int>
    ProactiveTabFreezeAndDiscardParams::kRefreezeTimeout;
constexpr base::FeatureParam<bool>
    ProactiveTabFreezeAndDiscardParams::kDisableHeuristicsProtections;

// Instantiate the feature parameters for the site characteristics database.
constexpr base::FeatureParam<int>
    SiteCharacteristicsDatabaseParams::kFaviconUpdateObservationWindow;
constexpr base::FeatureParam<int>
    SiteCharacteristicsDatabaseParams::kTitleUpdateObservationWindow;
constexpr base::FeatureParam<int>
    SiteCharacteristicsDatabaseParams::kAudioUsageObservationWindow;
constexpr base::FeatureParam<int>
    SiteCharacteristicsDatabaseParams::kNotificationsUsageObservationWindow;
constexpr base::FeatureParam<int>
    SiteCharacteristicsDatabaseParams::kTitleOrFaviconChangePostLoadGracePeriod;
constexpr base::FeatureParam<int>
    SiteCharacteristicsDatabaseParams::kFeatureUsagePostBackgroundGracePeriod;

ProactiveTabFreezeAndDiscardParams::ProactiveTabFreezeAndDiscardParams() =
    default;
ProactiveTabFreezeAndDiscardParams::ProactiveTabFreezeAndDiscardParams(
    const ProactiveTabFreezeAndDiscardParams& rhs) = default;

SiteCharacteristicsDatabaseParams::SiteCharacteristicsDatabaseParams() =
    default;
SiteCharacteristicsDatabaseParams::SiteCharacteristicsDatabaseParams(
    const SiteCharacteristicsDatabaseParams& rhs) = default;

ProactiveTabFreezeAndDiscardParams GetProactiveTabFreezeAndDiscardParams(
    int memory_in_gb) {
  // TimeDelta::Max() should be used to express infinite timeouts. A large
  // timeout that is not TimeDelta::Max() causes MessageLoop to output a
  // warning.
  constexpr base::TimeDelta kLargeTimeout = base::TimeDelta::FromDays(14);

  ProactiveTabFreezeAndDiscardParams params = {};

  params.should_proactively_discard =
      ProactiveTabFreezeAndDiscardParams::kShouldProactivelyDiscard.Get();

  params.should_periodically_unfreeze =
      ProactiveTabFreezeAndDiscardParams::kShouldPeriodicallyUnfreeze.Get();

  params.should_protect_tabs_sharing_browsing_instance =
      ProactiveTabFreezeAndDiscardParams::
          kShouldProtectTabsSharingBrowsingInstance.Get();

  params.low_loaded_tab_count =
      ProactiveTabFreezeAndDiscardParams::kLowLoadedTabCount.Get();

  params.high_loaded_tab_count =
      ProactiveTabFreezeAndDiscardParams::kHighLoadedTabCount.Get();

  // |moderate_loaded_tab_count| determined after |high_loaded_tab_count| so it
  // can be enforced that it is lower than |high_loaded_tab_count|.
  params.moderate_loaded_tab_count =
      GetModerateThresholdTabCountBasedOnSystemMemory(&params, memory_in_gb);

  params.low_occluded_timeout = base::TimeDelta::FromSeconds(
      ProactiveTabFreezeAndDiscardParams::kLowOccludedTimeout.Get());
  DCHECK_LT(params.low_occluded_timeout, kLargeTimeout);

  params.moderate_occluded_timeout = base::TimeDelta::FromSeconds(
      ProactiveTabFreezeAndDiscardParams::kModerateOccludedTimeout.Get());
  DCHECK_LT(params.moderate_occluded_timeout, kLargeTimeout);

  params.high_occluded_timeout = base::TimeDelta::FromSeconds(
      ProactiveTabFreezeAndDiscardParams::kHighOccludedTimeout.Get());
  DCHECK_LT(params.high_occluded_timeout, kLargeTimeout);

  params.freeze_timeout = base::TimeDelta::FromSeconds(
      ProactiveTabFreezeAndDiscardParams::kFreezeTimeout.Get());
  DCHECK_LT(params.freeze_timeout, kLargeTimeout);

  params.unfreeze_timeout = base::TimeDelta::FromSeconds(
      ProactiveTabFreezeAndDiscardParams::kUnfreezeTimeout.Get());
  DCHECK_LT(params.unfreeze_timeout, kLargeTimeout);

  params.refreeze_timeout = base::TimeDelta::FromSeconds(
      ProactiveTabFreezeAndDiscardParams::kRefreezeTimeout.Get());
  DCHECK_LT(params.refreeze_timeout, kLargeTimeout);

  params.disable_heuristics_protections =
      ProactiveTabFreezeAndDiscardParams::kDisableHeuristicsProtections.Get();

  return params;
}

const ProactiveTabFreezeAndDiscardParams&
GetStaticProactiveTabFreezeAndDiscardParams() {
  static base::NoDestructor<ProactiveTabFreezeAndDiscardParams> params(
      GetProactiveTabFreezeAndDiscardParams());
  return *params;
}

ProactiveTabFreezeAndDiscardParams*
GetMutableStaticProactiveTabFreezeAndDiscardParamsForTesting() {
  return const_cast<ProactiveTabFreezeAndDiscardParams*>(
      &GetStaticProactiveTabFreezeAndDiscardParams());
}

base::TimeDelta GetTabLoadTimeout(const base::TimeDelta& default_timeout) {
  int timeout_in_ms = base::GetFieldTrialParamByFeatureAsInt(
      features::kCustomizedTabLoadTimeout, kTabLoadTimeoutInMsParameterName,
      default_timeout.InMilliseconds());

  if (timeout_in_ms <= 0)
    return default_timeout;

  return base::TimeDelta::FromMilliseconds(timeout_in_ms);
}

SiteCharacteristicsDatabaseParams GetSiteCharacteristicsDatabaseParams() {
  SiteCharacteristicsDatabaseParams params = {};

  params.favicon_update_observation_window = base::TimeDelta::FromSeconds(
      SiteCharacteristicsDatabaseParams::kFaviconUpdateObservationWindow.Get());

  params.title_update_observation_window = base::TimeDelta::FromSeconds(
      SiteCharacteristicsDatabaseParams::kTitleUpdateObservationWindow.Get());

  params.audio_usage_observation_window = base::TimeDelta::FromSeconds(
      SiteCharacteristicsDatabaseParams::kAudioUsageObservationWindow.Get());

  params.notifications_usage_observation_window = base::TimeDelta::FromSeconds(
      SiteCharacteristicsDatabaseParams::kNotificationsUsageObservationWindow
          .Get());

  params.title_or_favicon_change_post_load_grace_period =
      base::TimeDelta::FromSeconds(
          SiteCharacteristicsDatabaseParams::
              kTitleOrFaviconChangePostLoadGracePeriod.Get());

  params.feature_usage_post_background_grace_period =
      base::TimeDelta::FromSeconds(
          SiteCharacteristicsDatabaseParams::
              kFeatureUsagePostBackgroundGracePeriod.Get());

  return params;
}

const SiteCharacteristicsDatabaseParams&
GetStaticSiteCharacteristicsDatabaseParams() {
  static base::NoDestructor<SiteCharacteristicsDatabaseParams> params(
      GetSiteCharacteristicsDatabaseParams());
  return *params;
}

int GetNumOldestTabsToScoreWithTabRanker() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kTabRanker, "number_of_oldest_tabs_to_score_with_TabRanker", 0);
}

int GetProcessTypeToScoreWithTabRanker() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kTabRanker, "process_type_of_tabs_to_score_with_TabRanker", 4);
}

int GetNumOldestTabsToLogWithTabRanker() {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kTabRanker, "number_of_oldest_tabs_to_log_with_TabRanker", 0);
}

bool DisableBackgroundLogWithTabRanker() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kTabRanker, "disable_background_log_with_TabRanker", true);
}

float GetDiscardCountPenaltyTabRanker() {
  return static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
      features::kTabRanker, "discard_count_penalty", 0.0));
}

float GetMRUScorerPenaltyTabRanker() {
  return static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
      features::kTabRanker, "mru_scorer_penalty", 1.0));
}

int GetScorerTypeForTabRanker() {
  return base::GetFieldTrialParamByFeatureAsInt(features::kTabRanker,
                                                "scorer_type", 1);
}

}  // namespace resource_coordinator
