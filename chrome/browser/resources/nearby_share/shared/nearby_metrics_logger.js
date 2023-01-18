// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * @enum {number}
 */
export const NearbyShareOnboardingFinalState = {
  DEVICE_NAME_PAGE: 0,
  VISIBILITY_PAGE: 1,
  COMPLETE: 2,
  INITIAL_PAGE: 3,
  MAX: 4,
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
  NEARBY_DEVICE_TRYING_TO_SHARE_NOTIFICATION: 3,
  MAX: 4,
};

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * @enum {number}
 */
const NearbyShareOnboardingFlowEvent = {
  ONBOARDING_SHOWN: 1,
  CONFIRM_ON_INITIAL_PAGE: 12,
  CANCEL_ON_INITIAL_PAGE: 13,
  VISIBILITY_CLICKED_ON_INITIAL_PAGE: 14,
  DEVICE_VISIBILITY_PAGE_SHOWN: 141,
  ALL_CONTACTS_SELECTED_AND_CONFIRMED: 1412,
  SOME_CONTACTS_SELECTED_AND_CONFIRMED: 1413,
  HIDDEN_SELECTED_AND_CONFIRMED: 1414,
  MANAGE_CONTACTS_SELECTED: 1415,
  CANCEL_SELECTED_ON_VISIBILITY_PAGE: 1416,
};

const NearbyShareOnboardingResultHistogramName =
    'Nearby.Share.Onboarding.Result';
const NearbyShareOnboardingEntryPointHistogramName =
    'Nearby.Share.Onboarding.EntryPoint';
const NearbyShareOnboardingDurationHistogramName =
    'Nearby.Share.Onboarding.Duration';
const NearbyShareOnboardingFlowEventHistogramName =
    'Nearby.Share.Onboarding.FlowEvent';

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
export function processOnboardingInitiatedMetrics(url) {
  let nearbyShareOnboardingEntryPoint;

  if (url.hostname === 'nearby') {
    nearbyShareOnboardingEntryPoint =
        NearbyShareOnboardingEntryPoint.SHARE_SHEET;
  } else if (url.hostname === 'os-settings') {
    const urlParams = new URLSearchParams(url.search);

    nearbyShareOnboardingEntryPoint =
        getOnboardingEntrypointFromQueryParam_(urlParams.get('entrypoint'));
  } else {
    assertNotReached('Invalid nearbyShareOnboardingEntryPoint');
  }

  chrome.send('metricsHandler:recordInHistogram', [
    NearbyShareOnboardingEntryPointHistogramName,
    nearbyShareOnboardingEntryPoint,
    NearbyShareOnboardingEntryPoint.MAX,
  ]);
  // Set time at which onboarding was initiated to track duration.
  onboardingInitiatedTimestamp = window.performance.now();
}

/**
 * Records the one-page onboarding flow entrypoint and stores the time at which
 * one-page onboarding was initiated. The url param is used to infer the
 * entrypoint.
 * @param {URL} url
 */
export function processOnePageOnboardingInitiatedMetrics(url) {
  let nearbyShareOnboardingEntryPoint;

  if (url.hostname === 'nearby') {
    nearbyShareOnboardingEntryPoint =
        NearbyShareOnboardingEntryPoint.SHARE_SHEET;
  } else if (url.hostname === 'os-settings') {
    const urlParams = new URLSearchParams(url.search);

    nearbyShareOnboardingEntryPoint =
        getOnboardingEntrypointFromQueryParam_(urlParams.get('entrypoint'));
  } else {
    assertNotReached('Invalid nearbyShareOnboardingEntryPoint');
  }

  chrome.send('metricsHandler:recordInHistogram', [
    NearbyShareOnboardingEntryPointHistogramName,
    nearbyShareOnboardingEntryPoint,
    NearbyShareOnboardingEntryPoint.MAX,
  ]);

  chrome.send('metricsHandler:recordSparseHistogram', [
    NearbyShareOnboardingFlowEventHistogramName,
    NearbyShareOnboardingFlowEvent.ONBOARDING_SHOWN,
  ]);

  // Set time at which onboarding was initiated to track duration.
  onboardingInitiatedTimestamp = window.performance.now();
}

