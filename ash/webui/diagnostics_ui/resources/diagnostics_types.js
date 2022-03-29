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
import './network_health_provider.mojom-lite.js';
import './system_data_provider.mojom-lite.js';
import './system_routine_controller.mojom-lite.js';

/**
 * Type alias for the SystemDataProvider.
 * @typedef {ash.diagnostics.mojom.SystemDataProvider}
 */
export const SystemDataProvider = ash.diagnostics.mojom.SystemDataProvider;

/**
 * Type alias for the SystemDataProviderInterface.
 * @typedef {ash.diagnostics.mojom.SystemDataProviderInterface}
 */
export const SystemDataProviderInterface =
    ash.diagnostics.mojom.SystemDataProviderInterface;

/**
 * Type alias for DeviceCapabilities.
 * @typedef {ash.diagnostics.mojom.DeviceCapabilities}
 */
export const DeviceCapabilities = ash.diagnostics.mojom.DeviceCapabilities;

/**
 * Type alias for VersionInfo.
 * @typedef {ash.diagnostics.mojom.VersionInfo}
 */
export const VersionInfo = ash.diagnostics.mojom.VersionInfo;

/**
 * Type alias for SystemInfo.
 * @typedef {ash.diagnostics.mojom.SystemInfo}
 */
export const SystemInfo = ash.diagnostics.mojom.SystemInfo;

/**
 * Type alias for ExternalPowerSource.
 * @typedef {ash.diagnostics.mojom.ExternalPowerSource}
 */
export const ExternalPowerSource = ash.diagnostics.mojom.ExternalPowerSource;

/**
 * Type alias for BatteryState.
 * @typedef {ash.diagnostics.mojom.BatteryState}
 */
export const BatteryState = ash.diagnostics.mojom.BatteryState;

/**
 * Type alias for BatteryInfo.
 * @typedef {ash.diagnostics.mojom.BatteryInfo}
 */
export const BatteryInfo = ash.diagnostics.mojom.BatteryInfo;

/**
 * Type alias for BatteryChargeStatusObserver.
 * @typedef {ash.diagnostics.mojom.BatteryChargeStatusObserver}
 */
export const BatteryChargeStatusObserver =
    ash.diagnostics.mojom.BatteryChargeStatusObserver;

/**
 * Type alias for BatteryChargeStatusObserverRemote.
 * @typedef {ash.diagnostics.mojom.BatteryChargeStatusObserverRemote}
 */
export const BatteryChargeStatusObserverRemote =
    ash.diagnostics.mojom.BatteryChargeStatusObserverRemote;

/**
 * Type alias for BatteryChargeStatusObserverInterface.
 * @typedef {ash.diagnostics.mojom.BatteryChargeStatusObserverInterface}
 */
export const BatteryChargeStatusObserverInterface =
    ash.diagnostics.mojom.BatteryChargeStatusObserverInterface;

/**
 * Type alias for BatteryChargeStatusObserverReceiver.
 * @typedef {ash.diagnostics.mojom.BatteryChargeStatusObserverReceiver}
 */
export const BatteryChargeStatusObserverReceiver =
    ash.diagnostics.mojom.BatteryChargeStatusObserverReceiver;

/**
 * Type alias for BatteryChargeStatus.
 * @typedef {ash.diagnostics.mojom.BatteryChargeStatus}
 */
export const BatteryChargeStatus = ash.diagnostics.mojom.BatteryChargeStatus;

/**
 * Type alias for BatteryHealthObserver.
 * @typedef {ash.diagnostics.mojom.BatteryHealthObserver}
 */
export const BatteryHealthObserver =
    ash.diagnostics.mojom.BatteryHealthObserver;

/**
 * Type alias for BatteryHealthObserver.
 * @typedef {ash.diagnostics.mojom.BatteryHealthObserverRemote}
 */
export const BatteryHealthObserverRemote =
    ash.diagnostics.mojom.BatteryHealthObserverRemote;

