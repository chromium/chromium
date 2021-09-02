// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://telemetry-extension.
 */

GEN('#include "ash/webui/telemetry_extension_ui/test/telemetry_extension_ui_browsertest.h"');

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://telemetry-extension';
const UNTRUSTED_HOST_ORIGIN = 'chrome-untrusted://telemetry-extension';

var TelemetryExtensionUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kTelemetryExtension']};
  }

  /** @override */
  get typedefCppFixture() {
    return 'TelemetryExtensionUiBrowserTest';
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get extraLibraries() {
    return [
      ...super.extraLibraries,
      '//ash/webui/telemetry_extension_ui/test/trusted_test_requester.js',
    ];
  }
};

// Tests that chrome://telemetry-extension runs js file and that it goes
// somewhere instead of 404ing or crashing.
TEST_F('TelemetryExtensionUIBrowserTest', 'HasChromeSchemeURL', () => {
  const title = document.querySelector('title');

  assertEquals('Telemetry Extension', title.innerText);
  assertEquals(HOST_ORIGIN, document.location.origin);
  testDone();
});

// Tests that chrome://telemetry-extension embeds a
// chrome-untrusted:// iframe.
TEST_F('TelemetryExtensionUIBrowserTest', 'HasChromeUntrustedIframe', () => {
  const iframe = document.querySelector('iframe');
  assertNotEquals(null, iframe);
  testDone();
});

TEST_F('TelemetryExtensionUIBrowserTest', 'ConvertDiagnosticsEnums', () => {
  // Unit tests for convertRoutineId
  assertEquals(234089591, diagnosticsProxy.convertRoutineId(234089591));

  // Unit tests for convertCommandToEnum
  const commandEnum = ash.health.mojom.DiagnosticRoutineCommandEnum;

  assertEquals(
      commandEnum.kContinue, diagnosticsProxy.convertCommandToEnum('continue'));
  assertEquals(
      commandEnum.kCancel, diagnosticsProxy.convertCommandToEnum('cancel'));
  assertEquals(
      commandEnum.kGetStatus,
      diagnosticsProxy.convertCommandToEnum('get-status'));
  assertEquals(
      commandEnum.kRemove, diagnosticsProxy.convertCommandToEnum('remove'));

  // Unit tests for convertStatus
  const statusEnum = ash.health.mojom.DiagnosticRoutineStatusEnum;

  assertEquals('ready', diagnosticsProxy.convertStatus(statusEnum.kReady));
  assertEquals('running', diagnosticsProxy.convertStatus(statusEnum.kRunning));
  assertEquals('waiting', diagnosticsProxy.convertStatus(statusEnum.kWaiting));
  assertEquals('passed', diagnosticsProxy.convertStatus(statusEnum.kPassed));
  assertEquals('failed', diagnosticsProxy.convertStatus(statusEnum.kFailed));
  assertEquals('error', diagnosticsProxy.convertStatus(statusEnum.kError));
  assertEquals(
      'cancelled', diagnosticsProxy.convertStatus(statusEnum.kCancelled));
  assertEquals(
      'failed-to-start',
      diagnosticsProxy.convertStatus(statusEnum.kFailedToStart));
  assertEquals('removed', diagnosticsProxy.convertStatus(statusEnum.kRemoved));
  assertEquals(
      'cancelling', diagnosticsProxy.convertStatus(statusEnum.kCancelling));
  assertEquals(
      'unsupported', diagnosticsProxy.convertStatus(statusEnum.kUnsupported));
  assertEquals('not-run', diagnosticsProxy.convertStatus(statusEnum.kNotRun));

  // Unit tests for convertUserMessage
  const userMessageEnum =
      ash.health.mojom.DiagnosticRoutineUserMessageEnum;

  assertEquals(
      'unplug-ac-power',
      diagnosticsProxy.convertUserMessage(userMessageEnum.kUnplugACPower));
  assertEquals(
      'plug-in-ac-power',
      diagnosticsProxy.convertUserMessage(userMessageEnum.kPlugInACPower));

  // Unit tests for convertPowerStatusToEnum
  const acPowerStatusEnum = ash.health.mojom.AcPowerStatusEnum;
  assertEquals(
      acPowerStatusEnum.kConnected,
      diagnosticsProxy.convertPowerStatusToEnum('connected'));
  assertEquals(
      acPowerStatusEnum.kDisconnected,
      diagnosticsProxy.convertPowerStatusToEnum('disconnected'));

  // Unit tests for convertNvmeSelfTestTypeToEnum
  const nvmeSelfTestTypeEnum = ash.health.mojom.NvmeSelfTestTypeEnum;
  assertEquals(
      nvmeSelfTestTypeEnum.kShortSelfTest,
      diagnosticsProxy.convertNvmeSelfTestTypeToEnum('short-self-test'));
  assertEquals(
      nvmeSelfTestTypeEnum.kLongSelfTest,
      diagnosticsProxy.convertNvmeSelfTestTypeToEnum('long-self-test'));

  // Unit tests for convertDiskReadTypeToEnum
  const diskReadRoutineTypeEnum = ash.health.mojom.DiskReadRoutineTypeEnum;
  assertEquals(
      diskReadRoutineTypeEnum.kLinearRead,
      diagnosticsProxy.convertDiskReadTypeToEnum('linear-read'));
  assertEquals(
      diskReadRoutineTypeEnum.kRandomRead,
      diagnosticsProxy.convertDiskReadTypeToEnum('random-read'));

  testDone();
});

