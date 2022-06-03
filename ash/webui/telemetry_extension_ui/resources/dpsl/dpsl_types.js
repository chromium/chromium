// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * dpsl.* types definitions.
 */

////////////////////// dpsl.telemetry.* type definitions ///////////////////////

/**
 * Response message containing Backlight Info
 * @typedef {!Array<{
 *   path: string,
 *   maxBrightness: number,
 *   brightness: number,
 * }>}
 */
dpsl.BacklightInfo;

/**
 * Response message containing Battery Info
 * @typedef {{
 *   cycleCount: bigint,
 *   voltageNow: number,
 *   vendor: string,
 *   serialNumber: string,
 *   chargeFullDesign: number,
 *   chargeFull: number,
 *   voltageMinDesign: number,
 *   modelName: string,
 *   chargeNow: number,
 *   currentNow: number,
 *   technology: string,
 *   status: string,
 *   manufactureDate: string,
 *   temperature: bigint
 * }}
 */
dpsl.BatteryInfo;

/**
 * Response message containing Bluetooth Info
 * @typedef {!Array<{
 *   name: string,
 *   address: string,
 *   powered: boolean,
 *   numConnectedDevices: number
 * }>}
 */
dpsl.BluetoothInfo;

/**
 * Response message containing VPD Info
 * @typedef {{
 *   skuNumber: string,
 *   serialNumber: string,
 *   modelName: string
 * }}
 */
dpsl.VpdInfo;

/**
 * Response message containing CPU Info
 * @typedef {{
 *   numTotalThreads: number,
 *   architecture: string,
 *   physicalCpus: Array<Object>
 * }}
 */
dpsl.CpuInfo;

/**
 * Response message containing Fan Info
 * @typedef {!Array<{
 *   speedRpm: number
 * }>}
 */
dpsl.FanInfo;

/**
 * Response message containing Memory Info
 * @typedef {{
 *   totalMemoryKib: number,
 *   freeMemoryKib: number,
 *   availableMemoryKib: number,
 *   pageFaultsSinceLastBoot: bigint
 * }}
 */
dpsl.MemoryInfo;

/**
 * Response message containing BlockDevice Info
 * @typedef {!Array<{
 *   path: string,
 *   size: bigint,
 *   type: string,
 *   manufacturerId: number,
 *   name: string,
 *   serial: string,
 *   bytesReadSinceLastBoot: bigint,
 *   bytesWrittenSinceLastBoot: bigint,
 *   readTimeSecondsSinceLastBoot: bigint,
 *   writeTimeSecondsSinceLastBoot: bigint,
 *   ioTimeSecondsSinceLastBoot: bigint,
 *   discardTimeSecondsSinceLastBoot: bigint
 * }>}
 */
dpsl.BlockDeviceInfo;

/**
 * Response message containing StatefulPartition Info
 * @typedef {{
 *   availableSpace: bigint,
 *   totalSpace: bigint
 * }}
 */
dpsl.StatefulPartitionInfo;

/**
 * Response message containing Timezone Info
 * @typedef {{
 *   posix: string,
 *   region: string
 * }}
 */
dpsl.TimezoneInfo;

/**
 * Union of the Telemetry Info types.
 * @typedef {(!dpsl.BacklightInfo|!dpsl.BatteryInfo|!dpsl.BluetoothInfo|
 *   !dpsl.VpdInfo|!dpsl.CpuInfo|!dpsl.FanInfo|!dpsl.MemoryInfo|
 *   !dpsl.BlockDeviceInfo|!dpsl.StatefulPartitionInfo|!dpsl.TimezoneInfo
 * )}
 */
dpsl.TelemetryInfoTypes;

///////////////////// dpsl.diagnostics.* type definitions /////////////////////

/**
 * Static list of available diagnostics routines (tests).
 * @typedef {!Array<!string>}
 */
dpsl.AvailableRoutinesList;

/**
 * |progressPercent| percentage of the routine progress.
 * |output| accumulated output, like logs.
 * |status| current status of the routine.
 * |detail| more detailed status message.
 * |userMessage| Request user action. Should be localized and displayed to the
 * user. Note: used in interactive routines only, two possible values are
 * returned: 'unplug-ac-power' or 'plug-in-ac-power'.
 * @typedef {{
 *   progressPercent: number,
 *   output: string,
 *   status: string,
 *   statusMessage: string,
 *   userMessage: string
  }}
 */
dpsl.RoutineStatus;

/**
 * Params object of dpsl.diagnostics.battery.runChargeRoutine()
 * @typedef {{
 *   lengthSeconds: !number,
 *   minimumChargePercentRequired: !number
 * }}
 */
dpsl.BatteryChargeRoutineParams;

/**
 * Params object of dpsl.diagnostics.battery.runDischargeRoutine()
 * @typedef {{
 *   lengthSeconds: !number,
 *   maximumDischargePercentAllowed: !number
 * }}
 */
dpsl.BatteryDischargeRoutineParams;

/**
 * Params object of dpsl.diagnostics.nvme.runWearLevelRoutine()
 * @typedef {{wearLevelThreshold: !number}}
 */
dpsl.NvmeWearLevelRoutineParams;

/**
 * Params object of dpsl.diagnostics.power.{runAcConnectedRoutine(),
 * runAcDisconnectedRoutine()}
 * @typedef {{expectedPowerType: !string}}
 */
dpsl.PowerAcRoutineParams;

/**
 * Params object of dpsl.diagnostics.cpu.{runCacheRoutine(), runStressRoutine(),
 * runFloatingPointAccuracyRoutine()}
 * @typedef {{duration: !number}}
 */
dpsl.CpuRoutineDurationParams;

/**
 * Params object of dpsl.diagnostics.cpu.runPrimeSearchRoutine()
 * @typedef {{lengthSeconds: !number, maximumNumber: !number}}
 */
dpsl.CpuPrimeSearchRoutineParams;

/**
 * Params object of dpsl.diagnostics.disk.run{Linear/Random}ReadRoutine()
 * @typedef {{lengthSeconds: !number, fileSizeMB: !number}}
 */
dpsl.DiskReadRoutineParams;
//////////////////// dpsl.system_events.* type definitions /////////////////////

/**
 * The list of supported system events: ['ac-inserted', 'ac-removed',
 * 'bluetooth-adapter-added', 'bluetooth-adapter-property-changed',
 * 'bluetooth-adapter-removed', 'bluetooth-device-added',
 * 'bluetooth-device-property-changed', 'bluetooth-device-removed',
 * 'lid-closed', 'lid-opened', 'os-resume', 'os-suspend']
]
 * @typedef {!Array<!string>}
 */
dpsl.EventTypes;