/**
 * Type alias for BatteryHealthObserverInterface.
 * @typedef {ash.diagnostics.mojom.BatteryHealthObserverInterface}
 */
export const BatteryHealthObserverInterface =
    ash.diagnostics.mojom.BatteryHealthObserverInterface;

/**
 * Type alias for BatteryHealthObserverReceiver.
 * @typedef {ash.diagnostics.mojom.BatteryHealthObserverReceiver}
 */
export const BatteryHealthObserverReceiver =
    ash.diagnostics.mojom.BatteryHealthObserverReceiver;

/**
 * Type alias for BatteryHealth.
 * @typedef {ash.diagnostics.mojom.BatteryHealth}
 */
export const BatteryHealth = ash.diagnostics.mojom.BatteryHealth;

/**
 * Type alias for MemoryUsageObserver.
 * @typedef {ash.diagnostics.mojom.MemoryUsageObserver}
 */
export const MemoryUsageObserver = ash.diagnostics.mojom.MemoryUsageObserver;

/**
 * Type alias for MemoryUsageObserverRemote.
 * @typedef {ash.diagnostics.mojom.MemoryUsageObserverRemote}
 */
export const MemoryUsageObserverRemote =
    ash.diagnostics.mojom.MemoryUsageObserverRemote;

/**
 * Type alias for MemoryUsageObserverInterface.
 * @typedef {ash.diagnostics.mojom.MemoryUsageObserverInterface}
 */
export const MemoryUsageObserverInterface =
    ash.diagnostics.mojom.MemoryUsageObserverInterface;

/**
 * Type alias for MemoryUsageObserverReceiver.
 * @typedef {ash.diagnostics.mojom.MemoryUsageObserverReceiver}
 */
export const MemoryUsageObserverReceiver =
    ash.diagnostics.mojom.MemoryUsageObserverReceiver;

/**
 * Type alias for MemoryUsage.
 * @typedef {ash.diagnostics.mojom.MemoryUsage}
 */
export const MemoryUsage = ash.diagnostics.mojom.MemoryUsage;

/**
 * Type alias for CpuUsageObserver.
 * @typedef {ash.diagnostics.mojom.CpuUsageObserver}
 */
export const CpuUsageObserver = ash.diagnostics.mojom.CpuUsageObserver;

/**
 * Type alias for CpuUsageObserverRemote.
 * @typedef {ash.diagnostics.mojom.CpuUsageObserverRemote}
 */
export const CpuUsageObserverRemote =
    ash.diagnostics.mojom.CpuUsageObserverRemote;

/**
 * Type alias for CpuUsageObserverInterface.
 * @typedef {ash.diagnostics.mojom.CpuUsageObserverInterface}
 */
export const CpuUsageObserverInterface =
    ash.diagnostics.mojom.CpuUsageObserverInterface;

/**
 * Type alias for CpuUsageObserverReceiver.
 * @typedef {ash.diagnostics.mojom.CpuUsageObserverReceiver}
 */
export const CpuUsageObserverReceiver =
    ash.diagnostics.mojom.CpuUsageObserverReceiver;

/**
 * Type alias for CpuUsage.
 * @typedef {ash.diagnostics.mojom.CpuUsage}
 */
export const CpuUsage = ash.diagnostics.mojom.CpuUsage;

/**
 * Enumeration of routines.
 * @typedef {ash.diagnostics.mojom.RoutineType}
 */
export const RoutineType = ash.diagnostics.mojom.RoutineType;

/**
 * Type alias for StandardRoutineResult.
 * @typedef {ash.diagnostics.mojom.StandardRoutineResult}
 */
export const StandardRoutineResult =
    ash.diagnostics.mojom.StandardRoutineResult;

/**
 * Type alias for PowerRoutineResult.
 * @typedef {ash.diagnostics.mojom.PowerRoutineResult}
 */
export const PowerRoutineResult = ash.diagnostics.mojom.PowerRoutineResult;