// Tests that Telemetry correctly converts Mojo enums to strings.
TEST_F('TelemetryExtensionUIBrowserTest', 'ConvertTelemetryEnums', () => {
  // Unit tests for convertErrorType.
  const errorTypeEnum = ash.health.mojom.ErrorType;

  assertEquals(
      'file-read-error',
      telemetryProxy.convertErrorType(errorTypeEnum.kFileReadError));
  assertEquals(
      'parse-error',
      telemetryProxy.convertErrorType(errorTypeEnum.kParseError));
  assertEquals(
      'system-utility-error',
      telemetryProxy.convertErrorType(errorTypeEnum.kSystemUtilityError));
  assertEquals(
      'service-unavailable',
      telemetryProxy.convertErrorType(errorTypeEnum.kServiceUnavailable));

  // Unit tests for convertCpuArch.
  const cpuArchEnum = ash.health.mojom.CpuArchitectureEnum;

  assertEquals('unknown', telemetryProxy.convertCpuArch(cpuArchEnum.kUnknown));
  assertEquals('x86-64', telemetryProxy.convertCpuArch(cpuArchEnum.kX86_64));
  assertEquals('AArch64', telemetryProxy.convertCpuArch(cpuArchEnum.kAArch64));
  assertEquals('Armv7l', telemetryProxy.convertCpuArch(cpuArchEnum.kArmv7l));

  // Check that convertAllEnums converts all Mojo enums to strings and does not
  // crash if some enums are not present in TelemetryInfo.
  assertDeepEquals({}, telemetryProxy.convertAllEnums({}));
  assertDeepEquals(
      {
        batteryResult: {},
        blockDeviceResult: {},
        vpdResult: {},
        cpuResult: {},
        timezoneResult: {},
        memoryResult: {},
        backlightResult: {},
        fanResult: {},
        statefulPartitionResult: {},
        bluetoothResult: {}
      },
      telemetryProxy.convertAllEnums({
        batteryResult: {},
        blockDeviceResult: {},
        vpdResult: {},
        cpuResult: {},
        timezoneResult: {},
        memoryResult: {},
        backlightResult: {},
        fanResult: {},
        statefulPartitionResult: {},
        bluetoothResult: {}
      }));
  // Check architecture equals to 0 (i.e. kUnknown).
  assertDeepEquals(
      {cpuResult: {cpuInfo: {architecture: 'unknown', physicalCpus: []}}},
      telemetryProxy.convertAllEnums({
        cpuResult:
            {cpuInfo: {architecture: cpuArchEnum.kUnknown, physicalCpus: []}}
      }));
  assertDeepEquals(
      {cpuResult: {cpuInfo: {architecture: 'x86-64', physicalCpus: []}}},
      telemetryProxy.convertAllEnums({
        cpuResult:
            {cpuInfo: {architecture: cpuArchEnum.kX86_64, physicalCpus: []}}
      }));
  // Check error type equals to 0 (i.e. kFileReadError).
  assertDeepEquals(
      {
        batteryResult: {error: {type: 'file-read-error', msg: 'battery'}},
        blockDeviceResult: {error: {type: 'file-read-error', msg: 'ssd'}},
        vpdResult: {error: {type: 'file-read-error', msg: 'vpd'}},
        cpuResult: {error: {type: 'file-read-error', msg: 'cpu'}},
        timezoneResult: {error: {type: 'file-read-error', msg: 'timezone'}},
        memoryResult: {error: {type: 'file-read-error', msg: 'memory'}},
        backlightResult: {error: {type: 'file-read-error', msg: 'backlight'}},
        fanResult: {error: {type: 'file-read-error', msg: 'fan'}},
        statefulPartitionResult:
            {error: {type: 'file-read-error', msg: 'partition'}},
        bluetoothResult: {error: {type: 'file-read-error', msg: 'bluetooth'}}
      },
      telemetryProxy.convertAllEnums({
        batteryResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'battery'}},
        blockDeviceResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'ssd'}},
        vpdResult: {error: {type: errorTypeEnum.kFileReadError, msg: 'vpd'}},
        cpuResult: {error: {type: errorTypeEnum.kFileReadError, msg: 'cpu'}},
        timezoneResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'timezone'}},
        memoryResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'memory'}},
        backlightResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'backlight'}},
        fanResult: {error: {type: errorTypeEnum.kFileReadError, msg: 'fan'}},
        statefulPartitionResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'partition'}},
        bluetoothResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'bluetooth'}}
      }));
  assertDeepEquals(
      {
        batteryResult: {error: {type: 'parse-error', msg: 'battery'}},
        blockDeviceResult: {error: {type: 'system-utility-error', msg: 'ssd'}},
        vpdResult: {error: {type: 'service-unavailable', msg: 'vpd'}},
        cpuResult: {error: {type: 'parse-error', msg: 'cpu'}},
        timezoneResult:
            {error: {type: 'system-utility-error', msg: 'timezone'}},
        memoryResult: {error: {type: 'service-unavailable', msg: 'memory'}},
        backlightResult: {error: {type: 'parse-error', msg: 'backlight'}},
        fanResult: {error: {type: 'system-utility-error', msg: 'fan'}},
        statefulPartitionResult:
            {error: {type: 'service-unavailable', msg: 'partition'}},
        bluetoothResult: {error: {type: 'parse-error', msg: 'bluetooth'}}
      },
      telemetryProxy.convertAllEnums({
        batteryResult:
            {error: {type: errorTypeEnum.kParseError, msg: 'battery'}},
        blockDeviceResult:
            {error: {type: errorTypeEnum.kSystemUtilityError, msg: 'ssd'}},
        vpdResult:
            {error: {type: errorTypeEnum.kServiceUnavailable, msg: 'vpd'}},
        cpuResult: {error: {type: errorTypeEnum.kParseError, msg: 'cpu'}},
        timezoneResult:
            {error: {type: errorTypeEnum.kSystemUtilityError, msg: 'timezone'}},
        memoryResult:
            {error: {type: errorTypeEnum.kServiceUnavailable, msg: 'memory'}},
        backlightResult:
            {error: {type: errorTypeEnum.kParseError, msg: 'backlight'}},
        fanResult:
            {error: {type: errorTypeEnum.kSystemUtilityError, msg: 'fan'}},
        statefulPartitionResult: {
          error: {type: errorTypeEnum.kServiceUnavailable, msg: 'partition'}
        },
        bluetoothResult:
            {error: {type: errorTypeEnum.kParseError, msg: 'bluetooth'}}
      }));

  testDone();
});

