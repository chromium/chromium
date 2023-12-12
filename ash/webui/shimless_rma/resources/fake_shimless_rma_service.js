// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {CalibrationComponentStatus, CalibrationObserverRemote, CalibrationOverallStatus, CalibrationSetupInstruction, CalibrationStatus, Component, ComponentType, ErrorObserverRemote, ExternalDiskStateObserverRemote, FeatureLevel, FinalizationError, FinalizationObserverRemote, FinalizationStatus, HardwareVerificationStatusObserverRemote, HardwareWriteProtectionStateObserverRemote, OsUpdateObserverRemote, OsUpdateOperation, PowerCableStateObserverRemote, ProvisioningError, ProvisioningObserverRemote, ProvisioningStatus, QrCode, RmadErrorCode, Shimless3pDiagnosticsAppInfo, ShimlessRmaServiceInterface, Show3pDiagnosticsAppResult, ShutdownMethod, State, StateResult, UpdateErrorCode, UpdateRoFirmwareObserverRemote, UpdateRoFirmwareStatus, WriteProtectDisableCompleteAction} from './shimless_rma.mojom-webui.js';

/** @implements {ShimlessRmaServiceInterface} */
export class FakeShimlessRmaService {
  constructor() {
    this.methods = new FakeMethodResolver();
    this.observables = new FakeObservables();

    /**
     * The list of states for this RMA flow.
     * @private {!Array<!StateResult>}
     */
    this.states = [];

    /**
     * The index into states for the current fake state.
     * @private {number}
     */
    this.stateIndex = 0;

    /**
     * The list of components.
     * @private {!Array<!Component>}
     */
    this.components = [];

    /**
     * Control automatically triggering a HWWP disable observation.
     * @private {boolean}
     */
    this.automaticallyTriggerDisableWriteProtectionObservation = false;

    /**
     * Control automatically triggering update RO firmware observations.
     * @private {boolean}
     */
    this.automaticallyTriggerUpdateRoFirmwareObservation = false;

    /**
     * Control automatically triggering provisioning observations.
     * @private {boolean}
     */
    this.automaticallyTriggerProvisioningObservation = false;

    /**
     * Control automatically triggering calibration observations.
     * @private {boolean}
     */
    this.automaticallyTriggerCalibrationObservation = false;

    /**
     * Control automatically triggering OS update observations.
     * @private {boolean}
     */
    this.automaticallyTriggerOsUpdateObservation = false;

    /**
     * Control automatically triggering a hardware verification observation.
     * @private {boolean}
     */
    this.automaticallyTriggerHardwareVerificationStatusObservation = false;

    /**
     * Control automatically triggering a finalization observation.
     * @private {boolean}
     */
    this.automaticallyTriggerFinalizationObservation = false;

    /**
     * Control automatically triggering power cable state observations.
     * @private {boolean}
     */
    this.automaticallyTriggerPowerCableStateObservation = false;

    /**
     * Both abortRma and forward state transitions can have significant delays
     * that are useful to fake for manual testing.
     * Defaults to no delay for unit tests.
     * @private {number}
     */
    this.resolveMethodDelayMs = 0;

    /**
     * The result of calling trackConfiguredNetworks.
     * @private {boolean}
     */
    this.trackConfiguredNetworksCalled = false;

    /**
     * The approval of last call to completeLast3pDiagnosticsInstallation.
     * @private {?boolean}
     */
    this.lastCompleteLast3pDiagnosticsInstallationApproval = null;

    /**
     * Has show3pDiagnosticsApp been called.
     * @private {boolean}
     */
    this.wasShow3pDiagnosticsAppCalled = false;

    this.reset();
  }

  /** @param {number} delayMs */
  setAsyncOperationDelayMs(delayMs) {
    this.resolveMethodDelayMs = delayMs;
  }