/**
 * Type alias for RoutineResult.
 * @typedef {ash.diagnostics.mojom.RoutineResult}
 */
export const RoutineResult = ash.diagnostics.mojom.RoutineResult;

/**
 * Type alias for RoutineResultInfo.
 * @typedef {ash.diagnostics.mojom.RoutineResultInfo}
 */
export const RoutineResultInfo = ash.diagnostics.mojom.RoutineResultInfo;

/**
 * Type alias for RoutineRunnerInterface.
 * @typedef {ash.diagnostics.mojom.RoutineRunnerInterface}
 */
export const RoutineRunnerInterface =
    ash.diagnostics.mojom.RoutineRunnerInterface;

/**
 * Type alias for RoutineRunnerRemote.
 * @typedef {ash.diagnostics.mojom.RoutineRunnerRemote}
 */
export const RoutineRunnerRemote = ash.diagnostics.mojom.RoutineRunnerRemote;

/**
 * Type alias for RoutineRunnerReceiver.
 * @typedef {ash.diagnostics.mojom.RoutineRunnerReceiver}
 */
export const RoutineRunnerReceiver =
    ash.diagnostics.mojom.RoutineRunnerReceiver;

/**
 * Type alias for SystemRoutineController.
 * @typedef {ash.diagnostics.mojom.SystemRoutineController}
 */
export const SystemRoutineController =
    ash.diagnostics.mojom.SystemRoutineController;

/**
 * Type alias for SystemRoutineControllerInterface.
 * @typedef {ash.diagnostics.mojom.SystemRoutineControllerInterface}
 */
export const SystemRoutineControllerInterface =
    ash.diagnostics.mojom.SystemRoutineControllerInterface;

/**
 * Type alias for NetworkListObserver.
 * @typedef {ash.diagnostics.mojom.NetworkListObserverRemote}
 */
export const NetworkListObserverRemote =
    ash.diagnostics.mojom.NetworkListObserverRemote;

/**
 * Type alias for NetworkStateObserver.
 * @typedef {ash.diagnostics.mojom.NetworkStateObserverRemote}
 */
export const NetworkStateObserverRemote =
    ash.diagnostics.mojom.NetworkStateObserverRemote;

/**
 * Type alias for Network.
 * @typedef {ash.diagnostics.mojom.Network}
 */
export const Network = ash.diagnostics.mojom.Network;

/**
 * Type alias for NetworkHealthProvider.
 * @typedef {ash.diagnostics.mojom.NetworkHealthProvider}
 */
export const NetworkHealthProvider =
    ash.diagnostics.mojom.NetworkHealthProvider;

/**
 * Type alias for NetworkHealthProviderInterface.
 * @typedef {ash.diagnostics.mojom.NetworkHealthProviderInterface}
 */
export const NetworkHealthProviderInterface =
    ash.diagnostics.mojom.NetworkHealthProviderInterface;

/**
 * Type alias for NetworkState.
 * @typedef {ash.diagnostics.mojom.NetworkState}
 */
export const NetworkState = ash.diagnostics.mojom.NetworkState;

/**
 * Type alias for NetworkType
 * @typedef {ash.diagnostics.mojom.NetworkType}
 */
export const NetworkType = ash.diagnostics.mojom.NetworkType;

/**
 * Type alias for NetworkListObserverReceiver.
 * @typedef {ash.diagnostics.mojom.NetworkListObserverReceiver}
 */
export const NetworkListObserverReceiver =
    ash.diagnostics.mojom.NetworkListObserverReceiver;

/**
 * Type alias for NetworkListObserverInterface.
 * @typedef {ash.diagnostics.mojom.NetworkListObserverInterface}
 */
export const NetworkListObserverInterface =
    ash.diagnostics.mojom.NetworkListObserverInterface;

/**
 * Type alias for NetworkStateObserverInterface.
 * @typedef {ash.diagnostics.mojom.NetworkStateObserverInterface}
 */
