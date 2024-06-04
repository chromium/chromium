// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for metrics. Those values are logged to UMA. Entries should not be
// renumbered and numeric values should never be reused. Please keep in sync
// with "ChromeOS.OnDeviceControls.DialogAction" in
// src/tools/metrics/histograms/metadata/families/histograms.xml.
const AppParentalControlsDialogHistogramBaseName =
    'ChromeOS.OnDeviceControls.DialogAction.';

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

export function recordParentalControlsDialogOpened(
    dialogType: ParentalControlsDialogType): void {
  chrome.metricsPrivate.recordEnumerationValue(
      getHistogramName(dialogType), ParentalControlsDialogAction.OPEN_DIALOG,
      Object.keys(ParentalControlsDialogAction).length);
}

export function recordParentalControlsDialogFlowCompleted(
    dialogType: ParentalControlsDialogType): void {
  chrome.metricsPrivate.recordEnumerationValue(
      getHistogramName(dialogType), ParentalControlsDialogAction.FLOW_COMPLETED,
      Object.keys(ParentalControlsDialogAction).length);
}

function getHistogramName(dialogType: ParentalControlsDialogType): string {
  return AppParentalControlsDialogHistogramBaseName.concat(dialogType);
}
