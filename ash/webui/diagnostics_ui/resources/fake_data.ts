// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';

import {CellularNetwork, EthernetNetwork, NetworkGuidInfo, WiFiNetwork} from './diagnostics_types.js';
import {ConnectionType, KeyboardInfo, MechanicalLayout, NumberPadPresence, PhysicalLayout, TopRightKey, TopRowKey} from './input.mojom-webui.js';
import {TouchDeviceInfo, TouchDeviceType} from './input_data_provider.mojom-webui.js';
import {AuthenticationType, LockType, NetworkState, NetworkType, RoamingState, SecurityType, WiFiStateProperties} from './network_health_provider.mojom-webui.js';
import {BatteryChargeStatus, BatteryHealth, BatteryInfo, BatteryState, CpuUsage, ExternalPowerSource, MemoryUsage, SystemInfo} from './system_data_provider.mojom-webui.js';

export const fakeBatteryChargeStatus: BatteryChargeStatus[] = [
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

export const fakeBatteryChargeStatus2: BatteryChargeStatus[] = [{
  batteryState: BatteryState.kDischarging,
  chargeNowMilliampHours: 4200,
  currentNowMilliamps: 1123,
  powerAdapterStatus: ExternalPowerSource.kDisconnected,
  powerTime: stringToMojoString16('3h 15m'),
}];

export const fakeBatteryHealth2: BatteryHealth[] = [
  {
    batteryWearPercentage: 7,
    chargeFullDesignMilliampHours: 6000,
    chargeFullNowMilliampHours: 5700,
    cycleCount: 73,
  },
];

export const fakeBatteryChargeStatus3: BatteryChargeStatus[] = [{
  batteryState: BatteryState.kDischarging,
  chargeNowMilliampHours: 0,
  currentNowMilliamps: 0,
  powerAdapterStatus: ExternalPowerSource.kDisconnected,
  powerTime: stringToMojoString16('0m'),
}];

export const fakeBatteryHealth3: BatteryHealth[] = [
  {
    batteryWearPercentage: 0,
    chargeFullDesignMilliampHours: 0,
    chargeFullNowMilliampHours: 0,
    cycleCount: 0,
  },
];

export const fakeBatteryHealth: BatteryHealth[] = [
  {
    batteryWearPercentage: 7,
    chargeFullDesignMilliampHours: 6000,
    chargeFullNowMilliampHours: 5700,
    cycleCount: 73,
  },
  {
    batteryWearPercentage: 8,
    chargeFullDesignMilliampHours: 6000,
    chargeFullNowMilliampHours: 5699,
    cycleCount: 73,
  },
];

export const fakeBatteryInfo: BatteryInfo = {
  chargeFullDesignMilliampHours: 6000,
  manufacturer: 'BatterCorp USA',
};

export const fakeBatteryInfo2: BatteryInfo = {
  chargeFullDesignMilliampHours: 9000,
  manufacturer: 'PowerPod 9000',
};

export const fakeCpuUsage: CpuUsage[] = [
  {
    averageCpuTempCelsius: 107,
    percentUsageSystem: 15,
    percentUsageUser: 20,
    scalingCurrentFrequencyKhz: 900,
    percentUsageFree: 65,
  },
  {
    averageCpuTempCelsius: 106,
    percentUsageSystem: 30,
    percentUsageUser: 40,
    scalingCurrentFrequencyKhz: 1000,
    percentUsageFree: 0,
  },
  {
    averageCpuTempCelsius: 107,
    percentUsageSystem: 31,
    percentUsageUser: 45,
    scalingCurrentFrequencyKhz: 900,
    percentUsageFree: 24,
  },
  {
    averageCpuTempCelsius: 109,
    percentUsageSystem: 55,
    percentUsageUser: 24,
    scalingCurrentFrequencyKhz: 900,
    percentUsageFree: 21,
  },
  {
    averageCpuTempCelsius: 109,
    percentUsageSystem: 49,
    percentUsageUser: 10,
    scalingCurrentFrequencyKhz: 900,
    percentUsageFree: 41,
  },
  {
    averageCpuTempCelsius: 161,
    percentUsageSystem: 1,
    percentUsageUser: 99,
    scalingCurrentFrequencyKhz: 900,
    percentUsageFree: 0,
  },
  {
    averageCpuTempCelsius: 118,
    percentUsageSystem: 35,
    percentUsageUser: 37,
    scalingCurrentFrequencyKhz: 900,
    percentUsageFree: 28,
  },
  {
    averageCpuTempCelsius: 110,
    percentUsageSystem: 26,
    percentUsageUser: 30,
    scalingCurrentFrequencyKhz: 900,
    percentUsageFree: 44,
  },
];

export const fakeMemoryUsage: MemoryUsage[] = [
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

export const fakeMemoryUsageLowAvailableMemory: MemoryUsage[] = [
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

export const fakeMemoryUsageHighAvailableMemory: MemoryUsage[] = [
  {
    availableMemoryKib: 650000,
    freeMemoryKib: 150000,
    totalMemoryKib: 1280000,
  },
];

export const fakeSystemInfo: SystemInfo = {
  boardName: 'CrOS Board',
  cpuModelName: 'BestCpu SoFast 1000',
  cpuThreadsCount: 8,
  cpuMaxClockSpeedKhz: 1000,
  deviceCapabilities: {hasBattery: true},
  marketingName: 'Coolest Chromebook',
  totalMemoryKib: 128000,
  versionInfo: {milestoneVersion: 'M99', fullVersionString: 'M99.1234.5.6'},
};

export const fakeSystemInfoWithoutBattery: SystemInfo = {
  boardName: 'CrOS Board',
  cpuModelName: 'BestCpu SoFast 1000',
  cpuThreadsCount: 8,
  cpuMaxClockSpeedKhz: 1000,
  deviceCapabilities: {hasBattery: false},
  marketingName: 'Coolest Chromebook',
  totalMemoryKib: 128000,
  versionInfo: {milestoneVersion: 'M99', fullVersionString: 'M99.1234.5.6'},
};

export const fakeSystemInfoWithTBD: SystemInfo = {
  boardName: 'CrOS Board',
  cpuModelName: 'BestCpu SoFast 1000',
  cpuThreadsCount: 8,
  cpuMaxClockSpeedKhz: 1000,
  deviceCapabilities: {hasBattery: true},
  marketingName: 'TBD',
  totalMemoryKib: 128000,
  versionInfo: {milestoneVersion: 'M99', fullVersionString: 'M99.1234.5.6'},
};

export const fakeSystemInfoWithoutBoardName: SystemInfo = {
  boardName: '',
  cpuModelName: 'BestCpu SoFast 1000',
  cpuThreadsCount: 8,
  cpuMaxClockSpeedKhz: 1000,
  deviceCapabilities: {hasBattery: true},
  marketingName: 'TBD',
  totalMemoryKib: 128000,
  versionInfo: {milestoneVersion: 'M99', fullVersionString: 'M99.1234.5.6'},
};

export const fakeAllNetworksAvailable: NetworkGuidInfo = {
  networkGuids: ['wifiGuid', 'ethernetGuid', 'cellularGuid'],
  activeGuid: 'ethernetGuid',
};

export const fakeWifiAndCellularNetworksAvailable: NetworkGuidInfo = {
  networkGuids: ['cellularGuid', 'wifiGuid'],
  activeGuid: 'wifiGuid',
};

export const fakeNetworkGuidInfoList: NetworkGuidInfo[] = [
  fakeAllNetworksAvailable,
  fakeWifiAndCellularNetworksAvailable,
];

export const fakeWiFiStateProperties: WiFiStateProperties = {
  signalStrength: 65,
  frequency: 5745,
  bssid: '44:07:0b:06:2d:85',
  ssid: 'Dial Up',
  security: SecurityType.kWepPsk,
};

export const fakeWifiNetwork: WiFiNetwork = {
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

export const fakeWifiNetworkDisabled: WiFiNetwork = {
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

export const fakeWifiNetworkNoNameServers: WiFiNetwork = {
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
    routingPrefix: 24,
    nameServers: undefined,
  },
};

export const fakeWifiNetworkEmptyNameServers: WiFiNetwork = {
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

export const fakeWifiNetworkMultipleNameServers: WiFiNetwork = {
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

export const fakeWifiNetworkInvalidNameServers: WiFiNetwork = {
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

export const fakeWifiNetworkNoIpAddress: WiFiNetwork = {
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

export const fakeDisconnectedWifiNetwork: WiFiNetwork = {
  state: NetworkState.kNotConnected,
  type: NetworkType.kWiFi,
  observerGuid: 'wifiDisconnectedGuid',
  name: '',
  macAddress: '84:C5:A6:30:3F:31',
};

export const fakePortalWifiNetwork: WiFiNetwork = {
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

export const fakeEthernetNetwork: EthernetNetwork = {
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

export const fakeConnectingEthernetNetwork: EthernetNetwork = {
  state: NetworkState.kConnecting,
  type: NetworkType.kEthernet,
  observerGuid: 'ethernetGuid',
  name: 'ethernetName',
  macAddress: '81:C5:A6:30:3F:33',
};

export const fakeDisconnectedEthernetNetwork: EthernetNetwork = {
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
};

export const fakeCellularNetwork: CellularNetwork = {
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
    },
  },
  observerGuid: 'cellularGuid',
  name: 'cellularName',
  macAddress: '85:C5:A6:30:3F:31',
  ipConfig: {
    ipAddress: '192.168.86.197',
    gateway: '',
    nameServers: undefined,
    routingPrefix: 0,
  },
};

export const fakeCellularWithIpConfigNetwork: CellularNetwork = {
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

export const fakeCellularDisabledNetwork: CellularNetwork = {
  state: NetworkState.kDisabled,
  type: NetworkType.kCellular,
  observerGuid: 'cellularDisabledGuid',
  name: 'cellularName',
  macAddress: '85:C5:A6:30:3F:31',
  ipConfig: undefined,
};

export const fakeCellularDisconnectedNetwork: CellularNetwork = {
  state: NetworkState.kNotConnected,
  type: NetworkType.kCellular,
  typeProperties: undefined,
  observerGuid: 'cellularDisconnectedGuid',
  name: 'cellularName',
  macAddress: '85:C5:A6:30:3F:31',
  ipConfig: undefined,
};

export const fakeKeyboards: KeyboardInfo[] = [
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

export const fakeTouchDevices: TouchDeviceInfo[] = [
  {
    id: 6,
    connectionType: ConnectionType.kInternal,
    type: TouchDeviceType.kPointer,
    name: 'Sample touchpad',
    testable: true,
  },
  {
    id: 7,
    connectionType: ConnectionType.kInternal,
    type: TouchDeviceType.kDirect,
    name: 'Sample touchscreen',
    testable: true,
  },
  {
    id: 8,
    connectionType: ConnectionType.kInternal,
    type: TouchDeviceType.kDirect,
    name: 'Sample untestable touchscreen',
    testable: false,
  },
];