// Tests that Telemetry.convert method correctly converts Mojo types into WebIDL
// types.
TEST_F(
    'TelemetryExtensionUIBrowserTest', 'ConvertTelemetryMojoTypesToWebIDLTypes',
    () => {
      // null/undefined --> null.
      assertEquals(null, telemetryProxy.convert(null));
      assertEquals(null, telemetryProxy.convert(undefined));

      // number/string --> number/string.
      assertEquals('simple string', telemetryProxy.convert('simple string'));
      assertEquals(2020, telemetryProxy.convert(2020));

      // {value: X} --> X if X is a number.
      assertEquals(15, telemetryProxy.convert({value: 15}));
      assertEquals(777.555, telemetryProxy.convert({value: 777.555}));

      // {value: X} --> X if X is a booelan.
      assertEquals(false, telemetryProxy.convert({value: false}));
      assertEquals(true, telemetryProxy.convert({value: true}));

      // {value: X} --> {value: X} if X is not a number.
      assertDeepEquals({value: 'ABC'}, telemetryProxy.convert({value: 'ABC'}));
      assertDeepEquals(
          {value: {k: 'v'}}, telemetryProxy.convert({value: {k: 'v'}}));

      // omit null/undefined properties.
      assertDeepEquals(
          {a: 1}, telemetryProxy.convert({a: 1, b: null, c: undefined}));
      assertDeepEquals(
          {a: {z: 'zZz'}},
          telemetryProxy.convert({a: {x: null, y: undefined, z: 'zZz'}}));

      // convert arrays.
      assertDeepEquals([], telemetryProxy.convert([]));
      assertDeepEquals([], telemetryProxy.convert([null, undefined]));
      assertDeepEquals([1], telemetryProxy.convert([{value: 1}]));
      assertDeepEquals(
          [{a: 1}, []], telemetryProxy.convert([null, undefined, {a: 1}, []]));
      assertDeepEquals([{x: 'xxx'}], telemetryProxy.convert([
        {a: null, b: undefined}, {x: 'xxx', y: null}
      ]));

      // convert objects without properties to null.
      assertEquals(null, telemetryProxy.convert({}));
      assertEquals(null, telemetryProxy.convert({a: null, b: undefined}));
      assertEquals(null, telemetryProxy.convert({a: {x: null, y: undefined}}));

      assertDeepEquals(
          {a: 1, c: {x: 1000, y: 'YYY', z: {value: 'ZzZ'}}},
          telemetryProxy.convert({
            a: 1,
            b: null,
            c: {x: {value: 1000}, y: 'YYY', z: {value: 'ZzZ'}}
          }));

      testDone();
    });

