// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_COLLECTION_PARAMS_H_
#define CHROME_BROWSER_METRICS_PERF_COLLECTION_PARAMS_H_

#include <stdint.h>

#include "base/time/time.h"

namespace metrics {

// Define parameters for a collection trigger.
struct TriggerParams {
  // Trigger profile collection with 1/|sampling_factor| probability.
  int64_t sampling_factor = 1;

  // Upper bound of a random delay added before collection on a trigger event.
  // The delay is uniformly chosen between 0 and this value.
  base::TimeDelta max_collection_delay;
};

// Defines collection parameters for metric collectors. Each collection trigger
// has its own set of parameters.
struct CollectionParams {
  CollectionParams();

  // Time a profile is collected for, where it makes sense.
  base::TimeDelta collection_duration;

  // For PERIODIC_COLLECTION, partition time since login into successive
  // intervals of this duration. In each interval, a random time is picked to
  // collect a profile.
  base::TimeDelta periodic_interval;

  // Parameters for other collection triggers.
  TriggerParams resume_from_suspend;
  TriggerParams restore_session;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_COLLECTION_PARAMS_H_
