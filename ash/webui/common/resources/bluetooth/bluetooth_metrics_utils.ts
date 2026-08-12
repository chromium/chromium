// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * These values are persisted to logs and should not be renumbered or reused.
 * See tools/metrics/histograms/metadata/bluetooth/enums.xml.
 */
// LINT.IfChange(BluetoothUiSurface)
export enum BluetoothUiSurface {
  SETTINGS_DEVICE_LIST_SUBPAGE = 0,
  SETTINGS_DEVICE_DETAIL_SUBPAGE = 1,
  SETTINGS_PAIRING_DIALOG = 2,
  BLUETOOTH_QUICK_SETTINGS = 3,
  PAIRING_DIALOG = 4,
  // [Deprecated] PAIRED_NOTIFICATION: 5,
  CONNECTION_TOAST = 6,
  DISCONNECTED_TOAST = 7,
  OOBE_HID_DETECTION = 8,
  PAIRED_TOAST = 9,
  COUNT = PAIRED_TOAST + 1,
}
// LINT.ThenChange(
//   //device/bluetooth/chromeos/bluetooth_utils.h:BluetoothUiSurface,
//   //tools/metrics/histograms/metadata/bluetooth/enums.xml:BluetoothUiSurface
// )

/**
 * Records metric indicating that |uiSurface| was displayed to the user.
 */
export function recordBluetoothUiSurfaceMetrics(uiSurface: BluetoothUiSurface): void {
  chrome.metricsPrivate.recordEnumerationValue(
      'Bluetooth.ChromeOS.UiSurfaceDisplayed', uiSurface,
      BluetoothUiSurface.COUNT);
}

/**
 * These values are persisted to logs and should not be renumbered or reused.
 * See tools/metrics/histograms/metadata/bluetooth/enums.xml.
 */
// LINT.IfChange(FastPairSavedDevicesUiEvent)
export enum FastPairSavedDevicesUiEvent {
  SETTINGS_SAVED_DEVICE_LIST_SUBPAGE_SHOWN = 0,
  SETTINGS_SAVED_DEVICE_LIST_HAS_DEVICES = 1,
  SETTINGS_SAVED_DEVICE_LIST_REMOVE_DIALOG = 2,
  SETTINGS_SAVED_DEVICE_LIST_REMOVE = 3,
  COUNT = SETTINGS_SAVED_DEVICE_LIST_REMOVE + 1,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/bluetooth/enums.xml:FastPairSavedDevicesUiEvent)

/**
 * Records metric indicating that |uiEvent| was displayed to the user.
 */
export function recordSavedDevicesUiEventMetrics(
    uiEvent: FastPairSavedDevicesUiEvent): void {
  chrome.metricsPrivate.recordEnumerationValue(
      'Bluetooth.ChromeOS.FastPair.SavedDevices.UiEvent', uiEvent,
      FastPairSavedDevicesUiEvent.COUNT);
}

