// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AuthenticationType, BatteryChargeStatus, BatteryHealth, BatteryInfo, BatteryState, ConnectionType, CpuUsage, ExternalPowerSource, KeyboardInfo, LockType, MechanicalLayout, MemoryUsage, Network, NetworkGuidInfo, NetworkState, NetworkType, NumberPadPresence, PhysicalLayout, PowerRoutineResult, RoamingState, RoutineType, SecurityType, StandardRoutineResult, SystemInfo, TopRightKey, TopRowKey, TouchDeviceInfo, TouchDeviceType, WiFiStateProperties} from './diagnostics_types.js';
import {stringToMojoString16} from './mojo_utils.js';

/** @type {!Array<!BatteryChargeStatus>} */
export const fakeBatteryChargeStatus = [
  {
    chargeNowMilliampHours: 4200,
    currentNowMilliamps: 1123,
    powerAdapterStatus: ExternalPowerSource.kAc,
    powerTime: stringToMojoString16('3h 15m'),
    batteryState: BatteryState.kCharging,
  },
  {
    chargeNowMilliampHours: 4500,
    currentNowMilliamps: 1123,
    powerAdapterStatus: ExternalPowerSource.kDisconnected,
    powerTime: stringToMojoString16('3h 01m'),
    batteryState: BatteryState.kDischarging,
  },
  {
    chargeNowMilliampHours: 4800,
    currentNowMilliamps: 1123,
    powerAdapterStatus: ExternalPowerSource.kDisconnected,
    powerTime: stringToMojoString16('2h 45m'),
    batteryState: BatteryState.kDischarging,
  },
  {
    chargeNowMilliampHours: 5700,
    currentNowMilliamps: 1123,
    powerAdapterStatus: ExternalPowerSource.kAc,
    powerTime: stringToMojoString16('2h 45m'),
    batteryState: BatteryState.kFull,
  },
];

/** @type {!Array<!BatteryChargeStatus>} */
export const fakeBatteryChargeStatus2 = [{
  chargeNowMilliampHours: 4200,
  currentNowMilliamps: 1123,
  powerAdapterStatus: ExternalPowerSource.kDisconnected,
  powerTime: stringToMojoString16('3h 15m'),
}];

/** @type {!Array<!BatteryHealth>} */
export const fakeBatteryHealth2 = [
  {
    batteryWearPercentage: 7,
    chargeFullDesignMilliampHours: 6000,
    chargeFullNowMilliampHours: 5700,
    cycleCount: 73,
  },
];

/** @type {!Array<!BatteryHealth>} */
export const fakeBatteryHealth = [
  {
    batteryWearPercentage: 7,
    chargeFullDesignMilliampHours: 6000,
    chargeFullNowMilliampHours: 5700,
    cycleCount: 73,
  },
  {
    battery_weabatteryWearPercentager_percentage: 8,
    chargeFullDesignMilliampHours: 6000,
    chargeFullNowMilliampHours: 5699,
    cycleCount: 73,
  },
];

/** @type {!BatteryInfo} */
export const fakeBatteryInfo = {
  chargeFullDesignMilliampHours: 6000,
  manufacturer: 'BatterCorp USA',
};

/** @type {!BatteryInfo} */
export const fakeBatteryInfo2 = {
  chargeFullDesignMilliampHours: 9000,
  manufacturer: 'PowerPod 9000',
};

/** @type {!Array<!CpuUsage>} */
export const fakeCpuUsage = [
  {
    averageCpuTempCelsius: 107,
    percentUsageSystem: 15,
    percentUsageUser: 20,
    scalingCurrentFrequencyKhz: 900,
  },
  {
    averageCpuTempCelsius: 106,
    percentUsageSystem: 30,
    percentUsageUser: 40,
    scalingCurrentFrequencyKhz: 1000,
  },
  {
    averageCpuTempCelsius: 107,
    percentUsageSystem: 31,
    percentUsageUser: 45,
    scalingCurrentFrequencyKhz: 900,
  },
  {
    averageCpuTempCelsius: 109,
    percentUsageSystem: 55,
    percentUsageUser: 24,
    scalingCurrentFrequencyKhz: 900,
  },
  {
    averageCpuTempCelsius: 109,
    percentUsageSystem: 49,
    percentUsageUser: 10,
    scalingCurrentFrequencyKhz: 900,
  },
  {
    averageCpuTempCelsius: 161,
    percentUsageSystem: 1,
    percentUsageUser: 99,
    scalingCurrentFrequencyKhz: 900,
  },
  {
    averageCpuTempCelsius: 118,
    percentUsageSystem: 35,
    percentUsageUser: 37,
    scalingCurrentFrequencyKhz: 900,
  },
  {
    averageCpuTempCelsius: 110,
    percentUsageSystem: 26,
    percentUsageUser: 30,
    scalingCurrentFrequencyKhz: 900,
  },
];

