// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for metrics. Those values are logged to UMA. Entries should not be
// renumbered and numeric values should never be reused. Please keep in sync
// with "ChromeOS.OnDeviceControls.DialogAction" in
// src/tools/metrics/histograms/metadata/families/histograms.xml.
const AppParentalControlsDialogHistogramBaseName =
    'ChromeOS.OnDeviceControls.DialogAction.';

// Used for metrics. Those values are logged to UMA. Entries should not be
// renumbered and numeric values should never be reused. Please keep in sync
// with "ChromeOS.OnDeviceControls.PinDialogError" in
// src/tools/metrics/histograms/metadata/families/histograms.xml.
const AppParentalControlsPinDialogErrorHistogram =
    'ChromeOS.OnDeviceControls.PinDialogError';

export enum ParentalControlsDialogType {
  SET_UP_CONTROLS = 'SetUpControls',
  ENTER_SUBPAGE_VERIFICATION = 'VerifyToEnterControlsPage',
  DISABLE_CONTROLS_VERIFICATION = 'VerifyToDisableControls',
}

// Used for metrics. Those values are logged to UMA. Entries should not be
// renumbered and numeric values should never be reused. Please keep in sync
// with "OnDeviceControlsDialogAction" in
// src/tools/metrics/histograms/metadata/families/enums.xml.
export enum ParentalControlsDialogAction {
  OPEN_DIALOG = 0,
  FLOW_COMPLETED = 1,
}

// Used for metrics. Those values are logged to UMA. Entries should not be
// renumbered and numeric values should never be reused. Please keep in sync
// with "OnDeviceControlsPinDialogError" in
// src/tools/metrics/histograms/metadata/families/enums.xml.
export enum ParentalControlsPinDialogError {
  INVALID_PIN_ON_SETUP = 0,
  INCORRECT_PIN = 1,
  FORGOT_PIN = 2,
}

export function recordParentalControlsDialogOpened(
    dialogType: ParentalControlsDialogType): void {
  chrome.metricsPrivate.recordEnumerationValue(
      getDialogHistogramName(dialogType),
      ParentalControlsDialogAction.OPEN_DIALOG,
      getEnumLength(ParentalControlsDialogAction));
}

export function recordParentalControlsDialogFlowCompleted(
    dialogType: ParentalControlsDialogType): void {
  chrome.metricsPrivate.recordEnumerationValue(
      getDialogHistogramName(dialogType),
      ParentalControlsDialogAction.FLOW_COMPLETED,
      getEnumLength(ParentalControlsDialogAction));
}

export function recordPinDialogError(error: ParentalControlsPinDialogError):
    void {
  chrome.metricsPrivate.recordEnumerationValue(
      AppParentalControlsPinDialogErrorHistogram, error,
      getEnumLength(ParentalControlsPinDialogError));
}

function getDialogHistogramName(dialogType: ParentalControlsDialogType):
    string {
  return AppParentalControlsDialogHistogramBaseName.concat(dialogType);
}

function getEnumLength(histogramEnum: Object): number {
  return Object.keys(histogramEnum).filter((key: any) => isNaN(key)).length;
}
