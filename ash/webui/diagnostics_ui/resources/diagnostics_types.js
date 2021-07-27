// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 */

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import './input_data_provider.mojom-lite.js';
import './network_health_provider.mojom-lite.js'
import './system_data_provider.mojom-lite.js';
import './system_routine_controller.mojom-lite.js';

/**
 * Type alias for the SystemDataProvider.
 * @typedef {ash.diagnostics.mojom.SystemDataProvider}
 */
export let SystemDataProvider = ash.diagnostics.mojom.SystemDataProvider;

/**
 * Type alias for the SystemDataProviderInterface.
 * @typedef {ash.diagnostics.mojom.SystemDataProviderInterface}
 */
export let SystemDataProviderInterface =
    ash.diagnostics.mojom.SystemDataProviderInterface;

/**
 * Type alias for DeviceCapabilities.
 * @typedef {ash.diagnostics.mojom.DeviceCapabilities}
 */
export let DeviceCapabilities = ash.diagnostics.mojom.DeviceCapabilities;

/**
 * Type alias for VersionInfo.
 * @typedef {ash.diagnostics.mojom.VersionInfo}
 */
export let VersionInfo = ash.diagnostics.mojom.VersionInfo;

/**
 * Type alias for SystemInfo.
 * @typedef {ash.diagnostics.mojom.SystemInfo}
 */
export let SystemInfo = ash.diagnostics.mojom.SystemInfo;

/**
 * Type alias for ExternalPowerSource.
 * @typedef {ash.diagnostics.mojom.ExternalPowerSource}
 */
export let ExternalPowerSource = ash.diagnostics.mojom.ExternalPowerSource;

/**
 * Type alias for BatteryState.
 * @typedef {ash.diagnostics.mojom.BatteryState}
 */
export let BatteryState = ash.diagnostics.mojom.BatteryState;

/**
 * Type alias for BatteryInfo.
 * @typedef {ash.diagnostics.mojom.BatteryInfo}
 */
export let BatteryInfo = ash.diagnostics.mojom.BatteryInfo;

/**
 * Type alias for BatteryChargeStatusObserver.
 * @typedef {ash.diagnostics.mojom.BatteryChargeStatusObserver}
 */
export let BatteryChargeStatusObserver =
    ash.diagnostics.mojom.BatteryChargeStatusObserver;

/**
 * Type alias for BatteryChargeStatusObserverRemote.
 * @typedef {ash.diagnostics.mojom.BatteryChargeStatusObserverRemote}
 */
export let BatteryChargeStatusObserverRemote =
    ash.diagnostics.mojom.BatteryChargeStatusObserverRemote;

/**
 * Type alias for BatteryChargeStatusObserverInterface.
 * @typedef {ash.diagnostics.mojom.BatteryChargeStatusObserverInterface}
 */
export let BatteryChargeStatusObserverInterface =
    ash.diagnostics.mojom.BatteryChargeStatusObserverInterface;

/**
 * Type alias for BatteryChargeStatusObserverReceiver.
 * @typedef {ash.diagnostics.mojom.BatteryChargeStatusObserverReceiver}
 */
export let BatteryChargeStatusObserverReceiver =
    ash.diagnostics.mojom.BatteryChargeStatusObserverReceiver;

/**
 * Type alias for BatteryChargeStatus.
 * @typedef {ash.diagnostics.mojom.BatteryChargeStatus}
 */
export let BatteryChargeStatus = ash.diagnostics.mojom.BatteryChargeStatus;

/**
 * Type alias for BatteryHealthObserver.
 * @typedef {ash.diagnostics.mojom.BatteryHealthObserver}
 */
export let BatteryHealthObserver =
    ash.diagnostics.mojom.BatteryHealthObserver;

/**
 * Type alias for BatteryHealthObserver.
 * @typedef {ash.diagnostics.mojom.BatteryHealthObserverRemote}
 */