/** @type {!Array<!MemoryUsage>} */
export const fakeMemoryUsage = [
  {
    availableMemoryKib: 570000,
    freeMemoryKib: 150000,
    totalMemoryKib: 1280000,
  },
  {
    availableMemoryKib: 520000,
    freeMemoryKib: 150000,
    totalMemoryKib: 1280000,
  },
  {
    availableMemoryKib: 530000,
    freeMemoryKib: 150000,
    totalMemoryKib: 1280000,
  },
  {
    availableMemoryKib: 650000,
    freeMemoryKib: 150000,
    totalMemoryKib: 1280000,
  },
];

/** @type {!Array<!MemoryUsage>} */
export const fakeMemoryUsageLowAvailableMemory = [
  {
    availableMemoryKib: 57000,
    freeMemoryKib: 15000,
    totalMemoryKib: 128000,
  },
  {
    availableMemoryKib: 52000,
    freeMemoryKib: 15000,
    totalMemoryKib: 128000,
  },
  {
    availableMemoryKib: 53000,
    freeMemoryKib: 15000,
    totalMemoryKib: 128000,
  },
  {
    availableMemoryKib: 65000,
    freeMemoryKib: 15000,
    totalMemoryKib: 128000,
  },
];

/** @type {!SystemInfo} */
export const fakeSystemInfo = {
  boardName: 'CrOS Board',
  cpuModelName: 'BestCpu SoFast 1000',
  cpuThreadsCount: 8,
  cpuMaxClockSpeedKhz: 1000,
  deviceCapabilities: {hasBattery: true},
  marketingName: 'Coolest Chromebook',
  totalMemoryKib: 128000,
  versionInfo: {milestoneVersion: 'M99', fullVersionString: 'M99.1234.5.6'},
};

/** @type {!SystemInfo} */
export const fakeSystemInfoWithoutBattery = {
  boardName: 'CrOS Board',
  cpuModelName: 'BestCpu SoFast 1000',
  cpuThreadsCount: 8,
  cpuMaxClockSpeedKhz: 1000,
  deviceCapabilities: {hasBattery: false},
  marketingName: 'Coolest Chromebook',
  totalMemoryKib: 128000,
  versionInfo: {milestoneVersion: 'M99', fullVersionString: 'M99.1234.5.6'},
};

/** @type {!SystemInfo} */
export const fakeSystemInfoWithTBD = {
  boardName: 'CrOS Board',
  cpuModelName: 'BestCpu SoFast 1000',
  cpuThreadsCount: 8,
  cpuMaxClockSpeedKhz: 1000,
  deviceCapabilities: {hasBattery: true},
  marketingName: 'TBD',
  totalMemoryKib: 128000,
  versionInfo: {milestoneVersion: 'M99', fullVersionString: 'M99.1234.5.6'},
};

/** @type {!SystemInfo} */
export const fakeSystemInfoWithoutBoardName = {
  boardName: '',
  cpuModelName: 'BestCpu SoFast 1000',
  cpuThreadsCount: 8,
  cpuMaxClockSpeedKhz: 1000,
  deviceCapabilities: {hasBattery: true},
  marketingName: 'TBD',
  totalMemoryKib: 128000,
  versionInfo: {milestoneVersion: 'M99', fullVersionString: 'M99.1234.5.6'},
};
/** @type {!Map<!RoutineType, !StandardRoutineResult>} */
export const fakeRoutineResults = new Map([
  [RoutineType.kCpuStress, StandardRoutineResult.kTestPassed],
  [RoutineType.kCpuCache, StandardRoutineResult.kTestPassed],
  [RoutineType.kCpuFloatingPoint, StandardRoutineResult.kTestFailed],
  [RoutineType.kCpuPrime, StandardRoutineResult.kExecutionError],
  [RoutineType.kMemory, StandardRoutineResult.kTestPassed],
  [RoutineType.kCaptivePortal, StandardRoutineResult.kTestPassed],
  [RoutineType.kDnsLatency, StandardRoutineResult.kTestPassed],
  [RoutineType.kDnsResolution, StandardRoutineResult.kTestPassed],
  [RoutineType.kDnsResolverPresent, StandardRoutineResult.kTestPassed],
  [RoutineType.kGatewayCanBePinged, StandardRoutineResult.kTestPassed],
  [RoutineType.kHasSecureWiFiConnection, StandardRoutineResult.kTestPassed],
  [RoutineType.kHttpFirewall, StandardRoutineResult.kTestPassed],
  [RoutineType.kHttpsFirewall, StandardRoutineResult.kTestPassed],
  [RoutineType.kHttpsLatency, StandardRoutineResult.kTestPassed],
  [RoutineType.kLanConnectivity, StandardRoutineResult.kTestPassed],
  [RoutineType.kSignalStrength, StandardRoutineResult.kTestPassed],
  [RoutineType.kArcHttp, StandardRoutineResult.kTestPassed],
  [RoutineType.kArcPing, StandardRoutineResult.kTestPassed],
  [RoutineType.kArcDnsResolution, StandardRoutineResult.kTestPassed],
]);

