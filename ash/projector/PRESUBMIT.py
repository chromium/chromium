# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Optional but recommended
PRESUBMIT_VERSION = '2.0.0'


def CheckMetricsChangeHasTrackerUpdatedMessage(input_api, output_api):
    """Reminder to update metrics tracker and verify in CL description."""
    description = input_api.change.DescriptionText()
    metrics_tracker_re = input_api.re.compile(
        r'PROJECTOR_METRICS_TRACKER_UPDATED|NO_METRICS_CHANGED',
        input_api.re.MULTILINE)
    metrics_file_re = input_api.re.compile(
        r'.*chromium\/src\/ash\/projector\/projector_metrics\.cc')
    error_message = ("If this CL adds, changes, or deletes a metric, please\n"
    "update go/projector-metrics-tracker before submitting, then add\n"
    "PROJECTOR_METRICS_TRACKER_UPDATED to the CL description. If there are\n"
    "no changes to metrics in this CL, add NO_METRICS_CHANGED to the CL\n"
    "description.")

    for f in input_api.change.AffectedFiles(include_deletes=True,
                                            file_filter=None):
        if metrics_file_re.match(f.AbsoluteLocalPath(
        )) and not metrics_tracker_re.search(description):
            return [output_api.PresubmitError(error_message)]
    return []