export let BatteryHealthObserverRemote =
    ash.diagnostics.mojom.BatteryHealthObserverRemote;

/**
 * Type alias for BatteryHealthObserverInterface.
 * @typedef {ash.diagnostics.mojom.BatteryHealthObserverInterface}
 */
export let BatteryHealthObserverInterface =
    ash.diagnostics.mojom.BatteryHealthObserverInterface;

/**
 * Type alias for BatteryHealthObserverReceiver.
 * @typedef {ash.diagnostics.mojom.BatteryHealthObserverReceiver}
 */
export let BatteryHealthObserverReceiver =
    ash.diagnostics.mojom.BatteryHealthObserverReceiver;

/**
 * Type alias for BatteryHealth.
 * @typedef {ash.diagnostics.mojom.BatteryHealth}
 */
export let BatteryHealth = ash.diagnostics.mojom.BatteryHealth;

/**
 * Type alias for MemoryUsageObserver.
 * @typedef {ash.diagnostics.mojom.MemoryUsageObserver}
 */
export let MemoryUsageObserver = ash.diagnostics.mojom.MemoryUsageObserver;

/**
 * Type alias for MemoryUsageObserverRemote.
 * @typedef {ash.diagnostics.mojom.MemoryUsageObserverRemote}
 */
export let MemoryUsageObserverRemote =
    ash.diagnostics.mojom.MemoryUsageObserverRemote;

/**
 * Type alias for MemoryUsageObserverInterface.
 * @typedef {ash.diagnostics.mojom.MemoryUsageObserverInterface}
 */
export let MemoryUsageObserverInterface =
    ash.diagnostics.mojom.MemoryUsageObserverInterface;

/**
 * Type alias for MemoryUsageObserverReceiver.
 * @typedef {ash.diagnostics.mojom.MemoryUsageObserverReceiver}
 */
export let MemoryUsageObserverReceiver =
    ash.diagnostics.mojom.MemoryUsageObserverReceiver;

/**
 * Type alias for MemoryUsage.
 * @typedef {ash.diagnostics.mojom.MemoryUsage}
 */
export let MemoryUsage = ash.diagnostics.mojom.MemoryUsage;

/**
 * Type alias for CpuUsageObserver.
 * @typedef {ash.diagnostics.mojom.CpuUsageObserver}
 */
export let CpuUsageObserver = ash.diagnostics.mojom.CpuUsageObserver;

/**
 * Type alias for CpuUsageObserverRemote.
 * @typedef {ash.diagnostics.mojom.CpuUsageObserverRemote}
 */
export let CpuUsageObserverRemote =
    ash.diagnostics.mojom.CpuUsageObserverRemote;

/**
 * Type alias for CpuUsageObserverInterface.
 * @typedef {ash.diagnostics.mojom.CpuUsageObserverInterface}
 */
export let CpuUsageObserverInterface =
    ash.diagnostics.mojom.CpuUsageObserverInterface;

/**
 * Type alias for CpuUsageObserverReceiver.
 * @typedef {ash.diagnostics.mojom.CpuUsageObserverReceiver}
 */
export let CpuUsageObserverReceiver =
    ash.diagnostics.mojom.CpuUsageObserverReceiver;

/**
 * Type alias for CpuUsage.
 * @typedef {ash.diagnostics.mojom.CpuUsage}
 */
export let CpuUsage = ash.diagnostics.mojom.CpuUsage;

/**
 * Enumeration of routines.
 * @typedef {ash.diagnostics.mojom.RoutineType}
 */
export let RoutineType = ash.diagnostics.mojom.RoutineType;

/**
 * Type alias for StandardRoutineResult.
 * @typedef {ash.diagnostics.mojom.StandardRoutineResult}
 */
export let StandardRoutineResult =
    ash.diagnostics.mojom.StandardRoutineResult;

/**
 * Type alias for PowerRoutineResult.
 * @typedef {ash.diagnostics.mojom.PowerRoutineResult}
 */