/** @type {!Map<!RoutineType, !PowerRoutineResult>} */
export const fakePowerRoutineResults = new Map([
  [
    RoutineType.kBatteryCharge,
    {
      result: StandardRoutineResult.kTestPassed,
      is_charging: true,
      percent_delta: 5,
      time_delta_seconds: 10,
    },
  ],
  [
    RoutineType.kBatteryDischarge,
    {
      result: StandardRoutineResult.kUnableToRun,
      is_charging: false,
      percent_delta: 0,
      time_delta_seconds: 0,
    },
  ],
]);

/** @type {!NetworkGuidInfo} */
export const fakeAllNetworksAvailable = {
  networkGuids: ['wifiGuid', 'ethernetGuid', 'cellularGuid'],
  activeGuid: 'ethernetGuid',
};

/** @type {!NetworkGuidInfo} */
export const fakeWifiAndCellularNetworksAvailable = {
  networkGuids: ['cellularGuid', 'wifiGuid'],
  activeGuid: 'wifiGuid',
};

/** @type {!Array<!NetworkGuidInfo>} */
export const fakeNetworkGuidInfoList = [
  fakeAllNetworksAvailable,
  fakeWifiAndCellularNetworksAvailable,
];

/** @type {!WiFiStateProperties} */
export const fakeWiFiStateProperties = {
  signalStrength: 65,
  frequency: 5745,
  bssid: '44:07:0b:06:2d:85',
  ssid: 'Dial Up',
  security: SecurityType.kWepPsk,
};

/** @type {!Network} */
export const fakeWifiNetwork = {
  state: NetworkState.kConnected,
  type: NetworkType.kWiFi,
  typeProperties: {
    wifi: {
      signalStrength: 65,
      frequency: 5745,
      bssid: '44:07:0b:06:2d:85',
      ssid: 'Dial Up',
      security: SecurityType.kNone,
    },
  },
  observerGuid: 'wifiGuid',
  name: 'Dial Up',
  macAddress: '84:C5:A6:30:3F:31',
  ipConfig: {
    ipAddress: '192.168.86.197',
    gateway: '192.168.86.1',
    nameServers: ['192.168.86.1'],
    routingPrefix: 24,
  },
};

/** @type {!Network} */
export const fakeWifiNetworkDisabled = {
  state: NetworkState.kDisabled,
  type: NetworkType.kWiFi,
  typeProperties: {
    wifi: {
      signalStrength: 65,
      frequency: 5745,
      bssid: '44:07:0b:06:2d:85',
      ssid: 'Dial Up',
      security: SecurityType.kNone,
    },
  },
  observerGuid: 'wifiGuidDisabled',
  name: 'Dial Up',
  macAddress: '84:C5:A6:30:3F:31',
  ipConfig: {
    ipAddress: '192.168.86.197',
    gateway: '192.168.86.1',
    nameServers: ['192.168.86.1'],
    routingPrefix: 24,
  },
};

/** @type {!Network} */
export const fakeWifiNetworkNoNameServers = {
  state: NetworkState.kConnected,
  type: NetworkType.kWiFi,
  typeProperties: {
    wifi: {
      signalStrength: 65,
      frequency: 5745,
      bssid: '44:07:0b:06:2d:85',
      ssid: 'Dial Up',
      security: SecurityType.kNone,
    },
  },
  observerGuid: 'wifiGuid',
  name: 'Dial Up',
  macAddress: '84:C5:A6:30:3F:31',
  ipConfig: {
    ipAddress: '192.168.86.197',
    gateway: '192.168.86.1',
    nameServers: null,
    routingPrefix: 24,
  },
};

