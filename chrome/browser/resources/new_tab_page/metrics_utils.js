// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

/**
 * Records |durationMs| in the |metricName| histogram.
 * @param {string} metricName
 * @param {number} durationMs
 */
export function recordDuration(metricName, durationMs) {
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
 * @param {string} metricName
 * @param {number} msSinceEpoch
 */
export function recordLoadDuration(metricName, msSinceEpoch) {
  recordDuration(
      metricName,
      msSinceEpoch - /** @type {number} */
          (loadTimeData.getValue('navigationStartTime')));
}

/**
 * Records |value| (expected to be between 0 and 10) into the ten-bucket
 * |metricName| histogram.
 * @param {string} metricName
 * @param {number} value
 */
export function recordPerdecage(metricName, value) {
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
 * @param {string} metricName
 */
export function recordOccurence(metricName) {
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
