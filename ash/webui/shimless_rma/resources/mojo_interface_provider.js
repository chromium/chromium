// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {fakeCalibrationComponents, fakeChromeVersion, fakeComponents, fakeDeviceRegions, fakeDeviceSkus, fakeDeviceWhiteLabels, fakeLog, fakeRsuChallengeCode, fakeRsuChallengeQrCode, fakeStates} from './fake_data.js';
import {FakeShimlessRmaService} from './fake_shimless_rma_service.js';
import {CalibrationSetupInstruction, NetworkConfigServiceInterface, RmadErrorCode, ShimlessRmaService, ShimlessRmaServiceInterface, WriteProtectDisableCompleteAction} from './shimless_rma_types.js';

/**
 * @fileoverview
 * Provides singleton access to (fake) mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * If true this will replace ShimlessRmaService with a fake.
 * @type {boolean}
 */
const useFakeService = false;

/**
 * @type {?ShimlessRmaServiceInterface}
 */
let shimlessRmaService = null;

/**
 * @type {?NetworkConfigServiceInterface}
 */
let networkConfigService = null;

/**
 * Sets up a FakeShimlessRmaService to be used at runtime.
 * TODO(gavindodd): Remove once mojo bindings are implemented.
 */
function setupFakeShimlessRmaService_() {
  // Create provider.
  const service = new FakeShimlessRmaService();

  service.setStates(fakeStates);

  service.setAsyncOperationDelayMs(500);

  service.setAbortRmaResult(RmadErrorCode.kRmaNotRequired);

  service.automaticallyTriggerHardwareVerificationStatusObservation();

  service.setGetCurrentOsVersionResult(fakeChromeVersion[0]);
  service.setCheckForOsUpdatesResult(true, 'fake version 1.2.3.4');
  service.setUpdateOsResult(false);
  service.automaticallyTriggerOsUpdateObservation();

  service.setGetComponentListResult(fakeComponents);
  service.automaticallyTriggerUpdateRoFirmwareObservation();
  service.automaticallyTriggerDisableWriteProtectionObservation();
  service.automaticallyTriggerCalibrationObservation();

  service.setGetRsuDisableWriteProtectChallengeResult(fakeRsuChallengeCode);
  service.setGetRsuDisableWriteProtectHwidResult('SAMUSTEST_2082');
  service.setGetRsuDisableWriteProtectChallengeQrCodeResponse(
      fakeRsuChallengeQrCode);

  service.setGetWriteProtectDisableCompleteAction(
      WriteProtectDisableCompleteAction.kCompleteAssembleDevice);

  service.setGetWriteProtectManuallyDisabledInstructionsResult(
      'g.co/help', fakeRsuChallengeQrCode);

  service.setGetOriginalSerialNumberResult('serial# 0001');
  service.setGetRegionListResult(fakeDeviceRegions);
  service.setGetOriginalRegionResult(1);
  service.setGetSkuListResult(fakeDeviceSkus);
  service.setGetOriginalSkuResult(1);
  service.setGetWhiteLabelListResult(fakeDeviceWhiteLabels);
  service.setGetOriginalWhiteLabelResult(1);

  service.setGetCalibrationSetupInstructionsResult(
      CalibrationSetupInstruction.kCalibrationInstructionPlaceLidOnFlatSurface);
  service.setGetCalibrationComponentListResult(fakeCalibrationComponents);

  service.automaticallyTriggerProvisioningObservation();
  service.automaticallyTriggerFinalizationObservation();

  service.automaticallyTriggerPowerCableStateObservation();
  service.setGetLogResult(fakeLog);

  // Set the fake service.
  setShimlessRmaServiceForTesting(service);
}

/**
 * @param {!ShimlessRmaServiceInterface} testService
 */
export function setShimlessRmaServiceForTesting(testService) {
  shimlessRmaService = testService;
}

/**
 * @return {!ShimlessRmaServiceInterface}
 */
export function getShimlessRmaService() {
  if (!shimlessRmaService) {
    if (useFakeService) {
      setupFakeShimlessRmaService_();
    } else {
      shimlessRmaService = ShimlessRmaService.getRemote();
    }
  }

  assert(!!shimlessRmaService);
  return shimlessRmaService;
}

/**
 * @param {!NetworkConfigServiceInterface} testService
 */
export function setNetworkConfigServiceForTesting(testService) {
  networkConfigService = testService;
}

/**
 * @return {!NetworkConfigServiceInterface}
 */
export function getNetworkConfigService() {
  if (!networkConfigService) {
    networkConfigService =
        chromeos.networkConfig.mojom.CrosNetworkConfig.getRemote();
  }

  assert(!!networkConfigService);
  return networkConfigService;
}

/**
 * @param {number} error
 * @return {string}
 */
export function rmadErrorString(error) {
  if (error === RmadErrorCode.kOk) {
    return '';
  }
  for (const [k, v] of Object.entries(RmadErrorCode)) {
    if (v === error) {
      return 'Error: ' + k + '(' + error + ')';
    }
  }
  return 'Error: unknown (' + error + ')';
}
