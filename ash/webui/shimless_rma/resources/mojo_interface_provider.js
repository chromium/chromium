// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {fakeChromeVersion, fakeComponents, fakeDeviceRegions, fakeDeviceSkus, fakeStates} from './fake_data.js';
import {FakeShimlessRmaService} from './fake_shimless_rma_service.js'
import {Component, ComponentRepairState, ComponentType, NetworkConfigServiceInterface, RmadErrorCode, RmaState, ShimlessRmaService, ShimlessRmaServiceInterface} from './shimless_rma_types.js';

/**
 * @fileoverview
 * Provides singleton access to (fake) mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * If true this will replace ShimlessRmaService with a fake.
 * @type {boolean}
 */
let useFakeService = true;

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
  let service = new FakeShimlessRmaService();

  service.setStates(fakeStates);

  service.setGetCurrentChromeVersionResult(fakeChromeVersion[0]);
  service.setCheckForChromeUpdatesResult(false);

  service.setGetComponentListResult(fakeComponents);
  service.setReimageRequiredResult(false);
  service.automaticallyTriggerDisableWriteProtectionObservation();
  service.automaticallyTriggerProvisioningObservation();
  service.automaticallyTriggerCalibrationObservation();

  service.setGetOriginalSerialNumberResult('serial# 0001')
  service.setGetRegionListResult(fakeDeviceRegions);
  service.setGetOriginalRegionResult(1);
  service.setGetSkuListResult(fakeDeviceSkus);
  service.setGetOriginalSkuResult(1);

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
