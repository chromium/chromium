// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum NearbyShareOnboardingFinalState {
  DEVICE_NAME_PAGE = 0,
  VISIBILITY_PAGE = 1,
  COMPLETE = 2,
  INITIAL_PAGE = 3,
  MAX = 4,
}

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum NearbyShareOnboardingEntryPoint {
  SETTINGS = 0,
  TRAY = 1,
  SHARE_SHEET = 2,
  NEARBY_DEVICE_TRYING_TO_SHARE_NOTIFICATION = 3,
  MAX = 4,
}

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
enum NearbyShareOnboardingFlowEvent {
  ONBOARDING_SHOWN = 1,
  CONFIRM_ON_INITIAL_PAGE = 12,
  CANCEL_ON_INITIAL_PAGE = 13,
  VISIBILITY_CLICKED_ON_INITIAL_PAGE = 14,
  DEVICE_VISIBILITY_PAGE_SHOWN = 141,
  ALL_CONTACTS_SELECTED_AND_CONFIRMED = 1412,
  SOME_CONTACTS_SELECTED_AND_CONFIRMED = 1413,
  HIDDEN_SELECTED_AND_CONFIRMED = 1414,
  MANAGE_CONTACTS_SELECTED = 1415,
  CANCEL_SELECTED_ON_VISIBILITY_PAGE = 1416,
  YOUR_DEVICES_SELECTED_AND_CONFIRMED = 1417,
}

const NearbyShareOnboardingResultHistogramName =
    'Nearby.Share.Onboarding.Result';
const NearbyShareOnboardingEntryPointHistogramName =
    'Nearby.Share.Onboarding.EntryPoint';
const NearbyShareOnboardingDurationHistogramName =
    'Nearby.Share.Onboarding.Duration';
const NearbyShareOnboardingFlowEventHistogramName =
    'Nearby.Share.Onboarding.FlowEvent';
const NearbyShareOnboardingEntryPointResultPrefix = 'Nearby.Share.Onboarding.';
const NearbyShareOnboardingEntryPointResultSuffix = '.Result';

/**
 * Tracks time that onboarding is started. Gets set to null after onboarding is
 * complete as a way to track if onboarding is in progress.
 */
let onboardingInitiatedTimestamp: number|null;

/**
 * Determines which histogram to log to based on entry point.
 */
function processOnboardingEntryPointResultMetrics(
    nearbyShareOnboardingEntryPoint: NearbyShareOnboardingEntryPoint,
    nearbyShareOnboardingFinalState: NearbyShareOnboardingFinalState): void {
  let entryPointString;
  switch (nearbyShareOnboardingEntryPoint) {
    case NearbyShareOnboardingEntryPoint.SETTINGS:
      entryPointString = 'Settings';
      break;
    case NearbyShareOnboardingEntryPoint.TRAY:
      entryPointString = 'Tray';
      break;
    case NearbyShareOnboardingEntryPoint.SHARE_SHEET:
      entryPointString = 'ShareSheet';
      break;
    case NearbyShareOnboardingEntryPoint
        .NEARBY_DEVICE_TRYING_TO_SHARE_NOTIFICATION:
      entryPointString = 'NearbyDeviceTryingToShareNotification';
      break;
    default:
      assertNotReached('Invalid nearbyShareOnboardingEntryPoint');
  }

  chrome.send('metricsHandler:recordInHistogram', [
    NearbyShareOnboardingEntryPointResultPrefix + entryPointString +
        NearbyShareOnboardingEntryPointResultSuffix,
    nearbyShareOnboardingFinalState,
    NearbyShareOnboardingFinalState.MAX,
  ]);
}

export function getOnboardingEntryPoint(url: URL):
    NearbyShareOnboardingEntryPoint {
  let nearbyShareOnboardingEntryPoint: NearbyShareOnboardingEntryPoint =
      NearbyShareOnboardingEntryPoint.MAX;

  if (url.hostname === 'nearby') {
    nearbyShareOnboardingEntryPoint =
        NearbyShareOnboardingEntryPoint.SHARE_SHEET;
  } else if (url.hostname === 'os-settings') {
    const urlParams = new URLSearchParams(url.search);

    nearbyShareOnboardingEntryPoint =
        getOnboardingEntrypointFromQueryParam(urlParams.get('entrypoint'));
  } else {
    assertNotReached('Invalid nearbyShareOnboardingEntryPoint');
  }

  return nearbyShareOnboardingEntryPoint;
}

/**
 * Records the onboarding flow entrypoint and stores the time at which
 * onboarding was initiated. The url param is used to infer the entrypoint.
 */
