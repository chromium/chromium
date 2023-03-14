// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_features.h"

#include "base/feature_list.h"

namespace dips {

// Enables the DIPS (Detect Incidental Party State) feature.
// On by default to allow for collecting metrics. All potentially dangerous
// behavior (database persistence, DIPS deletion) will be gated by params.
BASE_FEATURE(kFeature, "DIPS", base::FEATURE_ENABLED_BY_DEFAULT);

// Set whether DIPS persists its database to disk.
const base::FeatureParam<bool> kPersistedDatabaseEnabled{
    &kFeature, "persist_database", false};

// Set whether DIPS performs deletion.
const base::FeatureParam<bool> kDeletionEnabled{&kFeature, "delete", false};

// Set the time period that Chrome will wait for before clearing storage for a
// site after it performs some action (e.g. bouncing the user or using storage)
// without user interaction.
const base::FeatureParam<base::TimeDelta> kGracePeriod{
    &kFeature, "grace_period", base::Hours(1)};

// Set the cadence at which Chrome will attempt to clear incidental state
// repeatedly.
const base::FeatureParam<base::TimeDelta> kTimerDelay{&kFeature, "timer_delay",
                                                      base::Hours(1)};

// Set how long DIPS maintains an interaction for a site.
//
// If a site in the DIPS  database has an interaction within the grace period a
// DIPS-triggering action, then that action and all ensuing actions are
// protected from DIPS clearing until the interaction "expires" as set by this
// param.
const base::FeatureParam<base::TimeDelta> kInteractionTtl{
    &kFeature, "interaction_ttl", base::Days(45)};

// Sets the actions which will trigger DIPS clearing for a site. The default is
// to set to kBounce, but can be overridden by Finch experiment groups or by
// command-line flags.
constexpr base::FeatureParam<DIPSTriggeringAction>::Option
    kTriggeringActionOptions[] = {
        {DIPSTriggeringAction::kNone, "none"},
        {DIPSTriggeringAction::kStorage, "storage"},
        {DIPSTriggeringAction::kBounce, "bounce"},
        {DIPSTriggeringAction::kStatefulBounce, "stateful_bounce"}};
const base::FeatureParam<DIPSTriggeringAction> kTriggeringAction{
    &kFeature, "triggering_action", DIPSTriggeringAction::kNone,
    &kTriggeringActionOptions};

}  // namespace dips