/** @type {!Network} */
export const fakeWifiNetworkEmptyNameServers = {
  state: NetworkState.kConnected,
  type: NetworkType.kWiFi,
  typeProperties: {
    wifi: {
      signalStrength: 65,
      frequency: 5745,
      bssid: '44:07:0b:06:2d:85',
      ssid: 'Dial Up',
      security: SecurityType.kNone,
    },
  },
  observerGuid: 'wifiGuid',
  name: 'Dial Up',
  macAddress: '84:C5:A6:30:3F:31',
  ipConfig: {
    ipAddress: '192.168.86.197',
    gateway: '192.168.86.1',
    nameServers: [],
    routingPrefix: 24,
  },
};

/** @type {!Network} */
export const fakeWifiNetworkMultipleNameServers = {
  state: NetworkState.kConnected,
  type: NetworkType.kWiFi,
  typeProperties: {
    wifi: {
      signalStrength: 65,
      frequency: 5745,
      bssid: '44:07:0b:06:2d:85',
      ssid: 'Dial Up',
      security: SecurityType.kNone,
    },
  },
  observerGuid: 'wifiGuid',
  name: 'Dial Up',
  macAddress: '84:C5:A6:30:3F:31',
  ipConfig: {
    ipAddress: '192.168.86.197',
    gateway: '192.168.86.1',
    nameServers: ['192.168.86.1', '192.168.86.2'],
    routingPrefix: 24,
  },
};

/** @type {!Network} */
export const fakeWifiNetworkInvalidNameServers = {
  state: NetworkState.kConnected,
  type: NetworkType.kWiFi,
  typeProperties: {
    wifi: {
      signalStrength: 65,
      frequency: 5745,
      bssid: '44:07:0b:06:2d:85',
      ssid: 'Dial Up',
      security: SecurityType.kNone,
    },
  },
  observerGuid: 'wifiGuidInvalidNameServers',
  name: 'Dial Up',
  macAddress: '84:C5:A6:30:3F:31',
  ipConfig: {
    ipAddress: '192.168.86.197',
    gateway: '192.168.86.1',
    nameServers: ['0.0.0.0', '192.168.86.1'],
    routingPrefix: 24,
  },
};

/** @type {!Network} */
export const fakeWifiNetworkNoIpAddress = {
  state: NetworkState.kConnecting,
  type: NetworkType.kWiFi,
  typeProperties: {
    wifi: {
      signalStrength: 65,
      frequency: 5745,
      bssid: '44:07:0b:06:2d:85',
      ssid: 'Dial Up',
      security: SecurityType.kNone,
    },
  },
  observerGuid: 'wifiGuidNoIpAddress',
  name: 'Dial Up',
  macAddress: '84:C5:A6:30:3F:31',
  ipConfig: {
    ipAddress: '',
    gateway: '192.168.86.1',
    nameServers: ['0.0.0.0', '192.168.86.1'],
    routingPrefix: 24,
  },
};

export const fakeDisconnectedWifiNetwork = {
  state: NetworkState.kNotConnected,
  type: NetworkType.kWiFi,
  typeProperties: null,
  observerGuid: 'wifiDisconnectedGuid',
  name: '',
  macAddress: '84:C5:A6:30:3F:31',
  ipConfig: null,
};

/** @type {!Network} */
export const fakePortalWifiNetwork = {
  state: NetworkState.kPortal,
  type: NetworkType.kWiFi,
  typeProperties: {
    wifi: {
      signalStrength: 65,
      frequency: 5745,
      bssid: '44:07:0b:06:2d:85',
      ssid: 'Dial Up',
      security: SecurityType.kWepPsk,
    },
  },
  observerGuid: 'wifiPortalGuid',
  name: '',
  macAddress: '84:C5:A6:30:3F:31',
  ipConfig: {
    ipAddress: '192.168.86.197',
    gateway: '192.168.86.1',
    nameServers: ['192.168.86.1', '192.168.86.2'],
    routingPrefix: 24,
  },
};

/** @type {!Network} */
export const fakeEthernetNetwork = {
  state: NetworkState.kOnline,
  type: NetworkType.kEthernet,
  typeProperties: {
    ethernet: {
      authentication: AuthenticationType.k8021x,
    },
  },
  observerGuid: 'ethernetGuid',
  name: 'ethernetName',
  macAddress: '81:C5:A6:30:3F:31',
  ipConfig: {
    ipAddress: '192.168.86.197',
    gateway: '192.168.86.1',
    nameServers: ['192.168.86.1'],
    routingPrefix: 24,
  },
};