var TelemetryExtensionUIWithInteractiveRoutineUpdateBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('ConfigureDiagnosticsForInteractiveUpdate();');
  }
}

var TelemetryExtensionUIWithNonInteractiveRoutineUpdateBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('ConfigureDiagnosticsForNonInteractiveUpdate();');
  }
}

var TelemetryExtensionUIWithProbeServiceErrorsBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('ConfigureProbeServiceToReturnErrors();');
  }
}

var TelemetryExtensionUIWithAdapterAddedEventBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('EmitBluetoothAdapterAddedEventPeriodically();');
  }
}

var TelemetryExtensionUIWithAdapterRemovedEventBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('EmitBluetoothAdapterRemovedEventPeriodically();');
  }
}

var TelemetryExtensionUIWithAdapterPropertyChangedEventBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('EmitBluetoothAdapterPropertyChangedEventPeriodically();');
  }
}

var TelemetryExtensionUIWithDeviceAddedEventBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('EmitBluetoothDeviceAddedEventPeriodically();');
  }
}

var TelemetryExtensionUIWithDeviceRemovedEventBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('EmitBluetoothDeviceRemovedEventPeriodically();');
  }
}

var TelemetryExtensionUIWithDevicePropertyChangedEventBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('EmitBluetoothDevicePropertyChangedEventPeriodically();');
  }
}

var TelemetryExtensionUIWithLidClosedEventBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('EmitLidClosedEventPeriodically();');
  }
}

var TelemetryExtensionUIWithLidOpenedEventBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('EmitLidOpenedEventPeriodically();');
  }
}

var TelemetryExtensionUIWithAcInsertedEventBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('EmitAcInsertedEventPeriodically();');
  }
}

var TelemetryExtensionUIWithAcRemovedEventBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('EmitAcRemovedEventPeriodically();');
  }
}

var TelemetryExtensionUIWithOsSuspendEventBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('EmitOsSuspendEventPeriodically();');
  }
}

var TelemetryExtensionUIWithOsResumeEventBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('EmitOsResumeEventPeriodically();');
  }
}

