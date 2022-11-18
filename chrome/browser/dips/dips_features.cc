// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_features.h"

#include "base/feature_list.h"

namespace dips {

// Enables the DIPS (Detect Incidental Party State) feature.
BASE_FEATURE(kFeature, "DIPS", base::FEATURE_DISABLED_BY_DEFAULT);

// Set whether DIPS persists its database to disk.
const base::FeatureParam<bool> kPersistedDatabaseEnabled{
    &kFeature, "persist_database", false};

// Set the time period that Chrome will wait for before clearing storage for a
// site after it performs some action (e.g. bouncing the user or using storage)
// without user interaction.
const base::FeatureParam<base::TimeDelta> kGracePeriod{
    &kFeature, "grace_period", base::Hours(24)};

// Set the cadence at which Chrome will attempt to clear incidental state
// repeatedly.
const base::FeatureParam<base::TimeDelta> kTimerDelay{&kFeature, "timer_delay",
                                                      base::Hours(24)};

}  // namespace dips