  /**
   * Set the ordered list of states end error codes for this fake.
   * Setting an empty list (the default) returns kRmaNotRequired for any state
   * function.
   * Next state functions and transitionPreviousState will move through the fake
   * state through the list, and return kTransitionFailed if it would move off
   * either end. getCurrentState always return the state at the current index.
   *
   * @param {!Array<!StateResult>} states
   */
  setStates(states) {
    this.states = states;
    this.stateIndex = 0;
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  getCurrentState() {
    // As next state functions and transitionPreviousState can modify the result
    // of this function the result must be set at the time of the call.
    if (this.states.length === 0) {
      this.setFakeCurrentState(
          State.kUnknown, false, false, RmadErrorCode.kRmaNotRequired);
    } else {
      // It should not be possible for stateIndex to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex < this.states.length);
      const state = this.states[this.stateIndex];
      this.setFakeCurrentState(
          state.state, state.canExit, state.canGoBack, state.error);
    }
    return this.methods.resolveMethodWithDelay(
        'getCurrentState', this.resolveMethodDelayMs);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  transitionPreviousState() {
    // As next state methods and transitionPreviousState can modify the result
    // of this function the result must be set at the time of the call.
    if (this.states.length === 0) {
      this.setFakePrevState(
          State.kUnknown, false, false, RmadErrorCode.kRmaNotRequired);
    } else if (this.stateIndex === 0) {
      // It should not be possible for stateIndex to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex < this.states.length);
      const state = this.states[this.stateIndex];
      this.setFakePrevState(
          state.state, state.canExit, state.canGoBack,
          RmadErrorCode.kTransitionFailed);
    } else {
      this.stateIndex--;
      const state = this.states[this.stateIndex];
      this.setFakePrevState(
          state.state, state.canExit, state.canGoBack, state.error);
    }
    return this.methods.resolveMethodWithDelay(
        'transitionPreviousState', this.resolveMethodDelayMs);
  }

  /**
   * @return {!Promise<!{error: !RmadErrorCode}>}
   */
  abortRma() {
    return this.methods.resolveMethodWithDelay(
        'abortRma', this.resolveMethodDelayMs);
  }

  /**
   * Sets the value that will be returned when calling abortRma().
   * @param {!RmadErrorCode} error
   */
  setAbortRmaResult(error) {
    this.methods.setResult('abortRma', {error: error});
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  beginFinalization() {
    return this.getNextStateForMethod(
        'beginFinalization', State.kWelcomeScreen);
  }

  trackConfiguredNetworks() {
    this.trackConfiguredNetworksCalled = true;
  }

  getTrackConfiguredNetworks() {
    return this.trackConfiguredNetworksCalled;
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  networkSelectionComplete() {
    return this.getNextStateForMethod(
        'networkSelectionComplete', State.kConfigureNetwork);
  }

  /**
   * @return {!Promise<!{version: string}>}
   */
  getCurrentOsVersion() {
    return this.methods.resolveMethod('getCurrentOsVersion');
  }

  /**
   * @param {null|string} version
   */
  setGetCurrentOsVersionResult(version) {
    this.methods.setResult('getCurrentOsVersion', {version: version});
  }

  /**
   * @return {!Promise<!{updateAvailable: boolean, version: string}>}
   */
  checkForOsUpdates() {
    return this.methods.resolveMethod('checkForOsUpdates');
  }

  /** @param {string} version */
  setCheckForOsUpdatesResult(version) {
    this.methods.setResult(
        'checkForOsUpdates', {updateAvailable: true, version});
  }

  /**
   * @return {!Promise<!{updateStarted: boolean}>}
   */
  updateOs() {
    if (this.automaticallyTriggerOsUpdateObservation) {
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kCheckingForUpdate, 0.1, UpdateErrorCode.kSuccess,
          500);
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kUpdateAvailable, 0.3, UpdateErrorCode.kSuccess,
          1000);
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kDownloading, 0.5, UpdateErrorCode.kSuccess, 1500);
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kVerifying, 0.7, UpdateErrorCode.kSuccess, 2000);
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kFinalizing, 0.9, UpdateErrorCode.kSuccess, 2500);
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kUpdatedNeedReboot, 1.0, UpdateErrorCode.kSuccess,
          3000);
    }
    return this.methods.resolveMethod('updateOs');
  }

  /**
   * @param {boolean} started
   */
  setUpdateOsResult(started) {
    this.methods.setResult('updateOs', {updateStarted: started});
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  updateOsSkipped() {
    return this.getNextStateForMethod('updateOsSkipped', State.kUpdateOs);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  setSameOwner() {
    return this.getNextStateForMethod('setSameOwner', State.kChooseDestination);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  setDifferentOwner() {
    return this.getNextStateForMethod(
        'setDifferentOwner', State.kChooseDestination);
  }

  /**
   * @param {boolean} shouldWipeDevice
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  setWipeDevice(shouldWipeDevice) {
    return this.getNextStateForMethod('setWipeDevice', State.kChooseWipeDevice);
  }

  /**
   * @return {!Promise<!{available: boolean}>}
   */
  manualDisableWriteProtectAvailable() {
    return this.methods.resolveMethod('manualDisableWriteProtectAvailable');
  }

  /**
   * @param {boolean} available
   */
  setManualDisableWriteProtectAvailableResult(available) {
    this.methods.setResult(
        'manualDisableWriteProtectAvailable', {available: available});
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  chooseManuallyDisableWriteProtect() {
    return this.getNextStateForMethod(
        'chooseManuallyDisableWriteProtect',
        State.kChooseWriteProtectDisableMethod);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  chooseRsuDisableWriteProtect() {
    return this.getNextStateForMethod(
        'chooseRsuDisableWriteProtect', State.kChooseWriteProtectDisableMethod);
  }

  /**
   * @return {!Promise<!{challenge: string}>}
   */
  getRsuDisableWriteProtectChallenge() {
    return this.methods.resolveMethod('getRsuDisableWriteProtectChallenge');
  }

  /**
   * @param {string} challenge
   */
  setGetRsuDisableWriteProtectChallengeResult(challenge) {
    this.methods.setResult(
        'getRsuDisableWriteProtectChallenge', {challenge: challenge});
  }

  /**
   * @return {!Promise<!{hwid: string}>}
   */
  getRsuDisableWriteProtectHwid() {
    return this.methods.resolveMethod('getRsuDisableWriteProtectHwid');
  }

  /**
   * @param {string} hwid
   */
  setGetRsuDisableWriteProtectHwidResult(hwid) {
    this.methods.setResult('getRsuDisableWriteProtectHwid', {hwid: hwid});
  }

  /**
   * @return {!Promise<!{qrCodeData: !Array<number>}>}
   */
  getRsuDisableWriteProtectChallengeQrCode() {
    return this.methods.resolveMethod(
        'getRsuDisableWriteProtectChallengeQrCode');
  }

  /**
   * @param {!Array<number>} qrCodeData
   */
  setGetRsuDisableWriteProtectChallengeQrCodeResponse(qrCodeData) {
    this.methods.setResult(
        'getRsuDisableWriteProtectChallengeQrCode', {qrCodeData: qrCodeData});
  }

  /**
   * @param {string} code
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  setRsuDisableWriteProtectCode(code) {
    return this.getNextStateForMethod(
        'setRsuDisableWriteProtectCode', State.kEnterRSUWPDisableCode);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  writeProtectManuallyDisabled() {
    return this.getNextStateForMethod(
        'writeProtectManuallyDisabled', State.kWaitForManualWPDisable);
  }

  /** @return {!Promise<!{action: !WriteProtectDisableCompleteAction}>} */
  getWriteProtectDisableCompleteAction() {
    return this.methods.resolveMethod('getWriteProtectDisableCompleteAction');
  }

  /** @param {!WriteProtectDisableCompleteAction} action */
  setGetWriteProtectDisableCompleteAction(action) {
    this.methods.setResult(
        'getWriteProtectDisableCompleteAction', {action: action});
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  confirmManualWpDisableComplete() {
    return this.getNextStateForMethod(
        'confirmManualWpDisableComplete', State.kWPDisableComplete);
  }

  /**
   * @return {!Promise<!{components: !Array<!Component>}>}
   */
  getComponentList() {
    this.methods.setResult('getComponentList', {components: this.components});
    return this.methods.resolveMethod('getComponentList');
  }

  /**
   * @param {!Array<!Component>} components
   */
  setGetComponentListResult(components) {
    this.components = components;
  }

  /**
   * @param {!Array<!Component>} components
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  setComponentList(components) {
    return this.getNextStateForMethod(
        'setComponentList', State.kSelectComponents);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  reworkMainboard() {
    return this.getNextStateForMethod(
        'reworkMainboard', State.kSelectComponents);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  roFirmwareUpdateComplete() {
    return this.getNextStateForMethod(
        'roFirmwareUpdateComplete', State.kUpdateRoFirmware);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   *
   */
  shutdownForRestock() {
    return this.getNextStateForMethod('shutdownForRestock', State.kRestock);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  continueFinalizationAfterRestock() {
    return this.getNextStateForMethod(
        'continueFinalizationAfterRestock', State.kRestock);
  }

  /**
   * @return {!Promise<!{regions: !Array<string>}>}
   */
  getRegionList() {
    return this.methods.resolveMethod('getRegionList');
  }

  /**
   * @param {!Array<string>} regions
   */
  setGetRegionListResult(regions) {
    this.methods.setResult('getRegionList', {regions: regions});
  }

  /**
   * @return {!Promise<!{skus: !Array<bigint>}>}
   */
  getSkuList() {
    return this.methods.resolveMethod('getSkuList');
  }

  /**
   * @param {!Array<bigint>} skus
   */
  setGetSkuListResult(skus) {
    this.methods.setResult('getSkuList', {skus: skus});
  }

  /**
   * @return {!Promise<!{customLabels: !Array<string>}>}
   */
  getCustomLabelList() {
    return this.methods.resolveMethod('getCustomLabelList');
  }

  /**
   * @param {!Array<string>} customLabels
   */
  setGetCustomLabelListResult(customLabels) {
    this.methods.setResult('getCustomLabelList', {customLabels: customLabels});
  }

  /**
   * @return {!Promise<!{skuDescriptions: !Array<string>}>}
   */
  getSkuDescriptionList() {
    return this.methods.resolveMethod('getSkuDescriptionList');
  }

  /**
   * @param {!Array<string>} skuDescriptions
   */
  setGetSkuDescriptionListResult(skuDescriptions) {
    this.methods.setResult(
        'getSkuDescriptionList', {skuDescriptions: skuDescriptions});
  }

  /**
   * @return {!Promise<!{serialNumber: string}>}
   */
  getOriginalSerialNumber() {
    return this.methods.resolveMethod('getOriginalSerialNumber');
  }

  /**
   * @param {string} serialNumber
   */
  setGetOriginalSerialNumberResult(serialNumber) {
    this.methods.setResult(
        'getOriginalSerialNumber', {serialNumber: serialNumber});
  }

  /**
   * @return {!Promise<!{regionIndex: number}>}
   */
  getOriginalRegion() {
    return this.methods.resolveMethod('getOriginalRegion');
  }

  /**
   * @param {number} regionIndex
   */
  setGetOriginalRegionResult(regionIndex) {
    this.methods.setResult('getOriginalRegion', {regionIndex: regionIndex});
  }

  /**
   * @return {!Promise<!{skuIndex: number}>}
   */
  getOriginalSku() {
    return this.methods.resolveMethod('getOriginalSku');
  }

  /**
   * @param {number} skuIndex
   */
  setGetOriginalSkuResult(skuIndex) {
    this.methods.setResult('getOriginalSku', {skuIndex: skuIndex});
  }

  /**
   * @return {!Promise<!{customLabelIndex: number}>}
   */
  getOriginalCustomLabel() {
    return this.methods.resolveMethod('getOriginalCustomLabel');
  }

  /**
   * @param {number} customLabelIndex
   */
  setGetOriginalCustomLabelResult(customLabelIndex) {
    this.methods.setResult(
        'getOriginalCustomLabel', {customLabelIndex: customLabelIndex});
  }

  /**
   * @return {!Promise<!{dramPartNumber: string}>}
   */
  getOriginalDramPartNumber() {
    return this.methods.resolveMethod('getOriginalDramPartNumber');
  }

  /**
   * @param {string} dramPartNumber
   */
  setGetOriginalDramPartNumberResult(dramPartNumber) {
    this.methods.setResult(
        'getOriginalDramPartNumber', {dramPartNumber: dramPartNumber});
  }

  /**
   * @return {!Promise<!{originalFeatureLevel: FeatureLevel}>}
   */
  getOriginalFeatureLevel() {
    return this.methods.resolveMethod('getOriginalFeatureLevel');
  }

  /**
   * @param {FeatureLevel} featureLevel
   */
  setGetOriginalFeatureLevelResult(featureLevel) {
    this.methods.setResult(
        'getOriginalFeatureLevel', {originalFeatureLevel: featureLevel});
  }

  /**
   * @param {string} serialNumber
   * @param {number} regionIndex
   * @param {number} skuIndex
   * @param {number} customLabelIndex
   * @param {string} dramPartNumber
   * @param {boolean} isChassisBranded
   * @param {number} hwComplianceVersion
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  setDeviceInformation(
      serialNumber, regionIndex, skuIndex, customLabelIndex, dramPartNumber,
      isChassisBranded, hwComplianceVersion) {
    // TODO(gavindodd): Validate range of region and sku.
    return this.getNextStateForMethod(
        'setDeviceInformation', State.kUpdateDeviceInformation);
  }

  /**
   * @return {!Promise<!{components: !Array<!CalibrationComponentStatus>}>}
   */
  getCalibrationComponentList() {
    return this.methods.resolveMethod('getCalibrationComponentList');
  }

  /**
   * @param {!Array<!CalibrationComponentStatus>} components
   */
  setGetCalibrationComponentListResult(components) {
    this.methods.setResult(
        'getCalibrationComponentList', {components: components});
  }

  /**
   * @return {!Promise<!{instructions: CalibrationSetupInstruction}>}
   */
  getCalibrationSetupInstructions() {
    return this.methods.resolveMethod('getCalibrationSetupInstructions');
  }

  /**
   * @param {CalibrationSetupInstruction} instructions
   */
  setGetCalibrationSetupInstructionsResult(instructions) {
    this.methods.setResult(
        'getCalibrationSetupInstructions', {instructions: instructions});
  }

  /**
   * The fake does not use the status list parameter, the fake data is never
   * updated.
   * @param {!Array<!CalibrationComponentStatus>} unused
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  startCalibration(unused) {
    return this.getNextStateForMethod(
        'startCalibration', State.kCheckCalibration);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  runCalibrationStep() {
    return this.getNextStateForMethod(
        'runCalibrationStep', State.kSetupCalibration);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  continueCalibration() {
    return this.getNextStateForMethod(
        'continueCalibration', State.kRunCalibration);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  calibrationComplete() {
    return this.getNextStateForMethod(
        'calibrationComplete', State.kRunCalibration);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  retryProvisioning() {
    return this.getNextStateForMethod(
        'retryProvisioning', State.kProvisionDevice);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  provisioningComplete() {
    return this.getNextStateForMethod(
        'provisioningComplete', State.kProvisionDevice);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  finalizationComplete() {
    return this.getNextStateForMethod('finalizationComplete', State.kFinalize);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  retryFinalization() {
    return this.getNextStateForMethod('retryFinalization', State.kFinalize);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  writeProtectManuallyEnabled() {
    return this.getNextStateForMethod(
        'writeProtectManuallyEnabled', State.kWaitForManualWPEnable);
  }

  /** @return {!Promise<{log: string, error: !RmadErrorCode}>} */
  getLog() {
    return this.methods.resolveMethod('getLog');
  }

  /** @param {string} log */
  setGetLogResult(log) {
    this.methods.setResult('getLog', {log: log, error: RmadErrorCode.kOk});
  }

  /**
   * @return {!Promise<{savePath: !FilePath, error:
   *     !RmadErrorCode}>}
   */
  saveLog() {
    return this.methods.resolveMethod('saveLog');
  }

  /** @param {!FilePath} savePath */
  setSaveLogResult(savePath) {
    this.methods.setResult(
        'saveLog', {savePath: savePath, error: RmadErrorCode.kOk});
  }

  /** @return {!Promise<{powerwashRequired: boolean, error: !RmadErrorCode}>} */
  getPowerwashRequired() {
    return this.methods.resolveMethod('getPowerwashRequired');
  }

  /** @param {boolean} powerwashRequired */
  setGetPowerwashRequiredResult(powerwashRequired) {
    this.methods.setResult(
        'getPowerwashRequired',
        {powerwashRequired: powerwashRequired, error: RmadErrorCode.kOk});
  }

  launchDiagnostics() {
    console.log('(Fake) Launching diagnostics...');
  }

  /**
   * The fake does not use the status list parameter, the fake data is never
   * updated.
   * @param {!ShutdownMethod} unused
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  endRma(unused) {
    return this.getNextStateForMethod('endRma', State.kRepairComplete);
  }

  /**
   * @return {!Promise<!{error: !RmadErrorCode}>}
   */
  criticalErrorExitToLogin() {
    return this.methods.resolveMethodWithDelay(
        'criticalErrorExitToLogin', this.resolveMethodDelayMs);
  }

  /**
   * @return {!Promise<!{error: !RmadErrorCode}>}
   */
  criticalErrorReboot() {
    return this.methods.resolveMethodWithDelay(
        'criticalErrorReboot', this.resolveMethodDelayMs);
  }

  shutDownAfterHardwareError() {
    console.log('(Fake) Shutting down...');
  }

  /**
   * @return {!Promise<!{provider: ?string}>}
   */
  get3pDiagnosticsProvider() {
    return this.methods.resolveMethodWithDelay(
        'get3pDiagnosticsProvider', this.resolveMethodDelayMs);
  }

  /** @param {?string} provider */
  setGet3pDiagnosticsProviderResult(provider) {
    this.methods.setResult('get3pDiagnosticsProvider', {provider});
  }

  /**
   * @return {!Promise<{appPath: FilePath}>}
   */
  getInstallable3pDiagnosticsAppPath() {
    return this.methods.resolveMethod('getInstallable3pDiagnosticsAppPath');
  }

  /** @param {FilePath} appPath */
  setInstallable3pDiagnosticsAppPath(appPath) {
    this.methods.setResult('getInstallable3pDiagnosticsAppPath', {appPath});
  }

  /**
   * @return {!Promise<{appInfo: Shimless3pDiagnosticsAppInfo}>}
   */
  installLastFound3pDiagnosticsApp() {
    return this.methods.resolveMethod('installLastFound3pDiagnosticsApp');
  }

  /** @param {Shimless3pDiagnosticsAppInfo} appInfo */
  setInstallLastFound3pDiagnosticsApp(appInfo) {
    this.methods.setResult('installLastFound3pDiagnosticsApp', {appInfo});
  }

  /**
   * @param {boolean} isApproved
   * @return {!Promise}
   */
  completeLast3pDiagnosticsInstallation(isApproved) {
    this.lastCompleteLast3pDiagnosticsInstallationApproval = isApproved;
    return Promise.resolve();
  }

  /** @return {?boolean} */
  getLastCompleteLast3pDiagnosticsInstallationApproval() {
    return this.lastCompleteLast3pDiagnosticsInstallationApproval;
  }

  /**
   * @return {!Promise<{result: !Show3pDiagnosticsAppResult}>}
   */
  show3pDiagnosticsApp() {
    this.wasShow3pDiagnosticsAppCalled = true;
    return this.methods.resolveMethod('show3pDiagnosticsApp');
  }

  /** @param {!Show3pDiagnosticsAppResult} result */
  setShow3pDiagnosticsAppResult(result) {
    this.methods.setResult('show3pDiagnosticsApp', {result});
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveError.
   * @param {!ErrorObserverRemote} remote
   */
  observeError(remote) {
    this.observables.observe('ErrorObserver_onError', (error) => {
      remote.onError(
          /** @type {!RmadErrorCode} */ (error));
    });
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveOsUpdate.
   * @param {!OsUpdateObserverRemote} remote
   */
  observeOsUpdateProgress(remote) {
    this.observables.observe(
        'OsUpdateObserver_onOsUpdateProgressUpdated',
        (operation, progress, errorCode) => {
          remote.onOsUpdateProgressUpdated(
              /** @type {!OsUpdateOperation} */ (operation),
              /** @type {number} */ (progress),
              /** @type {!UpdateErrorCode} */ (errorCode));
        });
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveRoFirmwareUpdateProgress.
   * @param {!UpdateRoFirmwareObserverRemote} remote
   */
  observeRoFirmwareUpdateProgress(remote) {
    this.observables.observe(
        'UpdateRoFirmwareObserver_onUpdateRoFirmwareStatusChanged',
        (status) => {
          remote.onUpdateRoFirmwareStatusChanged(
              /** @type {!UpdateRoFirmwareStatus} */ (status));
        });
    if (this.automaticallyTriggerUpdateRoFirmwareObservation) {
      this.triggerUpdateRoFirmwareObserver(UpdateRoFirmwareStatus.kWaitUsb, 0);
      this.triggerUpdateRoFirmwareObserver(
          UpdateRoFirmwareStatus.kUpdating, 1000);
      this.triggerUpdateRoFirmwareObserver(
          UpdateRoFirmwareStatus.kRebooting, 2000);
      this.triggerUpdateRoFirmwareObserver(
          UpdateRoFirmwareStatus.kComplete, 3000);
    }
  }

  /**
   * Trigger update ro firmware observations when an observer is added.
   */
  automaticallyTriggerUpdateRoFirmwareObservation() {
    this.automaticallyTriggerUpdateRoFirmwareObservation = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveCalibration.
   * @param {!CalibrationObserverRemote} remote
   */
  observeCalibrationProgress(remote) {
    this.observables.observe(
        'CalibrationObserver_onCalibrationUpdated', (componentStatus) => {
          remote.onCalibrationUpdated(
              /** @type {!CalibrationComponentStatus} */ (componentStatus));
        });
    this.observables.observe(
        'CalibrationObserver_onCalibrationStepComplete', (status) => {
          remote.onCalibrationStepComplete(
              /** @type {!CalibrationOverallStatus} */ (status));
        });
    if (this.automaticallyTriggerCalibrationObservation) {
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationWaiting,
            progress: 0.0,
          },
          1000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationInProgress,
            progress: 0.2,
          },
          2000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationInProgress,
            progress: 0.4,
          },
          3000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationInProgress,
            progress: 0.6,
          },
          4000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationInProgress,
            progress: 0.8,
          },
          5000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kLidAccelerometer,
            status: CalibrationStatus.kCalibrationWaiting,
            progress: 0.0,
          },
          6000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationComplete,
            progress: 0.5,
          },
          7000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationFailed,
            progress: 1.0,
          },
          8000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseGyroscope,
            status: CalibrationStatus.kCalibrationSkip,
            progress: 1.0,
          },
          9000);
      this.triggerCalibrationOverallObserver(
          CalibrationOverallStatus.kCalibrationOverallComplete, 10000);
    }
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveProvisioning.
   * @param {!ProvisioningObserverRemote} remote
   */
  observeProvisioningProgress(remote) {
    this.observables.observe(
        'ProvisioningObserver_onProvisioningUpdated',
        (status, progress, error) => {
          remote.onProvisioningUpdated(
              /** @type {!ProvisioningStatus} */ (status),
              /** @type {number} */ (progress),
              /** @type {!ProvisioningError} */ (error));
        });
    if (this.automaticallyTriggerProvisioningObservation) {
      // Fake progress over 4 seconds.
      this.triggerProvisioningObserver(
          ProvisioningStatus.kInProgress, 0.25, ProvisioningError.kUnknown,
          1000);
      this.triggerProvisioningObserver(
          ProvisioningStatus.kInProgress, 0.5, ProvisioningError.kUnknown,
          2000);
      this.triggerProvisioningObserver(
          ProvisioningStatus.kInProgress, 0.75, ProvisioningError.kUnknown,
          3000);
      this.triggerProvisioningObserver(
          ProvisioningStatus.kComplete, 1.0, ProvisioningError.kUnknown, 4000);
    }
  }

  /**
   * Trigger provisioning observations when an observer is added.
   */
  automaticallyTriggerProvisioningObservation() {
    this.automaticallyTriggerProvisioningObservation = true;
  }

  /**
   * Trigger calibration observations when an observer is added.
   */
  automaticallyTriggerCalibrationObservation() {
    this.automaticallyTriggerCalibrationObservation = true;
  }

  /**
   * Trigger OS update observations when an OS update is started.
   */
  automaticallyTriggerOsUpdateObservation() {
    this.automaticallyTriggerOsUpdateObservation = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveHardwareWriteProtectionState.
   * @param {!HardwareWriteProtectionStateObserverRemote} remote
   */
  observeHardwareWriteProtectionState(remote) {
    this.observables.observe(
        'HardwareWriteProtectionStateObserver_onHardwareWriteProtectionStateChanged',
        (enabled) => {
          remote.onHardwareWriteProtectionStateChanged(
              /** @type {boolean} */ (enabled));
        });
    if (this.states &&
        this.automaticallyTriggerDisableWriteProtectionObservation) {
      assert(this.stateIndex < this.states.length);
      this.triggerHardwareWriteProtectionObserver(
          this.states[this.stateIndex].state === State.kWaitForManualWPEnable,
          3000);
    }
  }

  /**
   * Trigger a disable write protection observation when an observer is added.
   */
  automaticallyTriggerDisableWriteProtectionObservation() {
    this.automaticallyTriggerDisableWriteProtectionObservation = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObservePowerCableState.
   * @param {!PowerCableStateObserverRemote} remote
   */
  observePowerCableState(remote) {
    this.observables.observe(
        'PowerCableStateObserver_onPowerCableStateChanged', (pluggedIn) => {
          remote.onPowerCableStateChanged(/** @type {boolean} */ (pluggedIn));
        });
    if (this.automaticallyTriggerPowerCableStateObservation) {
      this.triggerPowerCableObserver(false, 1000);
      this.triggerPowerCableObserver(true, 10000);
      this.triggerPowerCableObserver(false, 15000);
    }
  }

  /**
   * Trigger a disable power cable state observations when an observer is added.
   */
  automaticallyTriggerPowerCableStateObservation() {
    this.automaticallyTriggerPowerCableStateObservation = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveExternalDiskState.
   * @param {!ExternalDiskStateObserverRemote} remote
   */
  observeExternalDiskState(remote) {
    this.observables.observe(
        'ExternalDiskStateObserver_onExternalDiskStateChanged', (detected) => {
          remote.onExternalDiskStateChanged(/** @type {boolean} */ (detected));
        });

    this.triggerExternalDiskObserver(true, 10000);
    this.triggerExternalDiskObserver(false, 15000);
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveHardwareVerificationStatus.
   * @param {!HardwareVerificationStatusObserverRemote} remote
   */
  observeHardwareVerificationStatus(remote) {
    this.observables.observe(
        'HardwareVerificationStatusObserver_onHardwareVerificationResult',
        (is_compliant, error_message) => {
          remote.onHardwareVerificationResult(
              /** @type {boolean} */ (is_compliant),
              /** @type {string} */ (error_message));
        });
    if (this.automaticallyTriggerHardwareVerificationStatusObservation) {
      this.triggerHardwareVerificationStatusObserver(true, '', 3000);
    }
  }


  /**
   * Trigger a hardware verification observation when an observer is added.
   */
  automaticallyTriggerHardwareVerificationStatusObservation() {
    this.automaticallyTriggerHardwareVerificationStatusObservation = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveFinalizationStatus.
   * @param {!FinalizationObserverRemote} remote
   */
  observeFinalizationStatus(remote) {
    this.observables.observe(
        'FinalizationObserver_onFinalizationUpdated',
        (status, progress, error) => {
          remote.onFinalizationUpdated(
              /** @type {!FinalizationStatus} */ (status),
              /** @type {number} */ (progress),
              /** @type {!FinalizationError} */ (error));
        });
    if (this.automaticallyTriggerFinalizationObservation) {
      this.triggerFinalizationObserver(
          FinalizationStatus.kInProgress, 0.25, FinalizationError.kUnknown,
          1000);
      this.triggerFinalizationObserver(
          FinalizationStatus.kInProgress, 0.75, FinalizationError.kUnknown,
          2000);
      this.triggerFinalizationObserver(
          FinalizationStatus.kComplete, 1.0, FinalizationError.kUnknown, 3000);
    }
  }

  /**
   * Trigger a finalization progress observation when an observer is added.
   */
  automaticallyTriggerFinalizationObservation() {
    this.automaticallyTriggerFinalizationObservation = true;
  }

  /**
   * Causes the error observer to fire after a delay.
   * @param {!RmadErrorCode} error
   * @param {number} delayMs
   */
  triggerErrorObserver(error, delayMs) {
    return this.triggerObserverAfterMs('ErrorObserver_onError', error, delayMs);
  }

  /**
   * Causes the OS update observer to fire after a delay.
   * @param {!OsUpdateOperation} operation
   * @param {number} progress
   * @param {UpdateErrorCode} error
   * @param {number} delayMs
   */
  triggerOsUpdateObserver(operation, progress, error, delayMs) {
    return this.triggerObserverAfterMs(
        'OsUpdateObserver_onOsUpdateProgressUpdated',
        [operation, progress, error], delayMs);
  }

  /**
   * Causes the update RO firmware observer to fire after a delay.
   * @param {!UpdateRoFirmwareStatus} status
   * @param {number} delayMs
   */
  triggerUpdateRoFirmwareObserver(status, delayMs) {
    return this.triggerObserverAfterMs(
        'UpdateRoFirmwareObserver_onUpdateRoFirmwareStatusChanged', status,
        delayMs);
  }

  /**
   * Causes the calibration observer to fire after a delay.
   * @param {!CalibrationComponentStatus} componentStatus
   * @param {number} delayMs
   */
  triggerCalibrationObserver(componentStatus, delayMs) {
    return this.triggerObserverAfterMs(
        'CalibrationObserver_onCalibrationUpdated', componentStatus, delayMs);
  }

  /**
   * Causes the calibration overall observer to fire after a delay.
   * @param {!CalibrationOverallStatus} status
   * @param {number} delayMs
   */
  triggerCalibrationOverallObserver(status, delayMs) {
    return this.triggerObserverAfterMs(
        'CalibrationObserver_onCalibrationStepComplete', status, delayMs);
  }

  /**
   * Causes the provisioning observer to fire after a delay.
   * @param {!ProvisioningStatus} status
   * @param {number} progress
   * @param {!ProvisioningError} error
   * @param {number} delayMs
   */
  triggerProvisioningObserver(status, progress, error, delayMs) {
    return this.triggerObserverAfterMs(
        'ProvisioningObserver_onProvisioningUpdated', [status, progress, error],
        delayMs);
  }

  /**
   * Causes the hardware write protection observer to fire after a delay.
   * @param {boolean} enabled
   * @param {number} delayMs
   */
  triggerHardwareWriteProtectionObserver(enabled, delayMs) {
    return this.triggerObserverAfterMs(
        'HardwareWriteProtectionStateObserver_onHardwareWriteProtectionStateChanged',
        enabled, delayMs);
  }

  /**
   * Causes the power cable observer to fire after a delay.
   * @param {boolean} pluggedIn
   * @param {number} delayMs
   */
  triggerPowerCableObserver(pluggedIn, delayMs) {
    return this.triggerObserverAfterMs(
        'PowerCableStateObserver_onPowerCableStateChanged', pluggedIn, delayMs);
  }

  /**
   * Causes the external disk observer to fire after a delay.
   * @param {boolean} detected
   * @param {number} delayMs
   */
  triggerExternalDiskObserver(detected, delayMs) {
    return this.triggerObserverAfterMs(
        'ExternalDiskStateObserver_onExternalDiskStateChanged', detected,
        delayMs);
  }

  /**
   * Causes the hardware verification observer to fire after a delay.
   * @param {boolean} is_compliant
   * @param {string} error_message
   * @param {number} delayMs
   */
  triggerHardwareVerificationStatusObserver(
      is_compliant, error_message, delayMs) {
    return this.triggerObserverAfterMs(
        'HardwareVerificationStatusObserver_onHardwareVerificationResult',
        [is_compliant, error_message], delayMs);
  }

  /**
   * Causes the finalization observer to fire after a delay.
   * @param {!FinalizationStatus} status
   * @param {number} progress
   * @param {!FinalizationError} error
   * @param {number} delayMs
   */
  triggerFinalizationObserver(status, progress, error, delayMs) {
    return this.triggerObserverAfterMs(
        'FinalizationObserver_onFinalizationUpdated', [status, progress, error],
        delayMs);
  }

  /**
   * Causes an observer to fire after a delay.
   * @param {string} method
   * @param {!T} result
   * @param {number} delayMs
   * @template T
   */
  triggerObserverAfterMs(method, result, delayMs) {
    const setDataTriggerAndResolve = function(service, resolve) {
      service.observables.setObservableData(method, [result]);
      service.observables.trigger(method);
      resolve();
    };

    return new Promise((resolve) => {
      if (delayMs === 0) {
        setDataTriggerAndResolve(this, resolve);
      } else {
        setTimeout(() => {
          setDataTriggerAndResolve(this, resolve);
        }, delayMs);
      }
    });
  }

  /**
   * Disables all observers and resets provider to its initial state.
   */
  reset() {
    this.registerMethods();
    this.registerObservables();

    this.states = [];
    this.stateIndex = 0;

    // This state data is more complicated so the behavior of the get/set
    // methods is a little different than other fakes in that they don't return
    // undefined by default.
    this.components = [];
    this.setGetLogResult('');
    this.setSaveLogResult({'path': ''});

    this.lastCompleteLast3pDiagnosticsInstallationApproval = null;
    this.setGet3pDiagnosticsProviderResult(null);
  }

  /**
   * Setup method resolvers.
   * @private
   */
  registerMethods() {
    this.methods = new FakeMethodResolver();

    this.methods.register('getCurrentState');
    this.methods.register('transitionPreviousState');

    this.methods.register('abortRma');

    this.methods.register('canExit');
    this.methods.register('canGoBack');

    this.methods.register('beginFinalization');

    this.methods.register('trackConfiguredNetworks');
    this.methods.register('networkSelectionComplete');

    this.methods.register('getCurrentOsVersion');
    this.methods.register('checkForOsUpdates');
    this.methods.register('updateOs');
    this.methods.register('updateOsSkipped');

    this.methods.register('setSameOwner');
    this.methods.register('setDifferentOwner');
    this.methods.register('setWipeDevice');

    this.methods.register('chooseManuallyDisableWriteProtect');
    this.methods.register('chooseRsuDisableWriteProtect');
    this.methods.register('getRsuDisableWriteProtectChallenge');
    this.methods.register('getRsuDisableWriteProtectHwid');
    this.methods.register('getRsuDisableWriteProtectChallengeQrCode');
    this.methods.register('setRsuDisableWriteProtectCode');

    this.methods.register('writeProtectManuallyDisabled');

    this.methods.register('getWriteProtectDisableCompleteAction');
    this.methods.register('confirmManualWpDisableComplete');

    this.methods.register('shutdownForRestock');
    this.methods.register('continueFinalizationAfterRestock');

    this.methods.register('getComponentList');
    this.methods.register('setComponentList');
    this.methods.register('reworkMainboard');

    this.methods.register('roFirmwareUpdateComplete');

    this.methods.register('getRegionList');
    this.methods.register('getSkuList');
    this.methods.register('getCustomLabelList');
    this.methods.register('getSkuDescriptionList');
    this.methods.register('getOriginalSerialNumber');
    this.methods.register('getOriginalRegion');
    this.methods.register('getOriginalSku');
    this.methods.register('getOriginalCustomLabel');
    this.methods.register('getOriginalDramPartNumber');
    this.methods.register('getOriginalFeatureLevel');
    this.methods.register('setDeviceInformation');

    this.methods.register('getCalibrationComponentList');
    this.methods.register('getCalibrationSetupInstructions');
    this.methods.register('startCalibration');
    this.methods.register('runCalibrationStep');
    this.methods.register('continueCalibration');
    this.methods.register('calibrationComplete');

    this.methods.register('retryProvisioning');
    this.methods.register('provisioningComplete');

    this.methods.register('retryFinalization');
    this.methods.register('finalizationComplete');

    this.methods.register('writeProtectManuallyEnabled');

    this.methods.register('getLog');
    this.methods.register('saveLog');
    this.methods.register('getPowerwashRequired');
    this.methods.register('endRma');

    // Critical error handling
    this.methods.register('criticalErrorExitToLogin');
    this.methods.register('criticalErrorReboot');

    this.methods.register('shutDownAfterHardwareError');

    this.methods.register('get3pDiagnosticsProvider');
    this.methods.register('getInstallable3pDiagnosticsAppPath');
    this.methods.register('installLastFound3pDiagnosticsApp');
    this.methods.register('show3pDiagnosticsApp');
  }

  /**
   * Setup observables.
   * @private
   */
  registerObservables() {
    if (this.observables) {
      this.observables.stopAllTriggerIntervals();
    }
    this.observables = new FakeObservables();
    this.observables.register('ErrorObserver_onError');
    this.observables.register('OsUpdateObserver_onOsUpdateProgressUpdated');
    this.observables.register(
        'UpdateRoFirmwareObserver_onUpdateRoFirmwareStatusChanged');
    this.observables.register('CalibrationObserver_onCalibrationUpdated');
    this.observables.register('CalibrationObserver_onCalibrationStepComplete');
    this.observables.register('ProvisioningObserver_onProvisioningUpdated');
    this.observables.register(
        'HardwareWriteProtectionStateObserver_onHardwareWriteProtectionStateChanged');
    this.observables.register(
        'PowerCableStateObserver_onPowerCableStateChanged');
    this.observables.register(
        'ExternalDiskStateObserver_onExternalDiskStateChanged');
    this.observables.register(
        'HardwareVerificationStatusObserver_onHardwareVerificationResult');
    this.observables.register('FinalizationObserver_onFinalizationUpdated');
  }

  /**
   * @param {string} method
   * @param {!State} expectedState
   * @return {!Promise<!{stateResult: !StateResult}>}
   * @private
   */
  getNextStateForMethod(method, expectedState) {
    if (this.states.length === 0) {
      this.setFakeStateForMethod(
          method, State.kUnknown, false, false, RmadErrorCode.kRmaNotRequired);
    } else if (this.stateIndex >= this.states.length - 1) {
      // It should not be possible for stateIndex to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex < this.states.length);
      const state = this.states[this.stateIndex];
      this.setFakeStateForMethod(
          method, state.state, state.canExit, state.canGoBack,
          RmadErrorCode.kTransitionFailed);
    } else if (this.states[this.stateIndex].state !== expectedState) {
      // Error: Called in wrong state.
      const state = this.states[this.stateIndex];
      this.setFakeStateForMethod(
          method, state.state, state.canExit, state.canGoBack,
          RmadErrorCode.kRequestInvalid);
    } else {
      // Success.
      this.stateIndex++;
      if (method === 'chooseManuallyDisableWriteProtect') {
        // A special case so that choosing manual WP disable sends you to the
        // appropriate page in the fake app.
        this.stateIndex++;
      }
      const state = this.states[this.stateIndex];
      this.setFakeStateForMethod(
          method, state.state, state.canExit, state.canGoBack, state.error);
    }
    return this.methods.resolveMethodWithDelay(
        method, this.resolveMethodDelayMs);
  }

  /**
   * Sets the value that will be returned when calling getCurrent().
   * @param {!State} state
   * @param {boolean} canExit,
   * @param {boolean} canGoBack,
   * @param {!RmadErrorCode} error
   * @private
   */
  setFakeCurrentState(state, canExit, canGoBack, error) {
    this.setFakeStateForMethod(
        'getCurrentState', state, canExit, canGoBack, error);
  }

  /**
   * Sets the value that will be returned when calling
   * transitionPreviousState().
   * @param {!State} state
   * @param {boolean} canExit,
   * @param {boolean} canGoBack,
   * @param {!RmadErrorCode} error
   * @private
   */
  setFakePrevState(state, canExit, canGoBack, error) {
    this.setFakeStateForMethod(
        'transitionPreviousState', state, canExit, canGoBack, error);
  }

  /**
   * Sets the value that will be returned when calling state specific functions
   * that update state. e.g. setSameOwner()
   * @param {string} method
   * @param {!State} state
   * @param {boolean} canExit,
   * @param {boolean} canGoBack,
   * @param {!RmadErrorCode} error
   * @private
   */
  setFakeStateForMethod(method, state, canExit, canGoBack, error) {
    this.methods.setResult(method, /** @type {{stateResult: !StateResult}} */ ({
                             stateResult: {
                               state: state,
                               canExit: canExit,
                               canGoBack: canGoBack,
                               error: error,
                             },
                           }));
  }
}
