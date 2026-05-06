// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
#define CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "components/metrics/metrics_reporting_level.h"

namespace metrics {

using OnMetricsReportingCallbackType = base::OnceCallback<void(bool)>;
using OnMetricsReportingLevelCallbackType =
    base::OnceCallback<void(MetricsReportingLevel)>;

// Specifies from where a change to the metrics reporting state was made. When
// metrics reporting is enabled from a settings page, histogram data that was
// collected while metrics reporting was disabled should be cleared (marked as
// reported) so as to not include them in the next log.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.metrics
enum class ChangeMetricsReportingStateCalledFrom {
  kUiSettings,

  // The user opted out of metrics reporting in the First Run Experience.
  kUiFirstRun,

  // Called from the session crashed dialog window.
  kSessionCrashedDialog,

  // Called from Chrome OS settings change. Chrome OS manages settings
  // externally and metrics service listens for changes.
  kCrosMetricsSettingsChange,

  // Called from Chrome OS on settings creation/initialization. This happens
  // once on each log in.
  kCrosMetricsSettingsCreated,

  // Called on ChromeOS from Lacros on initialization to initialize state.
  kCrosMetricsInitializedFromAsh,

  // Called on ChromeOS pre-consent metrics. This happens once per powerwash.
  kCrosMetricsPreConsent,
};

// Changes metrics reporting state without caring about the success of the
// change. |called_from| should be set to |kUiSettings| when enabling metrics
// from a settings page (to mark histogram data collected while metrics
// reporting was disabled as reported so as to not include them in the next
// log). If |called_from| is set to anything else, then metrics will not be
// cleared when enabling metrics reporting.
void ChangeMetricsReportingState(
    bool enabled,
    ChangeMetricsReportingStateCalledFrom called_from);

// Changes metrics reporting state to the new value of |enabled|. Starts or
// stops the metrics service based on the new state and then runs |callback_fn|
// (which can be null) with the updated state (as the operation may fail). On
// platforms other than CrOS and Android, also updates the underlying pref.
// |called_from| should be set to |kUiSettings| when enabling metrics from a
// settings page (to mark histogram data collected while metrics reporting was
// disabled as reported so as to not include them in the next log). If
// |called_from| is set to anything else, then metrics will not be cleared when
// enabling metrics reporting.
void ChangeMetricsReportingStateWithReply(
    bool enabled,
    OnMetricsReportingCallbackType callback_fn,
    ChangeMetricsReportingStateCalledFrom called_from);

// Implementation detail for ChangeMetricsReportingStateWithReply.
// Not intended for use outside of this file and
// ChromeMetricsServiceAccessor.
//
// Changes metrics reporting state to the new value of |enabled|. Starts or
// stops the metrics service based on the new state and then runs |callback_fn|
// (which can be null) with the updated state (as the operation may fail).
// If |level_to_write| is provided, it updates the kMetricsReportingLevel pref;
// otherwise, it updates the underlying legacy kMetricsReportingEnabled pref (on
// platforms other than CrOS and Android).
// |called_from| should be set to |kUiSettings| when enabling metrics from a
// settings page (to mark histogram data collected while metrics reporting was
// disabled as reported so as to not include them in the next log). If
// |called_from| is set to anything else, then metrics will not be cleared when
// enabling metrics reporting.
void ChangeMetricsReportingStateWithReplyImpl(
    bool enabled,
    OnMetricsReportingCallbackType callback_fn,
    ChangeMetricsReportingStateCalledFrom called_from,
    std::optional<MetricsReportingLevel> level_to_write);

// Changes metrics reporting state to the new value based on |level| without
// caring about the success of the change. |called_from| should be set to
// |kUiSettings| when enabling metrics from a settings page (to mark histogram
// data collected while metrics reporting was disabled as reported so as to not
// include them in the next log). If |called_from| is set to anything else,
// then metrics will not be cleared when enabling metrics reporting.
// TODO(b/492510818): This will be replacing the ChangeMetricsReportingState()
// method taking a boolean.
void ChangeMetricsReportingState(
    MetricsReportingLevel level,
    ChangeMetricsReportingStateCalledFrom called_from);

// Update metrics prefs on a permission (opt-in/out) change. When opting out,
// this clears various client ids. When opting in, this resets saving crash
// prefs, so as not to trigger upload of various stale data. |called_from|
// should be set to |kUiSettings| when enabling metrics from a settings page (to
// mark histogram data collected while metrics reporting was disabled as
// reported so as to not include them in the next log). If |called_from| is set
// to anything else, then metrics will not be cleared when enabling metrics
// reporting.
void UpdateMetricsPrefsOnPermissionChange(
    bool metrics_enabled,
    ChangeMetricsReportingStateCalledFrom called_from);

// Propagates the state of metrics reporting pref (which may be policy
// managed) to GoogleUpdateSettings.
void ApplyMetricsReportingPolicy();

// Returns whether MetricsReporting can be modified by the user (except
// Android).
//
// For Ash Chrome, metrics reporting may be controlled by an enterprise policy
// and the metrics service pref inherits the value from the policy. Reporting
// policy will be considered managed if an enterprise policy exists.
bool IsMetricsReportingPolicyManaged();

// Clears previously collected metrics data. Used when enabling metrics to
// prevent data collected while metrics reporting was disabled from being
// included in the next log. Note that histogram data is not discarded. Rather,
// they are just marked as being already reported.
void ClearPreviouslyCollectedMetricsData();

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