export const NetworkStateObserverInterface =
    ash.diagnostics.mojom.NetworkStateObserverInterface;

/**
 * Type alias for NetworkStateObserverReceiver.
 * @typedef {ash.diagnostics.mojom.NetworkStateObserverReceiver}
 */
export const NetworkStateObserverReceiver =
    ash.diagnostics.mojom.NetworkStateObserverReceiver;

/**
 * @typedef {{
 *   networkGuids: !Array<string>,
 *   activeGuid: string,
 * }}
 */
export let NetworkGuidInfo;

/**
 * Type alias for NetworkTypeProperties.
 * @typedef {ash.diagnostics.mojom.NetworkTypeProperties}
 */
export const NetworkTypeProperties =
    ash.diagnostics.mojom.NetworkTypeProperties;

/**
 * Type alias for AuthenticationType.
 * @typedef {ash.diagnostics.mojom.AuthenticationType}
 */
export const AuthenticationType = ash.diagnostics.mojom.AuthenticationType;

/**
 * Type alias for EthernetStateProperties.
 * @typedef {ash.diagnostics.mojom.EthernetStateProperties}
 */
export const EthernetStateProperties =
    ash.diagnostics.mojom.EthernetStateProperties;

/**
 * Type alias for WiFiStateProperties.
 * @typedef {ash.diagnostics.mojom.WiFiStateProperties}
 */
export const WiFiStateProperties = ash.diagnostics.mojom.WiFiStateProperties;

/**
 * Type alias for SecurityType.
 * @typedef {ash.diagnostics.mojom.SecurityType}
 */
export const SecurityType = ash.diagnostics.mojom.SecurityType;

/**
 * Type alias for RoamingState.
 * @typedef {ash.diagnostics.mojom.RoamingState}
 */
export const RoamingState = ash.diagnostics.mojom.RoamingState;

/**
 * Type alias for LockType.
 * @typedef {ash.diagnostics.mojom.LockType}
 */
export const LockType = ash.diagnostics.mojom.LockType;

/**
 * Radio band related to channel frequency.
 * @enum {number}
 */
export const ChannelBand = {
  UNKNOWN: 0,
  /** 5Ghz radio band. */
  FIVE_GHZ: 1,
  /** 2.4Ghz radio band. */
  TWO_DOT_FOUR_GHZ: 2,
};

/**
 * Struct for holding data related to WiFi network channel.
 * @typedef {{
 *   channel: number,
 *   band: !ChannelBand,
 * }}
 */
export let ChannelProperties;

/**
 * @typedef {{
 *   routine: !RoutineType,
 *   blocking: boolean,
 * }}
 */
export let RoutineProperties;

/**
 * @typedef {{
 *   header: string,
 *   linkText: string,
 *   url: string,
 * }}
 */
export let TroubleshootingInfo;

/**
 * Type alias for ash::diagnostics::metrics::NavigationView to support message
 * handler logic and metric recording. Enum values need to be kept in sync with
 * "ash/webui/diagnostics_ui/diagnostics_metrics_message_handler.h".
 * @enum {number}
 */
export const NavigationView = {
  kSystem: 0,
  kConnectivity: 1,
  kInput: 2,
  kMaxValue: 2,
};

/**
 * Type alias for ConnectionType.
 * @typedef {ash.diagnostics.mojom.ConnectionType}
 */
export const ConnectionType = ash.diagnostics.mojom.ConnectionType;

/**
 * Type alias for PhysicalLayout.
 * @typedef {ash.diagnostics.mojom.PhysicalLayout}
 */
export const PhysicalLayout = ash.diagnostics.mojom.PhysicalLayout;

/**
 * Type alias for MechanicalLayout.
 * @typedef {ash.diagnostics.mojom.MechanicalLayout}
 */
export const MechanicalLayout = ash.diagnostics.mojom.MechanicalLayout;

