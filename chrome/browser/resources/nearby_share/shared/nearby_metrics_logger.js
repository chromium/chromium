// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {assertNotReached} from 'chrome://resources/js/assert.m.js';

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * @enum {number}
 */
/* #export */ const NearbyShareOnboardingFinalState = {
  DEVICE_NAME_PAGE: 0,
  VISIBILITY_PAGE: 1,
  COMPLETE: 2,
  MAX: 3,
};

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * @enum {number}
 */
const NearbyShareOnboardingEntryPoint = {
  SETTINGS: 0,
  TRAY: 1,
  SHARE_SHEET: 2,
  MAX: 3,
};

const NearbyShareOnboardingResultHistogramName =
    'Nearby.Share.Onboarding.Result';
const NearbyShareOnboardingEntryPointHistogramName =
    'Nearby.Share.Onboarding.EntryPoint';
const NearbyShareOnboardingDurationHistogramName =
    'Nearby.Share.Onboarding.Duration';

/**
 * Tracks time that onboarding is started. Gets set to null after onboarding is
 * complete as a way to track if onboarding is in progress.
 * @type {?number}
 */
let onboardingInitiatedTimestamp;

/**
 * Records the onboarding flow entrypoint and stores the time at which
 * onboarding was initiated. The url param is used to infer the entrypoint.
 * @param {URL} url
 */
/* #export */ function processOnboardingInitiatedMetrics(url) {
  let nearbyShareOnboardingEntryPoint;

  if (url.hostname === 'nearby') {
    nearbyShareOnboardingEntryPoint =
        NearbyShareOnboardingEntryPoint.SHARE_SHEET;
  } else if (url.hostname === 'os-settings') {
    const urlParams = new URLSearchParams(url.search);

    nearbyShareOnboardingEntryPoint =
        (urlParams.get('entrypoint') === 'settings') ?
        NearbyShareOnboardingEntryPoint.SETTINGS :
        NearbyShareOnboardingEntryPoint.TRAY;
  } else {
    assertNotReached('Invalid nearbyShareOnboardingEntryPoint');
  }

  chrome.send('metricsHandler:recordInHistogram', [
    NearbyShareOnboardingEntryPointHistogramName,
    nearbyShareOnboardingEntryPoint, NearbyShareOnboardingEntryPoint.MAX
  ]);

  // Set time at which onboarding was initiated to track duration.
  onboardingInitiatedTimestamp = window.performance.now();
}

/**
 * If onboarding was cancelled this function is invoked to record during which
 * step the cancellation occurred.
 *
 * @param {NearbyShareOnboardingFinalState} nearbyShareOnboardingFinalState
 */
/* #export */ function processOnboardingCancelledMetrics(
    nearbyShareOnboardingFinalState) {
  if (!onboardingInitiatedTimestamp) {
    return;
  }
  chrome.send('metricsHandler:recordInHistogram', [
    NearbyShareOnboardingResultHistogramName, nearbyShareOnboardingFinalState,
    NearbyShareOnboardingFinalState.MAX
  ]);
  onboardingInitiatedTimestamp = null;
}

/**
 * Records a metric for successful onboarding flow completion and the time it
 * took to complete.
 */
/* #export */ function processOnboardingCompleteMetrics() {
  if (!onboardingInitiatedTimestamp) {
    return;
  }
  chrome.send('metricsHandler:recordInHistogram', [
    NearbyShareOnboardingResultHistogramName,
    NearbyShareOnboardingFinalState.COMPLETE,
    NearbyShareOnboardingFinalState.MAX
  ]);

  chrome.send('metricsHandler:recordMediumTime', [
    NearbyShareOnboardingDurationHistogramName,
    window.performance.now() - onboardingInitiatedTimestamp
  ]);

  onboardingInitiatedTimestamp = null;
}