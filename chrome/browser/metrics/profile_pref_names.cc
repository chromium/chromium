// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/profile_pref_names.h"

namespace metrics::prefs {

// Bool pref containing whether the user had consented to metrics
// collection. If true, then a new client ID will be generated if the user goes
// from an no->yes metrics consent state.
const char kMetricsRequiresClientIdResetOnConsent[] =
    "metrics.requires_client_id_reset_on_consent";

// Bool pref containing the current user metrics consent.
const char kMetricsUserConsent[] = "metrics.user_consent";

// String pref containing pseudo-anonymous identifier of a user. Will be reset
// if a user goes from a no->yes metrics consent state.
const char kMetricsUserId[] = "metrics.user_id";

// Array of dictionaries that are each UMA logs that were not sent because the
// user session ended before accumulated metrics were sent.
const char kMetricsUserMetricLogs[] = "metrics.user_metrics_logs";

// A dictionary that contains metadata about the unsent user metrics logs. The
// metadata is recorded before the unsent logs are persisted and will be written
// into the metrics logs later before being sent.
const char kMetricsUserMetricLogsMetadata[] =
    "metrics.user_metrics_logs_metadata";

}  // namespace metrics::prefs
