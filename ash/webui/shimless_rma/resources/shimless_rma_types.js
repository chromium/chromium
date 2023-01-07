// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 */

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import './file_path.mojom-lite.js';
import './mojom/shimless_rma.mojom-lite.js';

import {CrosNetworkConfigInterface, CrosNetworkConfigRemote, NetworkStateProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';

/**
 * @typedef {ash.shimlessRma.mojom.StateResult}
 */
export const StateResult = ash.shimlessRma.mojom.StateResult;

/**
 * @typedef {ash.shimlessRma.mojom.State}
 */
export const State = ash.shimlessRma.mojom.State;

/**
 * @typedef {ash.shimlessRma.mojom.RmadErrorCode}
 */
export const RmadErrorCode = ash.shimlessRma.mojom.RmadErrorCode;

/**
 * @typedef {ash.shimlessRma.mojom.QrCode}
 */
export const QrCode = ash.shimlessRma.mojom.QrCode;

/**
 * @typedef {ash.shimlessRma.mojom.ComponentType}
 */
export const ComponentType = ash.shimlessRma.mojom.ComponentType;

/**
 * @typedef {ash.shimlessRma.mojom.ComponentRepairStatus}
 */
export const ComponentRepairStatus =
    ash.shimlessRma.mojom.ComponentRepairStatus;

/** @typedef {ash.shimlessRma.mojom.WriteProtectDisableCompleteAction} */
export const WriteProtectDisableCompleteAction =
    ash.shimlessRma.mojom.WriteProtectDisableCompleteAction;

/** @typedef {ash.shimlessRma.mojom.UpdateRoFirmwareStatus} */
export const UpdateRoFirmwareStatus =
    ash.shimlessRma.mojom.UpdateRoFirmwareStatus;

/**
 * @typedef {ash.shimlessRma.mojom.CalibrationSetupInstruction}
 */
export const CalibrationSetupInstruction =
    ash.shimlessRma.mojom.CalibrationSetupInstruction;

/**
 * @typedef {ash.shimlessRma.mojom.CalibrationOverallStatus}
 */
export const CalibrationOverallStatus =
    ash.shimlessRma.mojom.CalibrationOverallStatus;

/**
 * @typedef {ash.shimlessRma.mojom.CalibrationStatus}
 */
export const CalibrationStatus = ash.shimlessRma.mojom.CalibrationStatus;

/**
 * @typedef {ash.shimlessRma.mojom.CalibrationComponentStatus}
 */
export const CalibrationComponentStatus =
    ash.shimlessRma.mojom.CalibrationComponentStatus;

/**
 * @typedef {ash.shimlessRma.mojom.ProvisioningStatus}
 */
export const ProvisioningStatus = ash.shimlessRma.mojom.ProvisioningStatus;

/**
 * @typedef {ash.shimlessRma.mojom.ProvisioningError}
 */
export const ProvisioningError = ash.shimlessRma.mojom.ProvisioningError;

/**
 * @typedef {ash.shimlessRma.mojom.FinalizationStatus}
 */
export const FinalizationStatus = ash.shimlessRma.mojom.FinalizationStatus;

/**
 * @typedef {ash.shimlessRma.mojom.FinalizationError}
 */
export const FinalizationError = ash.shimlessRma.mojom.FinalizationError;

/**
 * Type alias for OsUpdateOperation.
 * @typedef {ash.shimlessRma.mojom.OsUpdateOperation}
 */
export const OsUpdateOperation = ash.shimlessRma.mojom.OsUpdateOperation;

/**
 * Type alias for UpdateErrorCode.
 * @typedef {ash.shimlessRma.mojom.UpdateErrorCode}
 */
export const UpdateErrorCode = ash.shimlessRma.mojom.UpdateErrorCode;

/**
 * @typedef {ash.shimlessRma.mojom.Component}
 */
export const Component = ash.shimlessRma.mojom.Component;

/**
 * Type alias for ErrorObserverRemote.
 * @typedef {ash.shimlessRma.mojom.ErrorObserverRemote}
 */
export const ErrorObserverRemote = ash.shimlessRma.mojom.ErrorObserverRemote;

/**
 * Type alias for ErrorObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.ErrorObserverReceiver}
 */
export const ErrorObserverReceiver =
    ash.shimlessRma.mojom.ErrorObserverReceiver;

/**
 * Type alias for ErrorObserverInterface.
 * @typedef {ash.shimlessRma.mojom.ErrorObserverInterface}
 */
export const ErrorObserverInterface =
    ash.shimlessRma.mojom.ErrorObserverInterface;

/**
 * Type alias for OsUpdateObserverRemote.
 * @typedef {ash.shimlessRma.mojom.OsUpdateObserverRemote}
 */
export const OsUpdateObserverRemote =
    ash.shimlessRma.mojom.OsUpdateObserverRemote;

/**
 * Type alias for OsUpdateObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.OsUpdateObserverReceiver}
 */
export const OsUpdateObserverReceiver =
    ash.shimlessRma.mojom.OsUpdateObserverReceiver;

/**
 * Type alias for OsUpdateObserverInterface.
 * @typedef {ash.shimlessRma.mojom.OsUpdateObserverInterface}
 */
export const OsUpdateObserverInterface =
    ash.shimlessRma.mojom.OsUpdateObserverInterface;

/**
 * Type alias for UpdateRoFirmwareObserverRemote.
 * @typedef {ash.shimlessRma.mojom.UpdateRoFirmwareObserverRemote}
 */
export const UpdateRoFirmwareObserverRemote =
    ash.shimlessRma.mojom.UpdateRoFirmwareObserverRemote;

/**
 * Type alias for UpdateRoFirmwareObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.UpdateRoFirmwareObserverReceiver}
 */
export const UpdateRoFirmwareObserverReceiver =
    ash.shimlessRma.mojom.UpdateRoFirmwareObserverReceiver;

/**
 * Type alias for UpdateRoFirmwareObserverInterface.
 * @typedef {ash.shimlessRma.mojom.UpdateRoFirmwareObserverInterface}
 */
export const UpdateRoFirmwareObserverInterface =
    ash.shimlessRma.mojom.UpdateRoFirmwareObserverInterface;

/**
 * Type alias for CalibrationObserverRemote.
 * @typedef {ash.shimlessRma.mojom.CalibrationObserverRemote}
 */
export const CalibrationObserverRemote =
    ash.shimlessRma.mojom.CalibrationObserverRemote;

/**
 * Type alias for CalibrationObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.CalibrationObserverReceiver}
 */
export const CalibrationObserverReceiver =
    ash.shimlessRma.mojom.CalibrationObserverReceiver;

/**
 * Type alias for CalibrationObserverInterface.
 * @typedef {ash.shimlessRma.mojom.CalibrationObserverInterface}
 */
export const CalibrationObserverInterface =
    ash.shimlessRma.mojom.CalibrationObserverInterface;

/**
 * Type alias for ProvisioningObserverRemote.
 * @typedef {ash.shimlessRma.mojom.ProvisioningObserverRemote}
 */
export const ProvisioningObserverRemote =
    ash.shimlessRma.mojom.ProvisioningObserverRemote;

/**
 * Type alias for ProvisioningObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.ProvisioningObserverReceiver}
 */
export const ProvisioningObserverReceiver =
    ash.shimlessRma.mojom.ProvisioningObserverReceiver;

/**
 * Type alias for ProvisioningObserverInterface.
 * @typedef {ash.shimlessRma.mojom.ProvisioningObserverInterface}
 */
export const ProvisioningObserverInterface =
    ash.shimlessRma.mojom.ProvisioningObserverInterface;

/**
 * Type alias for HardwareWriteProtectionStateObserverRemote.
 * @typedef {ash.shimlessRma.mojom.HardwareWriteProtectionStateObserverRemote}
 */
export const HardwareWriteProtectionStateObserverRemote =
    ash.shimlessRma.mojom.HardwareWriteProtectionStateObserverRemote;

/**
 * Type alias for HardwareWriteProtectionStateObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.HardwareWriteProtectionStateObserverReceiver}
 */
export const HardwareWriteProtectionStateObserverReceiver =
    ash.shimlessRma.mojom.HardwareWriteProtectionStateObserverReceiver;

/**
 * Type alias for HardwareWriteProtectionStateObserverInterface.
 * @typedef {
 *    ash.shimlessRma.mojom.HardwareWriteProtectionStateObserverInterface
 * }
 */
export const HardwareWriteProtectionStateObserverInterface =
    ash.shimlessRma.mojom.HardwareWriteProtectionStateObserverInterface;

/**
 * Type alias for PowerCableStateObserverRemote.
 * @typedef {ash.shimlessRma.mojom.PowerCableStateObserverRemote}
 */
export const PowerCableStateObserverRemote =
    ash.shimlessRma.mojom.PowerCableStateObserverRemote;

/**
 * Type alias for PowerCableStateObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.PowerCableStateObserverReceiver}
 */
export const PowerCableStateObserverReceiver =
    ash.shimlessRma.mojom.PowerCableStateObserverReceiver;

/**
 * Type alias for PowerCableStateObserverInterface.
 * @typedef {ash.shimlessRma.mojom.PowerCableStateObserverInterface}
 */
export const PowerCableStateObserverInterface =
    ash.shimlessRma.mojom.PowerCableStateObserverInterface;

/**
 * Type alias for ExternalDiskStateObserverRemote.
 * @typedef {ash.shimlessRma.mojom.ExternalDiskStateObserverRemote}
 */
export const ExternalDiskStateObserverRemote =
    ash.shimlessRma.mojom.ExternalDiskStateObserverRemote;

/**
 * Type alias for ExternalDiskStateObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.ExternalDiskStateObserverReceiver}
 */
export const ExternalDiskStateObserverReceiver =
    ash.shimlessRma.mojom.ExternalDiskStateObserverReceiver;

/**
 * Type alias for ExternalDiskStateObserverInterface.
 * @typedef {ash.shimlessRma.mojom.ExternalDiskStateObserverInterface}
 */
export const ExternalDiskStateObserverInterface =
    ash.shimlessRma.mojom.ExternalDiskStateObserverInterface;

/**
 * Type alias for HardwareVerificationStatusObserverRemote.
 * @typedef {ash.shimlessRma.mojom.HardwareVerificationStatusObserverRemote}
 */
export const HardwareVerificationStatusObserverRemote =
    ash.shimlessRma.mojom.HardwareVerificationStatusObserverRemote;

/**
 * Type alias for HardwareVerificationStatusObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.HardwareVerificationStatusObserverReceiver}
 */
export const HardwareVerificationStatusObserverReceiver =
    ash.shimlessRma.mojom.HardwareVerificationStatusObserverReceiver;

/**
 * Type alias for HardwareVerificationStatusObserverInterface.
 * @typedef {
 *    ash.shimlessRma.mojom.HardwareVerificationStatusObserverInterface
 * }
 */
export const HardwareVerificationStatusObserverInterface =
    ash.shimlessRma.mojom.HardwareVerificationStatusObserverInterface;

/**
 * Type alias for FinalizationObserverRemote.
 * @typedef {ash.shimlessRma.mojom.FinalizationObserverRemote}
 */
export const FinalizationObserverRemote =
    ash.shimlessRma.mojom.FinalizationObserverRemote;

/**
 * Type alias for FinalizationObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.FinalizationObserverReceiver}
 */
export const FinalizationObserverReceiver =
    ash.shimlessRma.mojom.FinalizationObserverReceiver;

/**
 * Type alias for FinalizationObserverInterface.
 * @typedef {
 *    ash.shimlessRma.mojom.FinalizationObserverInterface
 * }
 */
export const FinalizationObserverInterface =
    ash.shimlessRma.mojom.FinalizationObserverInterface;

/**
 * Type alias for the ShimlessRmaService.
 * @typedef {ash.shimlessRma.mojom.ShimlessRmaService}
 */
export const ShimlessRmaService = ash.shimlessRma.mojom.ShimlessRmaService;

/**
 * Type alias for the ShimlessRmaServiceInterface.
 * @typedef {ash.shimlessRma.mojom.ShimlessRmaServiceInterface}
 */
export const ShimlessRmaServiceInterface =
    ash.shimlessRma.mojom.ShimlessRmaServiceInterface;

/**
 * Type alias for NetworkConfigServiceInterface.
 * @typedef {CrosNetworkConfigInterface}
 */
export const NetworkConfigServiceInterface = CrosNetworkConfigInterface;

/**
 * Type alias for NetworkConfigServiceRemote.
 * @typedef {CrosNetworkConfigRemote}
 */
export const NetworkConfigServiceRemote = CrosNetworkConfigRemote;

/**
 * Type alias for Network
 * @typedef {NetworkStateProperties}
 */
export const Network = NetworkStateProperties;

/**
 * Type alias for the ShutdownMethod.
 * @typedef {ash.shimlessRma.mojom.ShutdownMethod}
 */
export const ShutdownMethod = ash.shimlessRma.mojom.ShutdownMethod;

/**
 * @typedef {{savePath: mojoBase.mojom.FilePath, error: RmadErrorCode}}
 */
export let SaveLogResponse;
