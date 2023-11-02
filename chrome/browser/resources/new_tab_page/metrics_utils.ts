// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from './i18n_setup.js';

/** Records |durationMs| in the |metricName| histogram. */
export function recordDuration(metricName: string, durationMs: number) {
  chrome.metricsPrivate.recordValue(
      {
        metricName,
        type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
        min: 1,
        max: 60000,  // 60 seconds.
        buckets: 100,
      },
      Math.floor(durationMs));
}

/**
 * Records the duration between navigation start and |msSinceEpoch| in the
 * |metricName| histogram.
 */
export function recordLoadDuration(metricName: string, msSinceEpoch: number) {
  recordDuration(
      metricName, msSinceEpoch - loadTimeData.getValue('navigationStartTime'));
}

/**
 * Records |value| (expected to be between 0 and 10) into the ten-bucket
 * |metricName| histogram.
 */
export function recordPerdecage(metricName: string, value: number) {
  chrome.metricsPrivate.recordValue(
      {
        metricName,
        type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
        min: 1,       // Choose 1 if real min is 0.
        max: 11,      // Exclusive.
        buckets: 12,  // Numbers 0-10 and unused overflow bucket of 11.
      },
      value);
}

/**
 * Records that an event has happened rather than a value in the |metricName|
 * histogram.
 */
export function recordOccurence(metricName: string) {
  chrome.metricsPrivate.recordValue(
      {
        metricName,
        type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
        min: 1,
        max: 1,
        buckets: 1,
      },
      1);
}