// Test cases injected into the untrusted context.
// See implementations in untrusted_browsertest.js.
//
// To register a test with TelemetryExtensionUIBrowserTest class add an array
// with the test name from untrusted_browsertest.js.
// Example: ['UntrustedAwesomeTest']
//
// To register a test with a custom test class add an array with the test name
// from untrusted_browsertest.js and test class name whose definition needs to
// be above untrustedTests definition.
// Example: ['UntrustedAwesomeTest', 'MyAwesomeTestClass']
const untrustedTests = [
  ['UntrustedCanSpawnWorkers'],
  ['UntrustedDiagnosticsRequestAvailableRoutines'],
  ['UntrustedDiagnosticsRequestRoutineUpdateUnknownArguments'],
  ['UntrustedDiagnosticsRequestRunBatteryCapacityRoutine'],
  ['UntrustedDiagnosticsRequestRunBatteryHealthRoutine'],
  ['UntrustedDiagnosticsRequestRunSmartctlCheckRoutine'],
  ['UntrustedDiagnosticsRequestRunAcPowerRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunAcPowerRoutine'],
  ['UntrustedDiagnosticsRequestRunCpuCacheRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunCpuCacheRoutine'],
  ['UntrustedDiagnosticsRequestRunCpuStressRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunCpuStressRoutine'],
  ['UntrustedDiagnosticsRequestRunFPAccuracyRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunFPAccuracyRoutine'],
  ['UntrustedDiagnosticsRequestRunNvmeWearLevelRoutine'],
  ['UntrustedDiagnosticsRequestRunNvmeSelfTestRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunNvmeSelfTestRoutine'],
  ['UntrustedDiagnosticsRequestRunDiskReadRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunDiskReadRoutine'],
  ['UntrustedDiagnosticsRequestRunPrimeSearchRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunPrimeSearchRoutine'],
  ['UntrustedDiagnosticsRequestRunBatteryDischargeRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunBatteryDischargeRoutine'],
  ['UntrustedDiagnosticsRequestRunBatteryChargeRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunBatteryChargeRoutine'],
  ['UntrustedRequestTelemetryInfoUnknownCategory'],
  ['UntrustedRequestTelemetryInfo'],
  [
    'UntrustedDiagnosticsInteractiveRoutineCommand',
    'TelemetryExtensionUIWithInteractiveRoutineUpdateBrowserTest'
  ],
  [
    'UntrustedDiagnosticsNonInteractiveRoutineCommand',
    'TelemetryExtensionUIWithNonInteractiveRoutineUpdateBrowserTest'
  ],
  [
    'UntrustedRequestTelemetryInfoWithErrors',
    'TelemetryExtensionUIWithProbeServiceErrorsBrowserTest'
  ],
  [
    'UntrustedBluetoothAdapterAddedEventListener',
    'TelemetryExtensionUIWithAdapterAddedEventBrowserTest'
  ],
  [
    'UntrustedBluetoothAdapterRemovedEventListener',
    'TelemetryExtensionUIWithAdapterRemovedEventBrowserTest'
  ],
  [
    'UntrustedBluetoothAdapterPropertyChangedEventListener',
    'TelemetryExtensionUIWithAdapterPropertyChangedEventBrowserTest'
  ],
  [
    'UntrustedBluetoothDeviceAddedEventListener',
    'TelemetryExtensionUIWithDeviceAddedEventBrowserTest'
  ],
  [
    'UntrustedBluetoothDeviceRemovedEventListener',
    'TelemetryExtensionUIWithDeviceRemovedEventBrowserTest'
  ],
  [
    'UntrustedBluetoothDevicePropertyChangedEventListener',
    'TelemetryExtensionUIWithDevicePropertyChangedEventBrowserTest'
  ],
  [
    'UntrustedLidClosedEventListener',
    'TelemetryExtensionUIWithLidClosedEventBrowserTest'
  ],
  [
    'UntrustedLidOpenedEventListener',
    'TelemetryExtensionUIWithLidOpenedEventBrowserTest'
  ],
  [
    'UntrustedAcInsertedEventListener',
    'TelemetryExtensionUIWithAcInsertedEventBrowserTest'
  ],
  [
    'UntrustedAcRemovedEventListener',
    'TelemetryExtensionUIWithAcRemovedEventBrowserTest'
  ],
  [
    'UntrustedOsSuspendEventListener',
    'TelemetryExtensionUIWithOsSuspendEventBrowserTest'
  ],
  [
    'UntrustedOsResumeEventListener',
    'TelemetryExtensionUIWithOsResumeEventBrowserTest'
  ],
  ['UntrustedEventsServiceGetAvailableEvents'],
].forEach(test => registerUntrustedTest(...test));

/**
 * Registers a test in chrome-untrusted://.
 * @param {!string} testName
 * @param {!string=} testClass
 */
function registerUntrustedTest(
    testName, testClass = 'TelemetryExtensionUIBrowserTest') {
  TEST_F(testClass, testName, async () => {
    await runTestInUntrusted(testName);
    testDone();
  });
}

/**
 * @implements {ash.health.mojom.ProbeServiceInterface}
 */
class TestProbeService {
  constructor() {
    /**
     * @type {ash.health.mojom.ProbeServiceReceiver}
     */
    this.receiver_ = null;

    /**
     * History of the passed argument (categories) of probeTelemetryInfo method.
     * @type {Array<Array<!ash.health.mojom.ProbeCategoryEnum>>}
     */
    this.probeTelemetryInfoArgsHistory = [];
  }

  /**
   * @param {!MojoHandle} handle
   */
  bind(handle) {
    this.receiver_ = new ash.health.mojom.ProbeServiceReceiver(this);
    this.receiver_.$.bindHandle(handle);
  }

  /**
   * @override
   * @param { !Array<!ash.health.mojom.ProbeCategoryEnum> } categories
   * @return {!Promise<{telemetryInfo:
   *     !ash.health.mojom.TelemetryInfo}>}
   */
  probeTelemetryInfo(categories) {
    this.probeTelemetryInfoArgsHistory.push(categories);

    const telemetryInfo =
        /** @type {!ash.health.mojom.TelemetryInfo} */ ({
        backlightResult: null,
        batteryResult: null,
        blockDeviceResult: null,
        bluetoothResult: null,
        cpuResult: null,
        fanResult: null,
        memoryResult: null,
        statefulPartitionResult: null,
        timezoneResult: null,
        vpdResult: null,
      });

    return Promise.resolve({ telemetryInfo });
  }

  /**
   * @override
   */
  getOemData() {
    throw 'This method should not be called from telemetry extension';
  }
};

