// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const BluetoothUiSurface = {
  SETTINGS_DEVICE_LIST_SUBPAGE: 0,
  SETTINGS_DEVICE_DETAIL_SUBPAGE: 1,
  SETTINGS_PAIRING_DIALOG: 2,
  BLUETOOTH_QUICK_SETTINGS: 3,
  PAIRING_DIALOG: 4,
  PAIRED_NOTIFICATION: 5,
  CONNECTION_TOAST: 6,
  DISCONNECTED_TOAST: 7,
  OOBE_HID_DETECTION: 8,
  PAIRED_TOAST: 9,
};

/**
 * Records metric indicating that |uiSurface| was displayed to the user.
 * @param {!BluetoothUiSurface} uiSurface Bluetooth UI surface displayed.
 */
export function recordBluetoothUiSurfaceMetrics(uiSurface) {
  chrome.metricsPrivate.recordEnumerationValue(
      'Bluetooth.ChromeOS.UiSurfaceDisplayed', uiSurface,
      Object.keys(BluetoothUiSurface).length);
}

/**
 * Records metrics indicating the |durationInMs| taken for a user initiated
 * reconnection attempt to complete.
 * @param {number} durationInMs
 * @param {!chrome.bluetooth.Transport|undefined} transport The transport type
 *     of the device.
 * @param {!chrome.bluetoothPrivate.ConnectResultType|undefined}
 *     connectionResult
 */
export function recordUserInitiatedReconnectionAttemptDuration(
    durationInMs, transport, connectionResult) {
  if (!transport || !connectionResult) {
    return;
  }
  let transportHistogramName;
  switch (transport) {
    case chrome.bluetooth.Transport.CLASSIC:
      transportHistogramName = '.Classic';
      break;
    case chrome.bluetooth.Transport.DUAL:
      transportHistogramName = '.Dual';
      break;
    case chrome.bluetooth.Transport.LE:
      transportHistogramName = '.BLE';
      break;
    default:
      // Invalid transport type.
      return;
  }
  const successHistogramName =
      connectionResult === chrome.bluetoothPrivate.ConnectResultType.SUCCESS ?
      'Success' :
      'Failure';
  chrome.metricsPrivate.recordTime(
      'Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Duration.' +
          successHistogramName,
      durationInMs);
  chrome.metricsPrivate.recordTime(
      'Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Duration.' +
          successHistogramName + transportHistogramName,
      durationInMs);
}

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const FastPairSavedDevicesUiEvent = {
  SETTINGS_SAVED_DEVICE_LIST_SUBPAGE_SHOWN: 0,
  SETTINGS_SAVED_DEVICE_LIST_HAS_DEVICES: 1,
  SETTINGS_SAVED_DEVICE_LIST_REMOVE_DIALOG: 2,
  SETTINGS_SAVED_DEVICE_LIST_REMOVE: 3,
};

/**
 * Records metric indicating that |uiEvent| was displayed to the user.
 * @param {!FastPairSavedDevicesUiEvent} uiEvent
 * Fast Pair Saved Devices UI event displayed.
 */
export function recordSavedDevicesUiEventMetrics(uiEvent) {
  chrome.metricsPrivate.recordEnumerationValue(
      'Bluetooth.ChromeOS.FastPair.SavedDevices.UiEvent', uiEvent,
      Object.keys(FastPairSavedDevicesUiEvent).length);
}