/**
 * @param {?string} queryParam
 * @return {NearbyShareOnboardingEntryPoint}
 *
 * @private
 */
function getOnboardingEntrypointFromQueryParam_(queryParam) {
  switch (queryParam) {
    case 'settings':
      return NearbyShareOnboardingEntryPoint.SETTINGS;
    case 'notification':
      return NearbyShareOnboardingEntryPoint
          .NEARBY_DEVICE_TRYING_TO_SHARE_NOTIFICATION;
    default:
      return NearbyShareOnboardingEntryPoint.TRAY;
  }
}

/**
 * If onboarding was cancelled this function is invoked to record during which
 * step the cancellation occurred.
 *
 * @param {NearbyShareOnboardingFinalState} nearbyShareOnboardingFinalState
 */
export function processOnboardingCancelledMetrics(
    nearbyShareOnboardingFinalState) {
  if (!onboardingInitiatedTimestamp) {
    return;
  }
  chrome.send('metricsHandler:recordInHistogram', [
    NearbyShareOnboardingResultHistogramName,
    nearbyShareOnboardingFinalState,
    NearbyShareOnboardingFinalState.MAX,
  ]);
  onboardingInitiatedTimestamp = null;
}

/**
 * If one-page onboarding was cancelled this function is invoked to record
 * during which step the cancellation occurred.
 *
 * @param {NearbyShareOnboardingFinalState} nearbyShareOnboardingFinalState
 */
export function processOnePageOnboardingCancelledMetrics(
    nearbyShareOnboardingFinalState) {
  if (!onboardingInitiatedTimestamp) {
    return;
  }
  chrome.send('metricsHandler:recordInHistogram', [
    NearbyShareOnboardingResultHistogramName,
    nearbyShareOnboardingFinalState,
    NearbyShareOnboardingFinalState.MAX,
  ]);

  chrome.send('metricsHandler:recordSparseHistogram', [
    NearbyShareOnboardingFlowEventHistogramName,
    getOnboardingCancelledFlowEvent_(nearbyShareOnboardingFinalState),
  ]);
  onboardingInitiatedTimestamp = null;
}

/**
 * Get the corresponding onboarding canceling event.
 *
 * @param {NearbyShareOnboardingFinalState} nearbyShareOnboardingFinalState
 * @returns {NearbyShareOnboardingFlowEvent}
 *
 * @private
 */
function getOnboardingCancelledFlowEvent_(nearbyShareOnboardingFinalState) {
  switch (nearbyShareOnboardingFinalState) {
    case NearbyShareOnboardingFinalState.INITIAL_PAGE:
      return NearbyShareOnboardingFlowEvent.CANCEL_ON_INITIAL_PAGE;
    case NearbyShareOnboardingFinalState.VISIBILITY_PAGE:
      return NearbyShareOnboardingFlowEvent.CANCEL_SELECTED_ON_VISIBILITY_PAGE;
    default:
      assertNotReached('Invalid final state for cancel event');
  }
}

/**
 * Records a metric for successful onboarding flow completion and the time it
 * took to complete.
 */
export function processOnboardingCompleteMetrics() {
  if (!onboardingInitiatedTimestamp) {
    return;
  }
  chrome.send('metricsHandler:recordInHistogram', [
    NearbyShareOnboardingResultHistogramName,
    NearbyShareOnboardingFinalState.COMPLETE,
    NearbyShareOnboardingFinalState.MAX,
  ]);

  chrome.send('metricsHandler:recordMediumTime', [
    NearbyShareOnboardingDurationHistogramName,
    window.performance.now() - onboardingInitiatedTimestamp,
  ]);

  onboardingInitiatedTimestamp = null;
}

/**
 * Records a metric for successful one-page onboarding flow completion and the
 * time it took to complete.
 *
 * @param {NearbyShareOnboardingFinalState} nearbyShareOnboardingFinalState
 * @param {?Visibility} visibility
 */