export let PowerRoutineResult = ash.diagnostics.mojom.PowerRoutineResult;

/**
 * Type alias for RoutineResult.
 * @typedef {ash.diagnostics.mojom.RoutineResult}
 */
export let RoutineResult = ash.diagnostics.mojom.RoutineResult;

/**
 * Type alias for RoutineResultInfo.
 * @typedef {ash.diagnostics.mojom.RoutineResultInfo}
 */
export let RoutineResultInfo = ash.diagnostics.mojom.RoutineResultInfo;

/**
 * Type alias for RoutineRunnerInterface.
 * @typedef {ash.diagnostics.mojom.RoutineRunnerInterface}
 */
export let RoutineRunnerInterface =
    ash.diagnostics.mojom.RoutineRunnerInterface;

/**
 * Type alias for RoutineRunnerRemote.
 * @typedef {ash.diagnostics.mojom.RoutineRunnerRemote}
 */
export let RoutineRunnerRemote = ash.diagnostics.mojom.RoutineRunnerRemote;

/**
 * Type alias for RoutineRunnerReceiver.
 * @typedef {ash.diagnostics.mojom.RoutineRunnerReceiver}
 */
export let RoutineRunnerReceiver =
    ash.diagnostics.mojom.RoutineRunnerReceiver;

/**
 * Type alias for SystemRoutineController.
 * @typedef {ash.diagnostics.mojom.SystemRoutineController}
 */
export let SystemRoutineController =
    ash.diagnostics.mojom.SystemRoutineController;

/**
 * Type alias for SystemRoutineControllerInterface.
 * @typedef {ash.diagnostics.mojom.SystemRoutineControllerInterface}
 */
export let SystemRoutineControllerInterface =
    ash.diagnostics.mojom.SystemRoutineControllerInterface;

/**
 * Type alias for NetworkListObserver.
 * @typedef {ash.diagnostics.mojom.NetworkListObserverRemote}
 */
export let NetworkListObserverRemote =
    ash.diagnostics.mojom.NetworkListObserverRemote;

/**
 * Type alias for NetworkStateObserver.
 * @typedef {ash.diagnostics.mojom.NetworkStateObserverRemote}
 */
export let NetworkStateObserverRemote =
    ash.diagnostics.mojom.NetworkStateObserverRemote;

/**
 * Type alias for Network.
 * @typedef {ash.diagnostics.mojom.Network}
 */
export let Network = ash.diagnostics.mojom.Network;

/**
 * Type alias for NetworkHealthProvider.
 * @typedef {ash.diagnostics.mojom.NetworkHealthProvider}
 */
export let NetworkHealthProvider =
    ash.diagnostics.mojom.NetworkHealthProvider;

/**
 * Type alias for NetworkHealthProviderInterface.
 * @typedef {ash.diagnostics.mojom.NetworkHealthProviderInterface}
 */
export let NetworkHealthProviderInterface =
    ash.diagnostics.mojom.NetworkHealthProviderInterface;

/**
 * Type alias for NetworkState.
 * @typedef {ash.diagnostics.mojom.NetworkState}
 */
export let NetworkState = ash.diagnostics.mojom.NetworkState;

/**
 * Type alias for NetworkType
 * @typedef {ash.diagnostics.mojom.NetworkType}
 */
export let NetworkType = ash.diagnostics.mojom.NetworkType;

/**
 * Type alias for NetworkListObserverReceiver.
 * @typedef {ash.diagnostics.mojom.NetworkListObserverReceiver}
 */
export let NetworkListObserverReceiver =
    ash.diagnostics.mojom.NetworkListObserverReceiver;

/**
 * Type alias for NetworkListObserverInterface.
 * @typedef {ash.diagnostics.mojom.NetworkListObserverInterface}
 */
export let NetworkListObserverInterface =
    ash.diagnostics.mojom.NetworkListObserverInterface;

