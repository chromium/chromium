// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"

namespace features {

extern const base::Feature kCustomizedTabLoadTimeout;
extern const base::Feature kProactiveTabFreezeAndDiscard;
extern const base::Feature kSiteCharacteristicsDatabase;
extern const base::Feature kStaggeredBackgroundTabOpening;
extern const base::Feature kStaggeredBackgroundTabOpeningExperiment;
extern const base::Feature kTabRanker;

}  // namespace features

namespace resource_coordinator {

// The name of the ProactiveTabFreezeAndDiscard feature.
extern const char kProactiveTabFreezeAndDiscardFeatureName[];

// The name of the |ShouldProactivelyDiscard| parameter of the
// ProactiveTabFreezeAndDiscard feature.
extern const char kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardParam[];

// The name of the |ShouldPeriodicallyUnfreeze| parameter of the
// ProactiveTabFreezeAndDiscard feature.
extern const char
    kProactiveTabFreezeAndDiscard_ShouldPeriodicallyUnfreezeParam[];

// The name of the |DisableHeuristicsProtections| parameter of the
// ProactiveTabFreezeAndDiscard feature.
extern const char kProactiveTabFreezeAndDiscard_DisableHeuristicsParam[];

// Parameters used by the proactive tab discarding feature.
//
// Proactive discarding has 5 key parameters:
//
// - min/max occluded timeouts
// - min/soft_max/hard_max loaded tab counts
//
// Proactive tab discarding decisions are made at two points in time:
//
// - when a new tab is created
// - when a max occluded timeout fires
//
// The following is a description of the initial simple proactive discarding
// logic. First, the number of loaded tabs is converted into one of 4 tab count
// states (LOW, MODERATE, HIGH, EXCESSIVE) using 3 simple thresholds.
//
// +-------+----------+---------+-----------+
// +  LOW  | MODERATE |  HIGH   | EXCESSIVE |
// +-------+----------+---------+-----------+
// 0      n_low      n_mod     n_high      +inf
//
// Depending on the tab count state, tabs are eligible for proactive discarding
// at different time tresholds, where the timeout is longer for lower tab
// count states. When in the low state the timeout is effectively infinite (no
// proactive discarding will occur), and when in the excessive state the timeout
// is zero (discarding will occur immediately).
//
// This logic is independent of urgent discarding, which may embark when things
// are sufficiently bad. Similarly, manual or extension driven discards can
// override this logic. Finally, proactive discarding can only discard occluded
// tabs, so it is always possible to have arbitrarily many visible tabs.
//
// NOTE: This is extremely simplistic, and by design. We will be using this to
// do a very simple "lightspeed" experiment to determine how much possible
// savings proactive discarding can hope to achieve.
struct ProactiveTabFreezeAndDiscardParams {
  ProactiveTabFreezeAndDiscardParams();
  ProactiveTabFreezeAndDiscardParams(
      const ProactiveTabFreezeAndDiscardParams& rhs);

  // Static definition of the different parameters that can be used by this
  // feature.

  static constexpr base::FeatureParam<bool> kShouldProactivelyDiscard{
      &features::kProactiveTabFreezeAndDiscard,
      kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardParam, false};
  static constexpr base::FeatureParam<bool> kShouldPeriodicallyUnfreeze{
      &features::kProactiveTabFreezeAndDiscard,
      kProactiveTabFreezeAndDiscard_ShouldPeriodicallyUnfreezeParam, false};
  static constexpr base::FeatureParam<bool>
      kShouldProtectTabsSharingBrowsingInstance{
          &features::kProactiveTabFreezeAndDiscard,
          "ShouldProtectTabsSharingBrowsingInstance", true};
  // 50% of people cap out at 4 tabs, so for them proactive discarding won't
  // even be invoked. See Tabs.MaxTabsInADay.
  // TODO(chrisha): This should eventually be informed by the number of tabs
  // typically used over a given time horizon (metric being developed).
  static constexpr base::FeatureParam<int> kLowLoadedTabCount{
      &features::kProactiveTabFreezeAndDiscard, "LowLoadedTabCount", 4};
  // Testing in the lab shows that 2GB devices suffer beyond 6 tabs, and 4GB
  // devices suffer beyond about 12 tabs. As a very simple first step, we'll aim
  // at allowing 3 tabs per GB of RAM on a system before proactive discarding
  // kicks in. This is a system resource dependent max, which is combined with
  // the DefaultMaxLoadedTabCount to determine the max on a system.
  static constexpr base::FeatureParam<int> kModerateLoadedTabsPerGbRam{
      &features::kProactiveTabFreezeAndDiscard, "ModerateLoadedTabsPerGbRam",
      3};
  // 99.9% of people cap out with fewer than this number, so only 0.1% of the
  // population should ever encounter proactive discarding based on this cap.
  static constexpr base::FeatureParam<int> kHighLoadedTabCount{
      &features::kProactiveTabFreezeAndDiscard, "HighLoadedTabCount", 100};
  // Current discarding uses 10 minutes as a minimum cap. This uses
  // exponentially increasing timeouts beyond that.
  static constexpr base::FeatureParam<int> kLowOccludedTimeout{
      &features::kProactiveTabFreezeAndDiscard, "LowOccludedTimeoutSeconds",
      6 * base::Time::kSecondsPerHour};
  static constexpr base::FeatureParam<int> kModerateOccludedTimeout{
      &features::kProactiveTabFreezeAndDiscard,
      "ModerateOccludedTimeoutSeconds", 1 * base::Time::kSecondsPerHour};
  static constexpr base::FeatureParam<int> kHighOccludedTimeout{
      &features::kProactiveTabFreezeAndDiscard, "HighOccludedTimeoutSeconds",
      10 * base::Time::kSecondsPerMinute};
  static constexpr base::FeatureParam<int> kFreezeTimeout{
      &features::kProactiveTabFreezeAndDiscard, "FreezeTimeout",
      5 * base::Time::kSecondsPerMinute};
  static constexpr base::FeatureParam<int> kUnfreezeTimeout{
      &features::kProactiveTabFreezeAndDiscard, "UnfreezeTimeout",
      15 * base::Time::kSecondsPerMinute};
  static constexpr base::FeatureParam<int> kRefreezeTimeout{
      &features::kProactiveTabFreezeAndDiscard, "RefreezeTimeout", 10};