export function processOnePageOnboardingCompleteMetrics(
    nearbyShareOnboardingFinalState, visibility) {
  if (!onboardingInitiatedTimestamp) {
    return;
  }

  chrome.send('metricsHandler:recordSparseHistogram', [
    NearbyShareOnboardingFlowEventHistogramName,
    getOnboardingCompleteFlowEvent_(
        nearbyShareOnboardingFinalState, visibility),
  ]);

  chrome.send('metricsHandler:recordInHistogram', [
    NearbyShareOnboardingResultHistogramName,
    NearbyShareOnboardingFinalState.COMPLETE,
    NearbyShareOnboardingFinalState.MAX,
  ]);

  chrome.send('metricsHandler:recordMediumTime', [
    NearbyShareOnboardingDurationHistogramName,
    window.performance.now() - onboardingInitiatedTimestamp,
  ]);

  onboardingInitiatedTimestamp = null;
}

/**
 * Gets the corresponding onboarding complete flow event based on final state
 * and visibility selected.
 *
 * @param {NearbyShareOnboardingFinalState} nearbyShareOnboardingFinalState
 * @param {?Visibility} visibility
 * @returns {NearbyShareOnboardingFlowEvent}
 *
 * @private
 */
function getOnboardingCompleteFlowEvent_(
    nearbyShareOnboardingFinalState, visibility) {
  switch (nearbyShareOnboardingFinalState) {
    case NearbyShareOnboardingFinalState.INITIAL_PAGE:
      return NearbyShareOnboardingFlowEvent.CONFIRM_ON_INITIAL_PAGE;
    case NearbyShareOnboardingFinalState.VISIBILITY_PAGE:
      return getOnboardingCompleteFlowEventOnVisibilityPage_(visibility);
    default:
      assertNotReached('Invalid final state');
  }
}

/**
 * Gets the corresponding onboarding complete flow event on visibility
 * selection page based on final visibility selected.
 *
 * @param {?Visibility} visibility
 * @returns {NearbyShareOnboardingFlowEvent}
 *
 * @private
 */
function getOnboardingCompleteFlowEventOnVisibilityPage_(visibility) {
  switch (visibility) {
    case Visibility.kAllContacts:
      return NearbyShareOnboardingFlowEvent.ALL_CONTACTS_SELECTED_AND_CONFIRMED;
    case Visibility.kSelectedContacts:
      return NearbyShareOnboardingFlowEvent
          .SOME_CONTACTS_SELECTED_AND_CONFIRMED;
    case Visibility.kNoOne:
      return NearbyShareOnboardingFlowEvent.HIDDEN_SELECTED_AND_CONFIRMED;
    default:
      assertNotReached('Invalid visibility');
  }
}

/**
 * Records a metric for users clicking the visibility selection button on
 * the initial onboarding page.
 */
export function
processOnePageOnboardingVisibilityButtonOnInitialPageClickedMetrics() {
  chrome.send('metricsHandler:recordSparseHistogram', [
    NearbyShareOnboardingFlowEventHistogramName,
    NearbyShareOnboardingFlowEvent.VISIBILITY_CLICKED_ON_INITIAL_PAGE,
  ]);
}

/**
 * Records a metrics for successfully displaying visibility selection page.
 */
export function processOnePageOnboardingVisibilityPageShownMetrics() {
  chrome.send('metricsHandler:recordSparseHistogram', [
    NearbyShareOnboardingFlowEventHistogramName,
    NearbyShareOnboardingFlowEvent.DEVICE_VISIBILITY_PAGE_SHOWN,
  ]);
}

/**
 * Records a metrics for users clicking Manage Contacts button on the
 * visibility selection page.
 */
export function processOnePageOnboardingManageContactsMetrics() {
  chrome.send('metricsHandler:recordSparseHistogram', [
    NearbyShareOnboardingFlowEventHistogramName,
    NearbyShareOnboardingFlowEvent.MANAGE_CONTACTS_SELECTED,
  ]);
}
