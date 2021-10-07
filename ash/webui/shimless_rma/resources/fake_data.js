// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';

import {CalibrationComponentStatus, CalibrationStatus, Component, ComponentRepairStatus, ComponentType, Network, QrCode, RmadErrorCode, RmaState, StateResult} from './shimless_rma_types.js';

/** @type {!Array<!StateResult>} */
export const fakeStates = [
  {
    state: RmaState.kWelcomeScreen,
    canCancel: true,
    canGoBack: false,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kConfigureNetwork,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kUpdateOs,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kSelectComponents,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kChooseDestination,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kChooseWriteProtectDisableMethod,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kEnterRSUWPDisableCode,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kVerifyRsu,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kWaitForManualWPDisable,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kWPDisableComplete,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kChooseFirmwareReimageMethod,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  // TODO(gavindodd): RmaState.kRestock
  {
    state: RmaState.kUpdateDeviceInformation,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kRestock,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kCheckCalibration,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kSetupCalibration,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kRunCalibration,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kProvisionDevice,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  // TODO(gavindodd): RmaState.kWaitForManualWPEnable
  {
    state: RmaState.kFinalize,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
  {
    state: RmaState.kRepairComplete,
    canCancel: true,
    canGoBack: true,
    error: RmadErrorCode.kOk
  },
];

/** @type {!Array<string>} */
export const fakeChromeVersion = [
  '89.0.1232.1',
  '92.0.999.0',
  '95.0.4444.123',
];

/** @type {string} */
export const fakeRsuChallengeCode =
    'HRBXHV84NSTHT25WJECYQKB8SARWFTMSWNGFT2FVEEPX69VE99USV3QFBEANDVXGQVL93QK2M6P3DNV4';

/** @type {!QrCode} */
export const fakeRsuChallengeQrCode = {
  size: 4,
  data: [0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0],
};

/** @type {!Array<!Component>} */
export const fakeComponents = [
  {component: ComponentType.kCamera, state: ComponentRepairStatus.kOriginal},
  {component: ComponentType.kBattery, state: ComponentRepairStatus.kMissing},
  {component: ComponentType.kTouchpad, state: ComponentRepairStatus.kOriginal},
];

// onboarding_select_components_page_test needs a components list covering all
// possible repair states.
/** @type {!Array<!Component>} */
export const fakeComponentsForRepairStateTest = [
  {component: ComponentType.kCamera, state: ComponentRepairStatus.kOriginal},
  {component: ComponentType.kBattery, state: ComponentRepairStatus.kMissing},
  {component: ComponentType.kTouchpad, state: ComponentRepairStatus.kReplaced},
];

/** @type {!Array<!CalibrationComponentStatus>} */
export const fakeCalibrationComponents = [
  {
    component: ComponentType.kCamera,
    status: CalibrationStatus.kCalibrationWaiting,
    progress: 0.0
  },
  {
    component: ComponentType.kBattery,
    status: CalibrationStatus.kCalibrationComplete,
    progress: 1.0
  },
  {
    component: ComponentType.kBaseAccelerometer,
    status: CalibrationStatus.kCalibrationInProgress,
    progress: 1.0
  },
  {
    component: ComponentType.kLidAccelerometer,
    status: CalibrationStatus.kCalibrationFailed,
    progress: 1.0
  },
  {
    component: ComponentType.kTouchpad,
    status: CalibrationStatus.kCalibrationSkip,
    progress: 0.0
  },
];

/** @type {!Array<!Network>} */
export const fakeNetworks = [
  OncMojo.getDefaultNetworkState(
      chromeos.networkConfig.mojom.NetworkType.kWiFi, 'wifi0'),
];

//** @type {!Array<string>} */
export const fakeDeviceRegions = ['EMEA', 'APAC', 'AMER'];

//** @type {!Array<string>} */
export const fakeDeviceSkus = ['SKU 1', 'SKU 2', 'SKU 3'];