// Tests with a testing Mojo probe service, so we can test for example strings
// conversion to Mojo enum values.
var TelemetryExtensionUIWithInterceptorBrowserTest =
  class extends TelemetryExtensionUIBrowserTest {
    constructor() {
      super();

      /**
       * @type {TestProbeService}
       */
      this.probeService = null;

      this.probeServiceInterceptor = null;
    }

    /** @override */
    setUp() {
      this.probeService = new TestProbeService();

      /** @suppress {undefinedVars} */
      this.probeServiceInterceptor = new MojoInterfaceInterceptor(
        ash.health.mojom.ProbeService.$interfaceName);
      this.probeServiceInterceptor.oninterfacerequest = (e) => {
        this.probeService.bind(e.handle);
      };
      this.probeServiceInterceptor.start();
    }
  };

// Test cases injected into the untrusted context.
// See implementations in untrusted_browsertest.js.

// Tests that telemetry methods send the correct categories.
TEST_F(
  'TelemetryExtensionUIWithInterceptorBrowserTest',
  'UntrustedRequestTelemetryInfoWithInterceptor', async function () {
    await runTestInUntrusted('UntrustedRequestTelemetryInfoWithInterceptor');

    const allCategories = [
      ash.health.mojom.ProbeCategoryEnum.kBattery,
      ash.health.mojom.ProbeCategoryEnum.kNonRemovableBlockDevices,
      ash.health.mojom.ProbeCategoryEnum.kCachedVpdData,
      ash.health.mojom.ProbeCategoryEnum.kCpu,
      ash.health.mojom.ProbeCategoryEnum.kTimezone,
      ash.health.mojom.ProbeCategoryEnum.kMemory,
      ash.health.mojom.ProbeCategoryEnum.kBacklight,
      ash.health.mojom.ProbeCategoryEnum.kFan,
      ash.health.mojom.ProbeCategoryEnum.kStatefulPartition,
      ash.health.mojom.ProbeCategoryEnum.kBluetooth
    ];

    // This comparison respects the method calls order in
    // UntrustedRequestTelemetryInfoWithInterceptor test in
    // untrusted_browsertest.js
    assertDeepEquals(this.probeService.probeTelemetryInfoArgsHistory, [
      allCategories,
      [ash.health.mojom.ProbeCategoryEnum.kBacklight],
      [ash.health.mojom.ProbeCategoryEnum.kBattery],
      [ash.health.mojom.ProbeCategoryEnum.kNonRemovableBlockDevices],
      [ash.health.mojom.ProbeCategoryEnum.kCachedVpdData],
      [ash.health.mojom.ProbeCategoryEnum.kCpu],
      [ash.health.mojom.ProbeCategoryEnum.kTimezone],
      [ash.health.mojom.ProbeCategoryEnum.kMemory],
      [ash.health.mojom.ProbeCategoryEnum.kFan],
      [ash.health.mojom.ProbeCategoryEnum.kStatefulPartition],
      [ash.health.mojom.ProbeCategoryEnum.kBluetooth],
    ]);

      testDone();
    });


/**
 * @implements {ash.health.mojom.DiagnosticsServiceInterface}
 */
class TestDiagnosticsService {
  constructor() {
    /**
     * @type {ash.health.mojom.DiagnosticsServiceReceiver}
     */
    this.receiver_ = null;

    /**
     * @type {!ash.health.mojom.RunRoutineResponse}
     */
    this.routineResponse =
        /** @type {!ash.health.mojom.RunRoutineResponse}*/ (
            {id: 123, status: 'ready'});

    /**
     * @type {!Object}
     * @private
     */
    this.noninteractiveRoutineUpdateResponse = {
      progressPercent: 0,
      output: '',
      routineUpdateUnion: {
        noninteractiveUpdate:
            {status: 'ready', statusMessage: 'Routine ran by Google.'}
      }
    };

    /**
     * @type {!Object}
     * @private
     */
    this.interactiveRoutineUpdateResponse = {
      progressPercent: 50,
      output: 'This routine is running!',
      routineUpdateUnion: {interactiveUpdate: {userMessage: 'unplug-ac-power'}}
    };

    /**
     * @type {!Object}
     * @private
     */
    this.routineUpdateResponse = this.noninteractiveRoutineUpdateResponse;

    /**
     * History of the called method and passed argument in this class's.
     * @type {Array<!Array<!Object>>}
     */
    this.callHistory = [];
  }

  setInteractiveUpdateResponse() {
    this.routineUpdateResponse = this.interactiveRoutineUpdateResponse;
  }

  /**
   * @param {!MojoHandle} handle
   */
  bind(handle) {
    this.receiver_ = new ash.health.mojom.DiagnosticsServiceReceiver(this);
    this.receiver_.$.bindHandle(handle);
  }