/** @type {!Network} */
export const fakeConnectingEthernetNetwork = {
  state: NetworkState.kConnecting,
  type: NetworkType.kEthernet,
  observerGuid: 'ethernetGuid',
  name: 'ethernetName',
  macAddress: '81:C5:A6:30:3F:33',
};

/** @type {!Network} */
export const fakeDisconnectedEthernetNetwork = {
  state: NetworkState.kNotConnected,
  type: NetworkType.kEthernet,
  typeProperties: {
    ethernet: {
      authentication: AuthenticationType.kNone,
    },
  },
  observerGuid: 'ethernetDisconnectedGuid',
  name: 'ethernetName',
  macAddress: '81:C5:A6:30:3F:32',
  ipConfig: null,
};

/** @type {!Network} */
export const fakeCellularNetwork = {
  state: NetworkState.kConnected,
  type: NetworkType.kCellular,
  typeProperties: {
    cellular: {
      networkTechnology: 'LTE',
      roaming: true,
      roamingState: RoamingState.kRoaming,
      signalStrength: 55,
      iccid: '83948080007483825411',
      eid: '82099038007008862600508229159883',
      simLocked: true,
      lockType: LockType.kSimPin,
      simAbsent: false,
    },
  },
  observerGuid: 'cellularGuid',
  name: 'cellularName',
  macAddress: '85:C5:A6:30:3F:31',
  ipConfig: {
    ipAddress: '192.168.86.197',
    gateway: '',
    nameServers: null,
    routingPrefix: 0,
  },
};

/** @type {!Network} */
export const fakeCellularWithIpConfigNetwork = {
  state: NetworkState.kConnected,
  type: NetworkType.kCellular,
  typeProperties: {
    cellular: {
      networkTechnology: 'LTE',
      roaming: true,
      roamingState: RoamingState.kRoaming,
      signalStrength: 55,
      iccid: '83948080007483825411',
      eid: '82099038007008862600508229159883',
      simLocked: true,
      lockType: LockType.kSimPin,
      simAbsent: false,
    },
  },
  observerGuid: 'cellularWithIpConfigGuid',
  name: 'cellularName',
  macAddress: '85:C5:A6:30:3F:31',
  ipConfig: {
    ipAddress: '192.168.86.197',
    gateway: '192.168.86.1',
    nameServers: ['192.168.86.1'],
    routingPrefix: 24,
  },
};

/** @type {!Network} */
export const fakeCellularDisabledNetwork = {
  state: NetworkState.kDisabled,
  type: NetworkType.kCellular,
  observerGuid: 'cellularDisabledGuid',
  name: 'cellularName',
  macAddress: '85:C5:A6:30:3F:31',
};

/** @type {!Network} */
export const fakeCellularDisconnectedNetwork = {
  state: NetworkState.kNotConnected,
  type: NetworkType.kCellular,
  observerGuid: 'cellularDisconnectedGuid',
  name: 'cellularName',
  macAddress: '85:C5:A6:30:3F:31',
};

/** @type {!Array<!KeyboardInfo>} */
export const fakeKeyboards = [
  {
    id: 3,
    connectionType: ConnectionType.kInternal,
    name: 'AT Translated Set 2 keyboard',
    physicalLayout: PhysicalLayout.kChromeOS,
    mechanicalLayout: MechanicalLayout.kAnsi,
    regionCode: 'jp',
    hasAssistantKey: true,
    topRowKeys: [
      TopRowKey.kBack,
      TopRowKey.kForward,
      TopRowKey.kRefresh,
      TopRowKey.kFullscreen,
      TopRowKey.kOverview,
      TopRowKey.kScreenBrightnessDown,
      TopRowKey.kScreenBrightnessUp,
      TopRowKey.kVolumeMute,
      TopRowKey.kVolumeDown,
      TopRowKey.kVolumeUp,
    ],
    topRightKey: TopRightKey.kPower,
    numberPadPresent: NumberPadPresence.kPresent,
  },
];

/** @type {!Array<!TouchDeviceInfo>} */
export const fakeTouchDevices = [
  {
    id: 6,
    connectionType: ConnectionType.kInternal,
    type: TouchDeviceType.kPointer,
    name: 'Sample touchpad',
  },
  {
    id: 7,
    connectionType: ConnectionType.kInternal,
    type: TouchDeviceType.kDirect,
    name: 'Sample touchscreen',
  },
];
