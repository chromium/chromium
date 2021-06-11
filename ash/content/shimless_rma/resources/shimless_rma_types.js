// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 *
 * TODO(gavindodd): When the fake API is replaced by mojo these can be
 * re-aliased to the corresponding mojo types, or replaced by them.
 */

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';

/**
 * Return type from state progression methods.
 * Convenience type as mojo-lite does not define types for method results and
 * this is used frequently.
 * @typedef {{state: !RmaState, error: !RmadErrorCode}}
 */
export let StateResult;

/**
 * @enum {number}
 */
export let RmaState = {
  kUnknown: 0,
  kNotInRma: 1,
  kWelcomeScreen: 2,
  kConfigureNetwork: 3,
  kUpdateChrome: 4,
  kSelectComponents: 5,
  kChooseDestination: 6,
  kChooseWriteProtectDisableMethod: 7,
  kWaitForManualWPDisable: 8,
  kEnterRSUWPDisableCode: 9,
  kChooseFirmwareReimageMethod: 10,
  kUpdateDeviceInformation: 11,
  kCalibrateComponents: 12,
  kProvisionDevice: 13,
  kWaitForManualWPEnable: 14,
  kRepairComplete: 15,
  MIN_VALUE: 0,
  MAX_VALUE: 15,
};

/**
 * @enum {number}
 */
export let RmadErrorCode = {
  kNotSet: 0,
  kOk: 1,
  kRmaNotRequired: 2,
  kRequestInvalid: 3,
  kMissingComponent: 4,
  kWriteProtectDisableRsuCodeInvalid: 5,
  kWriteProtectDisableBatteryNotDisconnected: 6,
  kWriteProtectSignalNotDetected: 7,
  kReimagingDownloadNoNetwork: 8,
  kReimagingDownloadNetworkError: 9,
  kReimagingDownloadCancelled: 10,
  kReimagingUsbNotFound: 11,
  kReimagingUsbTooManyFound: 12,
  kReimagingUsbInvalidImage: 13,
  kReimagingImagingFailed: 14,
  kReimagingUnknownFailure: 15,
  kDeviceInfoInvalid: 16,
  kCalibrationFailed: 17,
  kProvisioningFailed: 18,
  kPowerwashFailed: 19,
  kFinalizationFailed: 20,
  kLogUploadFtpServerCannotConnect: 21,
  kLogUploadFtpServerConnectionRejected: 22,
  kLogUploadFtpServerTransferFailed: 23,
  kCannotCancelRma: 24,
  kTransitionFailed: 25,
  kAbortFailed: 26,
  MIN_VALUE: 0,
  MAX_VALUE: 26,
};


/**
 * @enum {number}
 */
export let ComponentType = {
  kComponentUnknown: 0,
  kMainboardRework: 1,
  kKeyboard: 2,
  kScreen: 3,
  kTrackpad: 4,
  kPowerButton: 5,
  kThumbReader: 6,
  MIN_VALUE: 0,
  MAX_VALUE: 6,
};


/**
 * @enum {number}
 */
export let ComponentRepairState = {
  kRepairUnknown: 0,
  kOriginal: 1,
  kReplaced: 2,
  kMissing: 3,
  MIN_VALUE: 0,
  MAX_VALUE: 3,
};


/**
 * @enum {number}
 */
export let CalibrationComponent = {
  kCalibrateUnknown: 0,
  kAccelerometer: 1,
  MIN_VALUE: 0,
  MAX_VALUE: 1,
};


/**
 * @enum {number}
 */
export let ProvisioningStep = {
  kProvisioningUnknown: 0,
  kInProgress: 1,
  kProvisioningComplete: 2,
  MIN_VALUE: 0,
  MAX_VALUE: 3,
};

/**
 * @typedef {{
 *   component: !ComponentType,
 *   state: !ComponentRepairState,
 * }}
 */
export let Component;

/**
 * Type alias for ErrorObserver.
 * @typedef {{onError: !function(!RmadErrorCode)}}
 */
export let ErrorObserverRemote;

/**
 * Type alias for CalibrationProgressObserver.
 * @typedef {{
 *   onCalibrationUpdated: !function(!CalibrationComponent, number)
 * }}
 */
export let CalibrationObserverRemote;

/**
 * Type alias for ProvisioningProgressObserver.
 * @typedef {{
 *   onProvisioningUpdated: !function(!ProvisioningStep, number)
 * }}
 */
export let ProvisioningObserverRemote;

/**
 * Type alias for HardwareWriteProtectionState.
 * @typedef {{
 *   onHardwareWriteProtectionStateChanged: !function(boolean)
 * }}
 */
export let HardwareWriteProtectionStateObserverRemote;

/**
 * Type alias for PowerCableState.
 * @typedef {{
 *   onPowerCableStateChanged: !function(boolean)
 * }}
 */
export let PowerCableStateObserverRemote;

/**
 * Type of ShimlessRmaServiceInterface.setStates function.
 * Only used to make the ShimlessRmaService type an interface.
 * TODO(gavindodd): Remove when a real mojo type is implemented.
 * @typedef {!function(!Array<{ state: !RmaState, error: !RmadErrorCode }>)}
 */
export let SetStatesFunction;


/**
 * Type alias for the ShimlessRmaServiceInterface.
 * TODO(gavindodd): Replace with a real mojo type when implemented.
 * @typedef {{
 *   setStates: !SetStatesFunction,
 * }}
 */
export let ShimlessRmaServiceInterface;

/**
 * Type alias for NetworkConfigServiceInterface.
 * @typedef {chromeos.networkConfig.mojom.CrosNetworkConfigInterface}
 */
export let NetworkConfigServiceInterface =
    chromeos.networkConfig.mojom.CrosNetworkConfigInterface;

/**
 * Type alias for NetworkConfigServiceRemote.
 * @typedef {chromeos.networkConfig.mojom.CrosNetworkConfigRemote}
 */
export let NetworkConfigServiceRemote =
    chromeos.networkConfig.mojom.CrosNetworkConfigRemote;

/**
 * Type alias for Network
 * @typedef {chromeos.networkConfig.mojom.NetworkStateProperties}
 */
export let Network = chromeos.networkConfig.mojom.NetworkStateProperties;