  /** @override */
  getAvailableRoutines() {}

  /**
   * @override
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runBatteryCapacityRoutine() {
    this.callHistory.push(['runBatteryCapacityRoutine']);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @override
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runBatteryHealthRoutine() {
    this.callHistory.push(['runBatteryHealthRoutine']);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @param {!number} lengthSeconds
   * @param {!number} maximumDischargePercentAllowed
   * @override
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runBatteryDischargeRoutine(lengthSeconds, maximumDischargePercentAllowed) {
    this.callHistory.push([
      'runBatteryDischargeRoutine', {
        lengthSeconds: lengthSeconds,
        maximumDischargePercentAllowed: maximumDischargePercentAllowed
      }
    ]);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @param {!number} lengthSeconds
   * @param {!number} minimumChargePercentRequired
   * @override
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runBatteryChargeRoutine(lengthSeconds, minimumChargePercentRequired) {
    this.callHistory.push([
      'runBatteryChargeRoutine', {
        lengthSeconds: lengthSeconds,
        minimumChargePercentRequired: minimumChargePercentRequired
      }
    ]);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @override
   * @param {!number} routineId
   * @param {!ash.health.mojom.DiagnosticRoutineCommandEnum} command
   * @param {!boolean} includeOutput
   * @return { !Promise<{routineUpdate: !ash.health.mojom.RoutineUpdate}> }
   */
  getRoutineUpdate(routineId, command, includeOutput) {
    this.callHistory.push([
      'getRoutineUpdate',
      {routineId: routineId, command: command, includeOutput: includeOutput}
    ]);

    let routineUpdate = /** @type {!ash.health.mojom.RoutineUpdate} */ (
        this.routineUpdateResponse);
    return Promise.resolve({routineUpdate});
  }

  /**
   * @override
   * @param {!ash.health.mojom.NvmeSelfTestTypeEnum} type
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runNvmeSelfTestRoutine(type) {
    this.callHistory.push(['runNvmeSelfTestRoutine', {type: type}]);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @override
   * @param {!number} threshold
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runNvmeWearLevelRoutine(threshold) {
    this.callHistory.push(['runNvmeWearLevelRoutine', {threshold: threshold}]);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @override
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runSmartctlCheckRoutine() {
    this.callHistory.push(['runSmartctlCheckRoutine']);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @override
   * @param {!ash.health.mojom.AcPowerStatusEnum} expectedStatus
   * @param {?string} expectedPowerType
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runAcPowerRoutine(expectedStatus, expectedPowerType) {
    this.callHistory.push([
      'runAcPowerRoutine',
      {expectedStatus: expectedStatus, expectedPowerType: expectedPowerType}
    ]);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @override
   * @param {!number} duration
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runCpuCacheRoutine(duration) {
    this.callHistory.push(['runCpuCacheRoutine', {duration: duration}]);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @override
   * @param {!number} duration
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runCpuStressRoutine(duration) {
    this.callHistory.push(['runCpuStressRoutine', {duration: duration}]);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @override
   * @param {!number} duration
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runFloatingPointAccuracyRoutine(duration) {
    this.callHistory.push(
        ['runFloatingPointAccuracyRoutine', {duration: duration}]);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @override
   * @param {!number} lengthSeconds
   * @param {!bigint} maxNum
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runPrimeSearchRoutine(lengthSeconds, maxNum) {
    this.callHistory.push([
      'runPrimeSearchRoutine', {lengthSeconds: lengthSeconds, maxNum: maxNum}
    ]);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @override
   * @param {!ash.health.mojom.DiskReadRoutineTypeEnum} type
   * @param {!number} lengthSeconds
   * @param {!number} fileSizeMB
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runDiskReadRoutine(type, lengthSeconds, fileSizeMB) {
    this.callHistory.push([
      'runDiskReadRoutine',
      {type: type, lengthSeconds: lengthSeconds, fileSizeMB: fileSizeMB}
    ]);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }

  /**
   * @override
   * @return {!Promise<{response: !ash.health.mojom.RunRoutineResponse}>}
   */
  runMemoryRoutine() {
    this.callHistory.push([
      'runMemoryRoutine',
      {}
    ]);

    let response = this.routineResponse;
    return Promise.resolve({response});
  }
};