  static constexpr base::FeatureParam<bool> kDisableHeuristicsProtections{
      &features::kProactiveTabFreezeAndDiscard,
      kProactiveTabFreezeAndDiscard_DisableHeuristicsParam, false};

  // Whether tabs should be proactively discarded. When the
  // |kProactiveTabFreezeAndDiscard| feature is enabled and this is false, only
  // proactive tab freezing happens.
  bool should_proactively_discard;
  // Whether frozen tabs should periodically be unfrozen to update their state.
  bool should_periodically_unfreeze;
  // Whether tabs should be protected from freezing/discarding if they share
  // their BrowsingInstance with another tab.
  bool should_protect_tabs_sharing_browsing_instance;
  // Tab count (inclusive) beyond which the state transitions to MODERATE.
  // Intended to cover the majority of simple workflows and be small enough that
  // it is very unlikely that memory pressure will be encountered with this many
  // tabs loaded.
  int low_loaded_tab_count;
  // Tab count (inclusive) beyond which the state transitions to HIGH. This
  // value is determined based on the available system memory, and is ensured to
  // be in the interval [low_loaded_tab_count, high_loaded_tab_count].
  int moderate_loaded_tab_count;
  // Tab count (inclusive) beyond which the state transitions to EXCESSIVE.
  // Not relative to system memory, as its intended to be a hard cap
  // more akin to a maximum mental model size.
  int high_loaded_tab_count;
  // Amount of time a tab must be occluded before eligible for proactive
  // discard when the tab count state is LOW.
  base::TimeDelta low_occluded_timeout;
  // Amount of time a tab must be occluded before eligible for proactive
  // discard when the tab count state is MODERATE.
  base::TimeDelta moderate_occluded_timeout;
  // Amount of time a tab must be occluded before eligible for proactive
  // discard when the tab count state is HIGH.
  base::TimeDelta high_occluded_timeout;
  // Amount of time a tab must be occluded before it is frozen.
  base::TimeDelta freeze_timeout;
  // Amount of time a tab must be unfrozen before it is temporarily unfrozen.
  base::TimeDelta unfreeze_timeout;
  // Amount of time that a tab stays unfrozen before being frozen again.
  base::TimeDelta refreeze_timeout;
  // Disable all the heuristics protections when doing a freezing or discarding
  // intervention.
  bool disable_heuristics_protections;
};

// Parameters used by the site characteristics database.
//
// The site characteristics database tracks tab usage of a some features, a tab,
// a feature is considered as unused if it hasn't been used for a sufficiently
// long period of time while the tab was backgrounded. There's currently 4
// features we're interested in:
//
// - Favicon update
// - Title update
// - Audio usage
// - Notifications usage
struct SiteCharacteristicsDatabaseParams {
  SiteCharacteristicsDatabaseParams();
  SiteCharacteristicsDatabaseParams(
      const SiteCharacteristicsDatabaseParams& rhs);

  // Static definition of the different parameters that can be used by this
  // feature.

