// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"

namespace features {

BASE_DECLARE_FEATURE(kCustomizedTabLoadTimeout);
BASE_DECLARE_FEATURE(kTabRanker);

}  // namespace features

namespace resource_coordinator {

base::TimeDelta GetTabLoadTimeout(const base::TimeDelta& default_timeout);

// Gets number of oldest tab that should be scored by TabRanker.
int GetNumOldestTabsToScoreWithTabRanker();

// Gets ProcessType of tabs that should be scored by TabRanker.
int GetProcessTypeToScoreWithTabRanker();

// Gets number of oldest tabs that should be logged by TabRanker.
int GetNumOldestTabsToLogWithTabRanker();

// Whether to disable recording of the TabManager_TabMetrics UKM.
bool DisableBackgroundLogWithTabRanker();

// Gets reload count penalty parameter for TabRanker.
float GetDiscardCountPenaltyTabRanker();

// Gets mru penalty parameter that converts mru index to scores.
float GetMRUScorerPenaltyTabRanker();

// Gets which type of scorer to use for TabRanker.
int GetScorerTypeForTabRanker();

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_
