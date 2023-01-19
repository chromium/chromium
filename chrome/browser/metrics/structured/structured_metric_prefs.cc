// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/structured_metric_prefs.h"

namespace metrics::structured {

// Keeps track of the system uptime of the last recorded uptime. This is used to
// determine whether the reset counter or not.
//
// TODO(crbug/1350322): Remove this and kEventSequenceResetCounter once
// implementation for handling resets for CrOS is implemented in platform2.
const char kEventSequenceLastSystemUptime[] =
    "metrics.event_sequence.last_system_uptime";

// Keeps track of the device reset counter.
const char kEventSequenceResetCounter[] =
    "metrics.event_sequence.reset_counter";

}  // namespace metrics::structured
