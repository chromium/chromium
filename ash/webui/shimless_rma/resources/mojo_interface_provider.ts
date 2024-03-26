// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CrosNetworkConfig, CrosNetworkConfigInterface as NetworkConfigServiceInterface} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';

import {fakeCalibrationComponentsWithFails, fakeChromeVersion, fakeComponents, fakeDeviceCustomLabels, fakeDeviceRegions, fakeDeviceSkuDescriptions, fakeDeviceSkus, fakeLog, fakeLogSavePath, fakeRsuChallengeCode, fakeRsuChallengeQrCode, fakeStates} from './fake_data.js';
import {FakeShimlessRmaService, FakeShimlessRmaServiceInterface} from './fake_shimless_rma_service.js';
import {CalibrationSetupInstruction, FeatureLevel, RmadErrorCode, ShimlessRmaService, ShimlessRmaServiceInterface, WriteProtectDisableCompleteAction} from './shimless_rma.mojom-webui.js';

/**
 * @fileoverview
 * Provides singleton access to (fake) mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * If true this will replace ShimlessRmaService with a fake.
 */
const useFakeService = false;

let shimlessRmaService: ShimlessRmaServiceInterface|null = null;

let networkConfigService: NetworkConfigServiceInterface|null = null;

export function populateFakeShimlessRmaService(
    service: FakeShimlessRmaServiceInterface): void {
  service.setStates(fakeStates);

  service.setAsyncOperationDelayMs(500);

  service.setAbortRmaResult(RmadErrorCode.kRmaNotRequired);

  service.enableAutomaticallyTriggerHardwareVerificationStatusObservation();

  service.setGetCurrentOsVersionResult(fakeChromeVersion[0]);
  service.setCheckForOsUpdatesResult('99.0.4844.74');
  service.setUpdateOsResult(true);
  service.enableAutomaticallyTriggerOsUpdateObservation();

  service.setGetComponentListResult(fakeComponents);
  service.enableAautomaticallyTriggerUpdateRoFirmwareObservation();
  service.enableAutomaticallyTriggerDisableWriteProtectionObservation();
  service.enableAutomaticallyTriggerCalibrationObservation();

  service.setGetRsuDisableWriteProtectChallengeResult(fakeRsuChallengeCode);
  service.setGetRsuDisableWriteProtectHwidResult('SAMUSTEST_2082');
  service.setGetRsuDisableWriteProtectChallengeQrCodeResponse(
      fakeRsuChallengeQrCode);

  service.setGetWriteProtectDisableCompleteAction(
      WriteProtectDisableCompleteAction.kCompleteAssembleDevice);

  service.setGetOriginalSerialNumberResult('serial# 0001');
  service.setGetRegionListResult(fakeDeviceRegions);
  service.setGetOriginalRegionResult(1);
  service.setGetSkuListResult(fakeDeviceSkus);
  service.setGetSkuDescriptionListResult(fakeDeviceSkuDescriptions);
  service.setGetOriginalSkuResult(1);
  service.setGetCustomLabelListResult(fakeDeviceCustomLabels);
  service.setGetOriginalCustomLabelResult(1);
  service.setGetOriginalDramPartNumberResult('dram# 0123');
  service.setGetOriginalFeatureLevelResult(
      FeatureLevel.kRmadFeatureLevelUnsupported);

  service.setGetCalibrationSetupInstructionsResult(
      CalibrationSetupInstruction.kCalibrationInstructionPlaceLidOnFlatSurface);
  service.setGetCalibrationComponentListResult(
      fakeCalibrationComponentsWithFails);

  service.enableAutomaticallyTriggerProvisioningObservation();
  service.enableAutomaticallyTriggerFinalizationObservation();

  service.enableAutomaticallyTriggerPowerCableStateObservation();
  service.setGetLogResult(fakeLog);
  service.setSaveLogResult({'path': fakeLogSavePath});
  service.setGetPowerwashRequiredResult(true);
}

// Sets up a FakeShimlessRmaService to be used at runtime.
function setupFakeShimlessRmaService(): void {
  // Create provider.
  const service = new FakeShimlessRmaService();
  populateFakeShimlessRmaService(service);

  // Set the fake service.
  setShimlessRmaServiceForTesting(service);
}

export function setShimlessRmaServiceForTesting(
    testService: ShimlessRmaServiceInterface): void {
  shimlessRmaService = testService;
}

export function getShimlessRmaService(): ShimlessRmaServiceInterface {
  if (!shimlessRmaService) {
    if (useFakeService) {
      setupFakeShimlessRmaService();
    } else {
      shimlessRmaService = ShimlessRmaService.getRemote();
    }
  }

  assert(!!shimlessRmaService);
  return shimlessRmaService;
}

export function setNetworkConfigServiceForTesting(
    testService: NetworkConfigServiceInterface): void {
  networkConfigService = testService;
}

export function getNetworkConfigService(): NetworkConfigServiceInterface {
  if (!networkConfigService) {
    networkConfigService = CrosNetworkConfig.getRemote();
  }

  assert(!!networkConfigService);
  return networkConfigService;
}
