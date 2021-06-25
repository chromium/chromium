// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';

import {Component, ComponentRepairState, ComponentType, Network, RmadErrorCode, RmaState, StateResult} from './shimless_rma_types.js';

/** @type {!Array<!StateResult>} */
export const fakeStates = [
  {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
  {state: RmaState.kConfigureNetwork, error: RmadErrorCode.kOk},
  {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
  {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
  {state: RmaState.kChooseWriteProtectDisableMethod, error: RmadErrorCode.kOk},
  {state: RmaState.kWaitForManualWPDisable, error: RmadErrorCode.kOk},
  {state: RmaState.kEnterRSUWPDisableCode, error: RmadErrorCode.kOk},
  {state: RmaState.kWPDisableComplete, error: RmadErrorCode.kOk},
  {state: RmaState.kSelectComponents, error: RmadErrorCode.kOk},
  {state: RmaState.kChooseFirmwareReimageMethod, error: RmadErrorCode.kOk},
  {state: RmaState.kProvisionDevice, error: RmadErrorCode.kOk},
];

/** @type {!Array<string>} */
export const fakeChromeVersion = [
  '89.0.1232.1',
  '92.0.999.0',
  '95.0.4444.123',
];

/** @type {!Array<!Component>} */
export const fakeComponents = [
  {component: ComponentType.kKeyboard, state: ComponentRepairState.kOriginal},
  {component: ComponentType.kThumbReader, state: ComponentRepairState.kMissing},
  {component: ComponentType.kTrackpad, state: ComponentRepairState.kOriginal},
];

// onboarding_select_components_page_test needs a components list covering all
// possible repair states.
/** @type {!Array<!Component>} */
export const fakeComponentsForRepairStateTest = [
  {component: ComponentType.kKeyboard, state: ComponentRepairState.kOriginal},
  {component: ComponentType.kThumbReader, state: ComponentRepairState.kMissing},
  {component: ComponentType.kTrackpad, state: ComponentRepairState.kReplaced},
];

/** @type {!Array<!Network>} */
export const fakeNetworks = [
  OncMojo.getDefaultNetworkState(
      chromeos.networkConfig.mojom.NetworkType.kWiFi, 'wifi0'),
];