/**
 * Type alias for NetworkStateObserverInterface.
 * @typedef {ash.diagnostics.mojom.NetworkStateObserverInterface}
 */
export let NetworkStateObserverInterface =
    ash.diagnostics.mojom.NetworkStateObserverInterface;

/**
 * Type alias for NetworkStateObserverReceiver.
 * @typedef {ash.diagnostics.mojom.NetworkStateObserverReceiver}
 */
export let NetworkStateObserverReceiver =
    ash.diagnostics.mojom.NetworkStateObserverReceiver;

/**
 * @typedef {{
 *   networkGuids: !Array<string>,
 *   activeGuid: string,
 * }}
 */
export let NetworkGuidInfo;

/**
 * Type alias for WiFiStateProperties.
 * @typedef {ash.diagnostics.mojom.WiFiStateProperties}
 */
export let WiFiStateProperties = ash.diagnostics.mojom.WiFiStateProperties;

/**
 * Type alias for ConnectionType.
 * @typedef {ash.diagnostics.mojom.ConnectionType}
 */
export let ConnectionType = ash.diagnostics.mojom.ConnectionType;

/**
 * Type alias for PhysicalLayout.
 * @typedef {ash.diagnostics.mojom.PhysicalLayout}
 */
export let PhysicalLayout = ash.diagnostics.mojom.PhysicalLayout;

/**
 * Type alias for MechanicalLayout.
 * @typedef {ash.diagnostics.mojom.MechanicalLayout}
 */
export let MechanicalLayout = ash.diagnostics.mojom.MechanicalLayout;

/**
 * Type alias for KeyboardInfo.
 * @typedef {ash.diagnostics.mojom.KeyboardInfo}
 */
export let KeyboardInfo = ash.diagnostics.mojom.KeyboardInfo;

/**
 * Type alias for TouchDeviceType.
 * @typedef {ash.diagnostics.mojom.TouchDeviceType}
 */
export let TouchDeviceType = ash.diagnostics.mojom.TouchDeviceType;

/**
 * Type alias for TouchDeviceInfo.
 * @typedef {ash.diagnostics.mojom.TouchDeviceInfo}
 */
export let TouchDeviceInfo = ash.diagnostics.mojom.TouchDeviceInfo;

/**
 * Type alias for ConnectedDevicesObserver.
 * @typedef {ash.diagnostics.mojom.ConnectedDevicesObserver}
 */
export let ConnectedDevicesObserver = ash.diagnostics.mojom.CpuUsageObserver;

/**
 * Type alias for ConnectedDevicesObserverRemote.
 * @typedef {ash.diagnostics.mojom.ConnectedDevicesObserverRemote}
 */
export let ConnectedDevicesObserverRemote =
    ash.diagnostics.mojom.ConnectedDevicesObserverRemote;

/**
 * Type alias for ConnectedDevicesObserverInterface.
 * @typedef {ash.diagnostics.mojom.ConnectedDevicesObserverInterface}
 */
export let ConnectedDevicesObserverInterface =
    ash.diagnostics.mojom.ConnectedDevicesObserverInterface;

/**
 * Type alias for ConnectedDevicesObserverReceiver.
 * @typedef {ash.diagnostics.mojom.ConnectedDevicesObserverReceiver}
 */
export let ConnectedDevicesObserverReceiver =
    ash.diagnostics.mojom.ConnectedDevicesObserverReceiver;


/**
 * Type alias for the the response from InputDataProvider.GetConnectedDevices.
 * @typedef {{keyboards: !Array<!KeyboardInfo>,
 *            touchDevices: !Array<!TouchDeviceInfo>}}
 */
export let GetConnectedDevicesResponse;

/**
 * Type alias for InputDataProviderInterface.
 * @typedef {ash.diagnostics.mojom.InputDataProviderInterface}
 */
export let InputDataProviderInterface =
    ash.diagnostics.mojom.InputDataProviderInterface;
