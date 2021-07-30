// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';

import {Component, ComponentRepairStatus, ComponentType, Network, QrCode, RmadErrorCode, RmaState, StateResult} from './shimless_rma_types.js';

/** @type {!Array<!StateResult>} */
export const fakeStates = [
  {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
  {state: RmaState.kConfigureNetwork, error: RmadErrorCode.kOk},
  {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
  {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
  {state: RmaState.kChooseWriteProtectDisableMethod, error: RmadErrorCode.kOk},
  {state: RmaState.kWaitForManualWPDisable, error: RmadErrorCode.kOk},
  {state: RmaState.kEnterRSUWPDisableCode, error: RmadErrorCode.kOk},
  {state: RmaState.kWPDisableComplete, error: RmadErrorCode.kOk},
  {state: RmaState.kSelectComponents, error: RmadErrorCode.kOk},
  {state: RmaState.kChooseFirmwareReimageMethod, error: RmadErrorCode.kOk},
  {state: RmaState.kUpdateDeviceInformation, error: RmadErrorCode.kOk},
  {state: RmaState.kProvisionDevice, error: RmadErrorCode.kOk},
];

/** @type {!Array<string>} */
export const fakeChromeVersion = [
  '89.0.1232.1',
  '92.0.999.0',
  '95.0.4444.123',
];

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

/** @type {!Array<!Network>} */
export const fakeNetworks = [
  OncMojo.getDefaultNetworkState(
      chromeos.networkConfig.mojom.NetworkType.kWiFi, 'wifi0'),
];

//** @type {!Array<string>} */
export const fakeDeviceRegions = ['EMEA', 'APAC', 'AMER'];

//** @type {!Array<string>} */
export const fakeDeviceSkus = ['SKU 1', 'SKU 2', 'SKU 3'];