// Tests with a fake Mojo diagnostics service.
var TelemetryExtensionUIWithDiagnosticsInterceptorBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  constructor() {
    super();

    /**
     * @type {TestDiagnosticsService}
     */
    this.diagnosticsService = null;

    this.diagnosticsServiceInterceptor = null;
  }

  /** @override */
  setUp() {
    this.diagnosticsService = new TestDiagnosticsService();

    /** @suppress {undefinedVars} */
    this.diagnosticsServiceInterceptor = new MojoInterfaceInterceptor(
        ash.health.mojom.DiagnosticsService.$interfaceName);
    this.diagnosticsServiceInterceptor.oninterfacerequest = (e) => {
      this.diagnosticsService.bind(e.handle);
    };
    this.diagnosticsServiceInterceptor.start();
  }
};

// See implementations in untrusted_browsertest.js.

// Tests that a routine is created and routine.{getStatus(), resume(), stop()}
// send the correct parameters to the fake Mojo service.
TEST_F(
    'TelemetryExtensionUIWithDiagnosticsInterceptorBrowserTest',
    'UntrustedDiagnosticsRoutineCommandWithInterceptor', async function() {
      await runTestInUntrusted(
          'UntrustedDiagnosticsRoutineCommandWithInterceptor');

      assertDeepEquals(
          [
            ['runBatteryCapacityRoutine'],
            [
              'getRoutineUpdate', {
                routineId: 123,
                command: ash.health.mojom.DiagnosticRoutineCommandEnum
                             .kGetStatus,
                includeOutput: true
              }
            ],
            [
              'getRoutineUpdate', {
                routineId: 123,
                command: ash.health.mojom.DiagnosticRoutineCommandEnum
                             .kContinue,
                includeOutput: true
              }
            ],
            [
              'getRoutineUpdate', {
                routineId: 123,
                command:
                    ash.health.mojom.DiagnosticRoutineCommandEnum.kCancel,
                includeOutput: true
              }
            ],
            [
              'getRoutineUpdate', {
                routineId: 123,
                command:
                    ash.health.mojom.DiagnosticRoutineCommandEnum.kRemove,
                includeOutput: true
              }
            ]
          ],
          this.diagnosticsService.callHistory);

      testDone();
    });

// Tests that diagnostics routines are created and correct parameters are sent
// to the fake Mojo service.
TEST_F(
    'TelemetryExtensionUIWithDiagnosticsInterceptorBrowserTest',
    'UntrustedDiagnosticsRunRoutineWithInterceptor', async function() {
      await runTestInUntrusted('UntrustedDiagnosticsRunRoutineWithInterceptor');

      assertDeepEquals(
          [
            ['runBatteryCapacityRoutine'], ['runBatteryHealthRoutine'],
            [
              'runBatteryDischargeRoutine',
              {lengthSeconds: 7, maximumDischargePercentAllowed: 50}
            ],
            [
              'runBatteryChargeRoutine',
              {lengthSeconds: 13, minimumChargePercentRequired: 87}
            ],
            ['runSmartctlCheckRoutine'],
            ['runNvmeWearLevelRoutine', {threshold: 37}],
            [
              'runNvmeSelfTestRoutine',
              {type: ash.health.mojom.NvmeSelfTestTypeEnum.kShortSelfTest}
            ],
            [
              'runNvmeSelfTestRoutine',
              {type: ash.health.mojom.NvmeSelfTestTypeEnum.kLongSelfTest}
            ],
            [
              'runAcPowerRoutine', {
                expectedStatus:
                    ash.health.mojom.AcPowerStatusEnum.kConnected,
                expectedPowerType: null
              }
            ],
            [
              'runAcPowerRoutine', {
                expectedStatus:
                    ash.health.mojom.AcPowerStatusEnum.kDisconnected,
                expectedPowerType: null
              }
            ],
            [
              'runAcPowerRoutine', {
                expectedStatus:
                    ash.health.mojom.AcPowerStatusEnum.kConnected,
                expectedPowerType: 'Mains'
              }
            ],
            [
              'runAcPowerRoutine', {
                expectedStatus:
                    ash.health.mojom.AcPowerStatusEnum.kDisconnected,
                expectedPowerType: 'Battery'
              }
            ],
            ['runCpuCacheRoutine', {duration: 30}],
            ['runCpuStressRoutine', {duration: 17}],
            ['runFloatingPointAccuracyRoutine', {duration: 94}],
            [
              'runPrimeSearchRoutine',
              {lengthSeconds: 45, maxNum: BigInt(1110987654321)}
            ],
            [
              'runDiskReadRoutine', {
                type: ash.health.mojom.DiskReadRoutineTypeEnum.kLinearRead,
                lengthSeconds: 44,
                fileSizeMB: 135
              }
            ],
            [
              'runDiskReadRoutine', {
                type: ash.health.mojom.DiskReadRoutineTypeEnum.kRandomRead,
                lengthSeconds: 23,
                fileSizeMB: 1749
              }
            ]
          ],
          this.diagnosticsService.callHistory);

      testDone();
    });
