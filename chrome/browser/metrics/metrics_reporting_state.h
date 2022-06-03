// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
#define CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_

#include "base/callback.h"
#include "build/build_config.h"
#include "components/metrics/metrics_service_client.h"

using OnMetricsReportingCallbackType = base::OnceCallback<void(bool)>;

// Changes metrics reporting state without caring about the success of the
// change.
void ChangeMetricsReportingState(bool enabled);

// Changes metrics reporting state to the new value of |enabled|. Starts or
// stops the metrics service based on the new state and then runs |callback_fn|
// (which can be null) with the updated state (as the operation may fail). On
// platforms other than CrOS and Android, also updates the underlying pref.
// TODO(https://crbug.com/880936): Support setting the pref on all platforms.
void ChangeMetricsReportingStateWithReply(
    bool enabled,
    OnMetricsReportingCallbackType callback_fn);

// Update metrics prefs on a permission (opt-in/out) change. When opting out,
// this clears various client ids. When opting in, this resets saving crash
// prefs, so as not to trigger upload of various stale data.
void UpdateMetricsPrefsOnPermissionChange(bool metrics_enabled);

#if !defined(OS_ANDROID)
// Propagates the state of metrics reporting pref (which may be policy
// managed) to GoogleUpdateSettings.
void ApplyMetricsReportingPolicy();
#endif

// Returns whether MetricsReporting can be modified by the user (except
// Android).
bool IsMetricsReportingPolicyManaged();

#endif  // CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