/**
 * Type alias for NumberPadPresence.
 * @typedef {ash.diagnostics.mojom.NumberPadPresence}
 */
export const NumberPadPresence = ash.diagnostics.mojom.NumberPadPresence;

/**
 * Type alias for TopRowKey.
 * @typedef {ash.diagnostics.mojom.TopRowKey}
 */
export const TopRowKey = ash.diagnostics.mojom.TopRowKey;

/**
 * Type alias for TopRightKey.
 * @typedef {ash.diagnostics.mojom.TopRightKey}
 */
export const TopRightKey = ash.diagnostics.mojom.TopRightKey;

/**
 * Type alias for KeyboardInfo.
 * @typedef {ash.diagnostics.mojom.KeyboardInfo}
 */
export const KeyboardInfo = ash.diagnostics.mojom.KeyboardInfo;

/**
 * Type alias for TouchDeviceType.
 * @typedef {ash.diagnostics.mojom.TouchDeviceType}
 */
export const TouchDeviceType = ash.diagnostics.mojom.TouchDeviceType;

/**
 * Type alias for TouchDeviceInfo.
 * @typedef {ash.diagnostics.mojom.TouchDeviceInfo}
 */
export const TouchDeviceInfo = ash.diagnostics.mojom.TouchDeviceInfo;

/**
 * Type alias for ConnectedDevicesObserver.
 * @typedef {ash.diagnostics.mojom.ConnectedDevicesObserver}
 */
export const ConnectedDevicesObserver = ash.diagnostics.mojom.CpuUsageObserver;

/**
 * Type alias for ConnectedDevicesObserverRemote.
 * @typedef {ash.diagnostics.mojom.ConnectedDevicesObserverRemote}
 */
export const ConnectedDevicesObserverRemote =
    ash.diagnostics.mojom.ConnectedDevicesObserverRemote;

/**
 * Type alias for ConnectedDevicesObserverInterface.
 * @typedef {ash.diagnostics.mojom.ConnectedDevicesObserverInterface}
 */
export const ConnectedDevicesObserverInterface =
    ash.diagnostics.mojom.ConnectedDevicesObserverInterface;

/**
 * Type alias for ConnectedDevicesObserverReceiver.
 * @typedef {ash.diagnostics.mojom.ConnectedDevicesObserverReceiver}
 */
export const ConnectedDevicesObserverReceiver =
    ash.diagnostics.mojom.ConnectedDevicesObserverReceiver;

/**
 * Type alias for KeyboardObserver.
 * @typedef {ash.diagnostics.mojom.KeyboardObserver}
 */
export const KeyboardObserver = ash.diagnostics.mojom.CpuUsageObserver;

/**
 * Type alias for KeyboardObserverRemote.
 * @typedef {ash.diagnostics.mojom.KeyboardObserverRemote}
 */
export const KeyboardObserverRemote =
    ash.diagnostics.mojom.KeyboardObserverRemote;

/**
 * Type alias for KeyboardObserverInterface.
 * @typedef {ash.diagnostics.mojom.KeyboardObserverInterface}
 */
export const KeyboardObserverInterface =
    ash.diagnostics.mojom.KeyboardObserverInterface;

/**
 * Type alias for KeyboardObserverReceiver.
 * @typedef {ash.diagnostics.mojom.KeyboardObserverReceiver}
 */
export const KeyboardObserverReceiver =
    ash.diagnostics.mojom.KeyboardObserverReceiver;


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
export const InputDataProviderInterface =
    ash.diagnostics.mojom.InputDataProviderInterface;

/**
 * Type alias for KeyEvent.
 * @typedef {ash.diagnostics.mojom.KeyEvent}
 */
export const KeyEvent = ash.diagnostics.mojom.KeyEvent;

/**
 * Type alias for KeyEventType.
 * @typedef {ash.diagnostics.mojom.KeyEvent}
 */
export const KeyEventType = ash.diagnostics.mojom.KeyEventType;