export function processOnboardingInitiatedMetrics(
    nearbyShareOnboardingEntryPoint: NearbyShareOnboardingEntryPoint): void {
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
 */
export function processOnePageOnboardingInitiatedMetrics(
    nearbyShareOnboardingEntryPoint: NearbyShareOnboardingEntryPoint): void {
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

function getOnboardingEntrypointFromQueryParam(queryParam: string|null):
    NearbyShareOnboardingEntryPoint {
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
 */
export function processOnboardingCancelledMetrics(
    nearbyShareOnboardingEntryPointState: NearbyShareOnboardingEntryPoint,
    nearbyShareOnboardingFinalState: NearbyShareOnboardingFinalState): void {
  if (!onboardingInitiatedTimestamp) {
    return;
  }
  chrome.send('metricsHandler:recordInHistogram', [
    NearbyShareOnboardingResultHistogramName,
    nearbyShareOnboardingFinalState,
    NearbyShareOnboardingFinalState.MAX,
  ]);

  processOnboardingEntryPointResultMetrics(
      nearbyShareOnboardingEntryPointState, nearbyShareOnboardingFinalState);
  onboardingInitiatedTimestamp = null;
}

/**
 * If one-page onboarding was cancelled this function is invoked to record
 * during which step the cancellation occurred.
 */
export function processOnePageOnboardingCancelledMetrics(
    nearbyShareOnboardingEntryPointState: NearbyShareOnboardingEntryPoint,
    nearbyShareOnboardingFinalState: NearbyShareOnboardingFinalState): void {
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
    getOnboardingCancelledFlowEvent(nearbyShareOnboardingFinalState),
  ]);

  processOnboardingEntryPointResultMetrics(
      nearbyShareOnboardingEntryPointState, nearbyShareOnboardingFinalState);
  onboardingInitiatedTimestamp = null;
}

function getOnboardingCancelledFlowEvent(
    nearbyShareOnboardingFinalState: NearbyShareOnboardingFinalState):
    NearbyShareOnboardingFlowEvent {
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
export function processOnboardingCompleteMetrics(
    nearbyShareOnboardingEntryPointState: NearbyShareOnboardingEntryPoint):
    void {
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

  processOnboardingEntryPointResultMetrics(
      nearbyShareOnboardingEntryPointState,
      NearbyShareOnboardingFinalState.COMPLETE);
  onboardingInitiatedTimestamp = null;
}

/**
 * Records a metric for successful one-page onboarding flow completion and the
 * time it took to complete.
 */
export function processOnePageOnboardingCompleteMetrics(
    nearbyShareOnboardingEntryPointState: NearbyShareOnboardingEntryPoint,
    nearbyShareOnboardingFinalState: NearbyShareOnboardingFinalState,
    visibility: Visibility|null): void {
  if (!onboardingInitiatedTimestamp) {
    return;
  }

  chrome.send('metricsHandler:recordSparseHistogram', [
    NearbyShareOnboardingFlowEventHistogramName,
    getOnboardingCompleteFlowEvent(nearbyShareOnboardingFinalState, visibility),
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

  processOnboardingEntryPointResultMetrics(
      nearbyShareOnboardingEntryPointState,
      NearbyShareOnboardingFinalState.COMPLETE);
  onboardingInitiatedTimestamp = null;
}

/**
 * Gets the corresponding onboarding complete flow event based on final state
 * and visibility selected.
 */
function getOnboardingCompleteFlowEvent(
    nearbyShareOnboardingFinalState: NearbyShareOnboardingFinalState,
    visibility: Visibility|null): NearbyShareOnboardingFlowEvent {
  switch (nearbyShareOnboardingFinalState) {
    case NearbyShareOnboardingFinalState.INITIAL_PAGE:
      return NearbyShareOnboardingFlowEvent.CONFIRM_ON_INITIAL_PAGE;
    case NearbyShareOnboardingFinalState.VISIBILITY_PAGE:
      return getOnboardingCompleteFlowEventOnVisibilityPage(visibility);
    default:
      assertNotReached('Invalid final state');
  }
}

/**
 * Gets the corresponding onboarding complete flow event on visibility
 * selection page based on final visibility selected.
 */
function getOnboardingCompleteFlowEventOnVisibilityPage(
    visibility: Visibility|null): NearbyShareOnboardingFlowEvent {
  switch (visibility) {
    case Visibility.kAllContacts:
      return NearbyShareOnboardingFlowEvent.ALL_CONTACTS_SELECTED_AND_CONFIRMED;
    case Visibility.kSelectedContacts:
      return NearbyShareOnboardingFlowEvent
          .SOME_CONTACTS_SELECTED_AND_CONFIRMED;
    case Visibility.kYourDevices:
      return NearbyShareOnboardingFlowEvent.YOUR_DEVICES_SELECTED_AND_CONFIRMED;
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
processOnePageOnboardingVisibilityButtonOnInitialPageClickedMetrics(): void {
  chrome.send('metricsHandler:recordSparseHistogram', [
    NearbyShareOnboardingFlowEventHistogramName,
    NearbyShareOnboardingFlowEvent.VISIBILITY_CLICKED_ON_INITIAL_PAGE,
  ]);
}

/**
 * Records a metrics for successfully displaying visibility selection page.
 */
export function processOnePageOnboardingVisibilityPageShownMetrics(): void {
  chrome.send('metricsHandler:recordSparseHistogram', [
    NearbyShareOnboardingFlowEventHistogramName,
    NearbyShareOnboardingFlowEvent.DEVICE_VISIBILITY_PAGE_SHOWN,
  ]);
}

/**
 * Records a metrics for users clicking Manage Contacts button on the
 * visibility selection page.
 */
export function processOnePageOnboardingManageContactsMetrics(): void {
  chrome.send('metricsHandler:recordSparseHistogram', [
    NearbyShareOnboardingFlowEventHistogramName,
    NearbyShareOnboardingFlowEvent.MANAGE_CONTACTS_SELECTED,
  ]);
}