  // Observations windows have a default value of 2 hours, 95% of backgrounded
  // tabs don't use any of these features in this time window.
  static constexpr base::FeatureParam<int> kFaviconUpdateObservationWindow{
      &features::kSiteCharacteristicsDatabase, "FaviconUpdateObservationWindow",
      2 * base::Time::kSecondsPerHour};
  static constexpr base::FeatureParam<int> kTitleUpdateObservationWindow{
      &features::kSiteCharacteristicsDatabase, "TitleUpdateObservationWindow",
      2 * base::Time::kSecondsPerHour};
  static constexpr base::FeatureParam<int> kAudioUsageObservationWindow{
      &features::kSiteCharacteristicsDatabase, "AudioUsageObservationWindow",
      2 * base::Time::kSecondsPerHour};
  static constexpr base::FeatureParam<int> kNotificationsUsageObservationWindow{
      &features::kSiteCharacteristicsDatabase,
      "NotificationsUsageObservationWindow", 2 * base::Time::kSecondsPerHour};
  static constexpr base::FeatureParam<int>
      kTitleOrFaviconChangePostLoadGracePeriod{
          &features::kSiteCharacteristicsDatabase,
          "TitleOrFaviconChangePostLoadGracePeriod", 20 /* 20 seconds */};
  static constexpr base::FeatureParam<int>
      kFeatureUsagePostBackgroundGracePeriod{
          &features::kSiteCharacteristicsDatabase,
          "FeatureUsagePostBackgroundGracePeriod", 10 /* 10 seconds */};

  // Minimum observation window before considering that this website doesn't
  // update its favicon while in background.
  base::TimeDelta favicon_update_observation_window;
  // Minimum observation window before considering that this website doesn't
  // update its title while in background.
  base::TimeDelta title_update_observation_window;
  // Minimum observation window before considering that this website doesn't
  // use audio while in background.
  base::TimeDelta audio_usage_observation_window;
  // Minimum observation window before considering that this website doesn't
  // use notifications while in background.
  base::TimeDelta notifications_usage_observation_window;
  // The period of time after loading during which we ignore title/favicon
  // change events. It's possible for some site that are loaded in background to
  // use some of these features without this being an attempt to communicate
  // with the user (e.g. the tab is just really finishing to load).
  base::TimeDelta title_or_favicon_change_post_load_grace_period;
  // The period of time during which we ignore events after a tab gets
  // backgrounded. It's necessary because some events might happen shortly after
  // backgrounding a tab without this being an attempt to communicate with the
  // user:
  //    - There might be a delay between a media request gets initiated and the
  //      time the audio actually starts.
  //    - Same-document navigation can cause the title or favicon to change, if
  //      the user switch tab before this completes this will be recorded as a
  //      background communication event while in reality it's just a navigation
  //      event.
  base::TimeDelta feature_usage_post_background_grace_period;
};

// Gets parameters for the proactive tab discarding feature. This does no
// parameter validation, and sets the default values if the feature is not
// enabled.
ProactiveTabFreezeAndDiscardParams GetProactiveTabFreezeAndDiscardParams(
    int memory_in_gb = base::SysInfo::AmountOfPhysicalMemory() /
                       (1024 * 1024 * 1024));

// Return a static ProactiveTabFreezeAndDiscardParams object that can be used by
// all the classes that need one.
const ProactiveTabFreezeAndDiscardParams&
GetStaticProactiveTabFreezeAndDiscardParams();
ProactiveTabFreezeAndDiscardParams*
GetMutableStaticProactiveTabFreezeAndDiscardParamsForTesting();

base::TimeDelta GetTabLoadTimeout(const base::TimeDelta& default_timeout);

// Gets parameters for the site characteristics database feature. This does no
// parameter validation, and sets the default values if the feature is not
// enabled.
SiteCharacteristicsDatabaseParams GetSiteCharacteristicsDatabaseParams();

// Return a static SiteCharacteristicsDatabaseParams object that can be used by
// all the classes that need one.
const SiteCharacteristicsDatabaseParams&
GetStaticSiteCharacteristicsDatabaseParams();

// Gets number of oldest tab that should be scored by TabRanker.
int GetNumOldestTabsToScoreWithTabRanker();

// Gets ProcessType of tabs that should be scored by TabRanker.
int GetProcessTypeToScoreWithTabRanker();

// Gets number of oldest tabs that should be logged by TabRanker.
int GetNumOldestTabsToLogWithTabRanker();

// Whether to disable background time TabMetrics log.
bool DisableBackgroundLogWithTabRanker();

// Gets reload count penalty parameter for TabRanker.
float GetDiscardCountPenaltyTabRanker();

// Gets mru penalty parameter that converts mru index to scores.
float GetMRUScorerPenaltyTabRanker();

// Gets which type of scorer to use for TabRanker.
int GetScorerTypeForTabRanker();

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_
