// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './file_path.mojom-lite.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';

import {CalibrationComponentStatus, CalibrationObserverRemote, CalibrationOverallStatus, CalibrationSetupInstruction, CalibrationStatus, Component, ComponentType, ErrorObserverRemote, ExternalDiskStateObserverRemote, FeatureLevel, FinalizationError, FinalizationObserverRemote, FinalizationStatus, HardwareVerificationStatusObserverRemote, HardwareWriteProtectionStateObserverRemote, OsUpdateObserverRemote, OsUpdateOperation, PowerCableStateObserverRemote, ProvisioningError, ProvisioningObserverRemote, ProvisioningStatus, QrCode, RmadErrorCode, Shimless3pDiagnosticsAppInfo, ShimlessRmaServiceInterface, Show3pDiagnosticsAppResult, ShutdownMethod, State, StateResult, UpdateErrorCode, UpdateRoFirmwareObserverRemote, UpdateRoFirmwareStatus, WriteProtectDisableCompleteAction} from './shimless_rma_types.js';

/** @implements {ShimlessRmaServiceInterface} */
export class FakeShimlessRmaService {
  constructor() {
    this.methods_ = new FakeMethodResolver();
    this.observables_ = new FakeObservables();

    /**
     * The list of states for this RMA flow.
     * @private {!Array<!StateResult>}
     */
    this.states_ = [];

    /**
     * The index into states_ for the current fake state.
     * @private {number}
     */
    this.stateIndex_ = 0;

    /**
     * The list of components.
     * @private {!Array<!Component>}
     */
    this.components_ = [];

    /**
     * Control automatically triggering a HWWP disable observation.
     * @private {boolean}
     */
    this.automaticallyTriggerDisableWriteProtectionObservation_ = false;

    /**
     * Control automatically triggering update RO firmware observations.
     * @private {boolean}
     */
    this.automaticallyTriggerUpdateRoFirmwareObservation_ = false;

    /**
     * Control automatically triggering provisioning observations.
     * @private {boolean}
     */
    this.automaticallyTriggerProvisioningObservation_ = false;

    /**
     * Control automatically triggering calibration observations.
     * @private {boolean}
     */
    this.automaticallyTriggerCalibrationObservation_ = false;

    /**
     * Control automatically triggering OS update observations.
     * @private {boolean}
     */
    this.automaticallyTriggerOsUpdateObservation_ = false;

    /**
     * Control automatically triggering a hardware verification observation.
     * @private {boolean}
     */
    this.automaticallyTriggerHardwareVerificationStatusObservation_ = false;

    /**
     * Control automatically triggering a finalization observation.
     * @private {boolean}
     */
    this.automaticallyTriggerFinalizationObservation_ = false;

    /**
     * Control automatically triggering power cable state observations.
     * @private {boolean}
     */
    this.automaticallyTriggerPowerCableStateObservation_ = false;

    /**
     * Both abortRma and forward state transitions can have significant delays
     * that are useful to fake for manual testing.
     * Defaults to no delay for unit tests.
     * @private {number}
     */
    this.resolveMethodDelayMs_ = 0;

    /**
     * The result of calling trackConfiguredNetworks.
     * @private {boolean}
     */
    this.trackConfiguredNetworksCalled_ = false;

    /**
     * The approval of last call to completeLast3pDiagnosticsInstallation.
     * @private {?boolean}
     */
    this.lastCompleteLast3pDiagnosticsInstallationApproval_ = null;

    /**
     * Has show3pDiagnosticsApp been called.
     * @private {boolean}
     */
    this.wasShow3pDiagnosticsAppCalled_ = false;

    this.reset();
  }

  /** @param {number} delayMs */
  setAsyncOperationDelayMs(delayMs) {
    this.resolveMethodDelayMs_ = delayMs;
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
    this.states_ = states;
    this.stateIndex_ = 0;
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  getCurrentState() {
    // As next state functions and transitionPreviousState can modify the result
    // of this function the result must be set at the time of the call.
    if (this.states_.length === 0) {
      this.setFakeCurrentState_(
          State.kUnknown, false, false, RmadErrorCode.kRmaNotRequired);
    } else {
      // It should not be possible for stateIndex_ to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex_ < this.states_.length);
      const state = this.states_[this.stateIndex_];
      this.setFakeCurrentState_(
          state.state, state.canExit, state.canGoBack, state.error);
    }
    return this.methods_.resolveMethodWithDelay(
        'getCurrentState', this.resolveMethodDelayMs_);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  transitionPreviousState() {
    // As next state methods and transitionPreviousState can modify the result
    // of this function the result must be set at the time of the call.
    if (this.states_.length === 0) {
      this.setFakePrevState_(
          State.kUnknown, false, false, RmadErrorCode.kRmaNotRequired);
    } else if (this.stateIndex_ === 0) {
      // It should not be possible for stateIndex_ to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex_ < this.states_.length);
      const state = this.states_[this.stateIndex_];
      this.setFakePrevState_(
          state.state, state.canExit, state.canGoBack,
          RmadErrorCode.kTransitionFailed);
    } else {
      this.stateIndex_--;
      const state = this.states_[this.stateIndex_];
      this.setFakePrevState_(
          state.state, state.canExit, state.canGoBack, state.error);
    }
    return this.methods_.resolveMethodWithDelay(
        'transitionPreviousState', this.resolveMethodDelayMs_);
  }

  /**
   * @return {!Promise<!{error: !RmadErrorCode}>}
   */
  abortRma() {
    return this.methods_.resolveMethodWithDelay(
        'abortRma', this.resolveMethodDelayMs_);
  }

  /**
   * Sets the value that will be returned when calling abortRma().
   * @param {!RmadErrorCode} error
   */
  setAbortRmaResult(error) {
    this.methods_.setResult('abortRma', {error: error});
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  beginFinalization() {
    return this.getNextStateForMethod_(
        'beginFinalization', State.kWelcomeScreen);
  }

  trackConfiguredNetworks() {
    this.trackConfiguredNetworksCalled_ = true;
  }

  getTrackConfiguredNetworks() {
    return this.trackConfiguredNetworksCalled_;
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  networkSelectionComplete() {
    return this.getNextStateForMethod_(
        'networkSelectionComplete', State.kConfigureNetwork);
  }

  /**
   * @return {!Promise<!{version: string}>}
   */
  getCurrentOsVersion() {
    return this.methods_.resolveMethod('getCurrentOsVersion');
  }

  /**
   * @param {null|string} version
   */
  setGetCurrentOsVersionResult(version) {
    this.methods_.setResult('getCurrentOsVersion', {version: version});
  }

  /**
   * @return {!Promise<!{updateAvailable: boolean, version: string}>}
   */
  checkForOsUpdates() {
    return this.methods_.resolveMethod('checkForOsUpdates');
  }

  /** @param {string} version */
  setCheckForOsUpdatesResult(version) {
    this.methods_.setResult(
        'checkForOsUpdates', {updateAvailable: true, version});
  }

  /**
   * @return {!Promise<!{updateStarted: boolean}>}
   */
  updateOs() {
    if (this.automaticallyTriggerOsUpdateObservation_) {
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
    return this.methods_.resolveMethod('updateOs');
  }

  /**
   * @param {boolean} started
   */
  setUpdateOsResult(started) {
    this.methods_.setResult('updateOs', {updateStarted: started});
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  updateOsSkipped() {
    return this.getNextStateForMethod_('updateOsSkipped', State.kUpdateOs);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  setSameOwner() {
    return this.getNextStateForMethod_(
        'setSameOwner', State.kChooseDestination);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  setDifferentOwner() {
    return this.getNextStateForMethod_(
        'setDifferentOwner', State.kChooseDestination);
  }

  /**
   * @param {boolean} shouldWipeDevice
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  setWipeDevice(shouldWipeDevice) {
    return this.getNextStateForMethod_(
        'setWipeDevice', State.kChooseWipeDevice);
  }

  /**
   * @return {!Promise<!{available: boolean}>}
   */
  manualDisableWriteProtectAvailable() {
    return this.methods_.resolveMethod('manualDisableWriteProtectAvailable');
  }

  /**
   * @param {boolean} available
   */
  setManualDisableWriteProtectAvailableResult(available) {
    this.methods_.setResult(
        'manualDisableWriteProtectAvailable', {available: available});
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  chooseManuallyDisableWriteProtect() {
    return this.getNextStateForMethod_(
        'chooseManuallyDisableWriteProtect',
        State.kChooseWriteProtectDisableMethod);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  chooseRsuDisableWriteProtect() {
    return this.getNextStateForMethod_(
        'chooseRsuDisableWriteProtect', State.kChooseWriteProtectDisableMethod);
  }

  /**
   * @return {!Promise<!{challenge: string}>}
   */
  getRsuDisableWriteProtectChallenge() {
    return this.methods_.resolveMethod('getRsuDisableWriteProtectChallenge');
  }

  /**
   * @param {string} challenge
   */
  setGetRsuDisableWriteProtectChallengeResult(challenge) {
    this.methods_.setResult(
        'getRsuDisableWriteProtectChallenge', {challenge: challenge});
  }

  /**
   * @return {!Promise<!{hwid: string}>}
   */
  getRsuDisableWriteProtectHwid() {
    return this.methods_.resolveMethod('getRsuDisableWriteProtectHwid');
  }

  /**
   * @param {string} hwid
   */
  setGetRsuDisableWriteProtectHwidResult(hwid) {
    this.methods_.setResult('getRsuDisableWriteProtectHwid', {hwid: hwid});
  }

  /**
   * @return {!Promise<!{qrCodeData: !Array<number>}>}
   */
  getRsuDisableWriteProtectChallengeQrCode() {
    return this.methods_.resolveMethod(
        'getRsuDisableWriteProtectChallengeQrCode');
  }

  /**
   * @param {!Array<number>} qrCodeData
   */
  setGetRsuDisableWriteProtectChallengeQrCodeResponse(qrCodeData) {
    this.methods_.setResult(
        'getRsuDisableWriteProtectChallengeQrCode', {qrCodeData: qrCodeData});
  }

  /**
   * @param {string} code
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  setRsuDisableWriteProtectCode(code) {
    return this.getNextStateForMethod_(
        'setRsuDisableWriteProtectCode', State.kEnterRSUWPDisableCode);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  writeProtectManuallyDisabled() {
    return this.getNextStateForMethod_(
        'writeProtectManuallyDisabled', State.kWaitForManualWPDisable);
  }

  /** @return {!Promise<!{action: !WriteProtectDisableCompleteAction}>} */
  getWriteProtectDisableCompleteAction() {
    return this.methods_.resolveMethod('getWriteProtectDisableCompleteAction');
  }

  /** @param {!WriteProtectDisableCompleteAction} action */
  setGetWriteProtectDisableCompleteAction(action) {
    this.methods_.setResult(
        'getWriteProtectDisableCompleteAction', {action: action});
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  confirmManualWpDisableComplete() {
    return this.getNextStateForMethod_(
        'confirmManualWpDisableComplete', State.kWPDisableComplete);
  }

  /**
   * @return {!Promise<!{components: !Array<!Component>}>}
   */
  getComponentList() {
    this.methods_.setResult('getComponentList', {components: this.components_});
    return this.methods_.resolveMethod('getComponentList');
  }

  /**
   * @param {!Array<!Component>} components
   */
  setGetComponentListResult(components) {
    this.components_ = components;
  }

  /**
   * @param {!Array<!Component>} components
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  setComponentList(components) {
    return this.getNextStateForMethod_(
        'setComponentList', State.kSelectComponents);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  reworkMainboard() {
    return this.getNextStateForMethod_(
        'reworkMainboard', State.kSelectComponents);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  roFirmwareUpdateComplete() {
    return this.getNextStateForMethod_(
        'roFirmwareUpdateComplete', State.kUpdateRoFirmware);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   *
   */
  shutdownForRestock() {
    return this.getNextStateForMethod_('shutdownForRestock', State.kRestock);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  continueFinalizationAfterRestock() {
    return this.getNextStateForMethod_(
        'continueFinalizationAfterRestock', State.kRestock);
  }

  /**
   * @return {!Promise<!{regions: !Array<string>}>}
   */
  getRegionList() {
    return this.methods_.resolveMethod('getRegionList');
  }

  /**
   * @param {!Array<string>} regions
   */
  setGetRegionListResult(regions) {
    this.methods_.setResult('getRegionList', {regions: regions});
  }

  /**
   * @return {!Promise<!{skus: !Array<bigint>}>}
   */
  getSkuList() {
    return this.methods_.resolveMethod('getSkuList');
  }

  /**
   * @param {!Array<bigint>} skus
   */
  setGetSkuListResult(skus) {
    this.methods_.setResult('getSkuList', {skus: skus});
  }

  /**
   * @return {!Promise<!{customLabels: !Array<string>}>}
   */
  getCustomLabelList() {
    return this.methods_.resolveMethod('getCustomLabelList');
  }

  /**
   * @param {!Array<string>} customLabels
   */
  setGetCustomLabelListResult(customLabels) {
    this.methods_.setResult('getCustomLabelList', {customLabels: customLabels});
  }

  /**
   * @return {!Promise<!{serialNumber: string}>}
   */
  getOriginalSerialNumber() {
    return this.methods_.resolveMethod('getOriginalSerialNumber');
  }

  /**
   * @param {string} serialNumber
   */
  setGetOriginalSerialNumberResult(serialNumber) {
    this.methods_.setResult(
        'getOriginalSerialNumber', {serialNumber: serialNumber});
  }

  /**
   * @return {!Promise<!{regionIndex: number}>}
   */
  getOriginalRegion() {
    return this.methods_.resolveMethod('getOriginalRegion');
  }

  /**
   * @param {number} regionIndex
   */
  setGetOriginalRegionResult(regionIndex) {
    this.methods_.setResult('getOriginalRegion', {regionIndex: regionIndex});
  }

  /**
   * @return {!Promise<!{skuIndex: number}>}
   */
  getOriginalSku() {
    return this.methods_.resolveMethod('getOriginalSku');
  }

  /**
   * @param {number} skuIndex
   */
  setGetOriginalSkuResult(skuIndex) {
    this.methods_.setResult('getOriginalSku', {skuIndex: skuIndex});
  }

  /**
   * @return {!Promise<!{customLabelIndex: number}>}
   */
  getOriginalCustomLabel() {
    return this.methods_.resolveMethod('getOriginalCustomLabel');
  }

  /**
   * @param {number} customLabelIndex
   */
  setGetOriginalCustomLabelResult(customLabelIndex) {
    this.methods_.setResult(
        'getOriginalCustomLabel', {customLabelIndex: customLabelIndex});
  }

  /**
   * @return {!Promise<!{dramPartNumber: string}>}
   */
  getOriginalDramPartNumber() {
    return this.methods_.resolveMethod('getOriginalDramPartNumber');
  }

  /**
   * @param {string} dramPartNumber
   */
  setGetOriginalDramPartNumberResult(dramPartNumber) {
    this.methods_.setResult(
        'getOriginalDramPartNumber', {dramPartNumber: dramPartNumber});
  }

  /**
   * @return {!Promise<!{originalFeatureLevel: FeatureLevel}>}
   */
  getOriginalFeatureLevel() {
    return this.methods_.resolveMethod('getOriginalFeatureLevel');
  }

  /**
   * @param {FeatureLevel} featureLevel
   */
  setGetOriginalFeatureLevelResult(featureLevel) {
    this.methods_.setResult(
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
    return this.getNextStateForMethod_(
        'setDeviceInformation', State.kUpdateDeviceInformation);
  }

  /**
   * @return {!Promise<!{components: !Array<!CalibrationComponentStatus>}>}
   */
  getCalibrationComponentList() {
    return this.methods_.resolveMethod('getCalibrationComponentList');
  }

  /**
   * @param {!Array<!CalibrationComponentStatus>} components
   */
  setGetCalibrationComponentListResult(components) {
    this.methods_.setResult(
        'getCalibrationComponentList', {components: components});
  }

  /**
   * @return {!Promise<!{instructions: CalibrationSetupInstruction}>}
   */
  getCalibrationSetupInstructions() {
    return this.methods_.resolveMethod('getCalibrationSetupInstructions');
  }

  /**
   * @param {CalibrationSetupInstruction} instructions
   */
  setGetCalibrationSetupInstructionsResult(instructions) {
    this.methods_.setResult(
        'getCalibrationSetupInstructions', {instructions: instructions});
  }

  /**
   * The fake does not use the status list parameter, the fake data is never
   * updated.
   * @param {!Array<!CalibrationComponentStatus>} unused
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  startCalibration(unused) {
    return this.getNextStateForMethod_(
        'startCalibration', State.kCheckCalibration);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  runCalibrationStep() {
    return this.getNextStateForMethod_(
        'runCalibrationStep', State.kSetupCalibration);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  continueCalibration() {
    return this.getNextStateForMethod_(
        'continueCalibration', State.kRunCalibration);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  calibrationComplete() {
    return this.getNextStateForMethod_(
        'calibrationComplete', State.kRunCalibration);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  retryProvisioning() {
    return this.getNextStateForMethod_(
        'retryProvisioning', State.kProvisionDevice);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  provisioningComplete() {
    return this.getNextStateForMethod_(
        'provisioningComplete', State.kProvisionDevice);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  finalizationComplete() {
    return this.getNextStateForMethod_('finalizationComplete', State.kFinalize);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  retryFinalization() {
    return this.getNextStateForMethod_('retryFinalization', State.kFinalize);
  }

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   */
  writeProtectManuallyEnabled() {
    return this.getNextStateForMethod_(
        'writeProtectManuallyEnabled', State.kWaitForManualWPEnable);
  }

  /** @return {!Promise<{log: string, error: !RmadErrorCode}>} */
  getLog() {
    return this.methods_.resolveMethod('getLog');
  }

  /** @param {string} log */
  setGetLogResult(log) {
    this.methods_.setResult('getLog', {log: log, error: RmadErrorCode.kOk});
  }

  /**
   * @return {!Promise<{savePath: !mojoBase.mojom.FilePath, error:
   *     !RmadErrorCode}>}
   */
  saveLog() {
    return this.methods_.resolveMethod('saveLog');
  }

  /** @param {!mojoBase.mojom.FilePath} savePath */
  setSaveLogResult(savePath) {
    this.methods_.setResult(
        'saveLog', {savePath: savePath, error: RmadErrorCode.kOk});
  }

  /** @return {!Promise<{powerwashRequired: boolean, error: !RmadErrorCode}>} */
  getPowerwashRequired() {
    return this.methods_.resolveMethod('getPowerwashRequired');
  }

  /** @param {boolean} powerwashRequired */
  setGetPowerwashRequiredResult(powerwashRequired) {
    this.methods_.setResult(
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
    return this.getNextStateForMethod_('endRma', State.kRepairComplete);
  }

  /**
   * @return {!Promise<!{error: !RmadErrorCode}>}
   */
  criticalErrorExitToLogin() {
    return this.methods_.resolveMethodWithDelay(
        'criticalErrorExitToLogin', this.resolveMethodDelayMs_);
  }

  /**
   * @return {!Promise<!{error: !RmadErrorCode}>}
   */
  criticalErrorReboot() {
    return this.methods_.resolveMethodWithDelay(
        'criticalErrorReboot', this.resolveMethodDelayMs_);
  }

  shutDownAfterHardwareError() {
    console.log('(Fake) Shutting down...');
  }

  /**
   * @return {!Promise<!{provider: ?string}>}
   */
  get3pDiagnosticsProvider() {
    return this.methods_.resolveMethodWithDelay(
        'get3pDiagnosticsProvider', this.resolveMethodDelayMs_);
  }

  /** @param {?string} provider */
  setGet3pDiagnosticsProviderResult(provider) {
    this.methods_.setResult('get3pDiagnosticsProvider', {provider});
  }

  /**
   * @return {!Promise<{appPath: mojoBase.mojom.FilePath}>}
   */
  getInstallable3pDiagnosticsAppPath() {
    return this.methods_.resolveMethod('getInstallable3pDiagnosticsAppPath');
  }

  /** @param {mojoBase.mojom.FilePath} appPath */
  setInstallable3pDiagnosticsAppPath(appPath) {
    this.methods_.setResult('getInstallable3pDiagnosticsAppPath', {appPath});
  }

  /**
   * @return {!Promise<{appInfo: Shimless3pDiagnosticsAppInfo}>}
   */
  installLastFound3pDiagnosticsApp() {
    return this.methods_.resolveMethod('installLastFound3pDiagnosticsApp');
  }

  /** @param {Shimless3pDiagnosticsAppInfo} appInfo */
  setInstallLastFound3pDiagnosticsApp(appInfo) {
    this.methods_.setResult('installLastFound3pDiagnosticsApp', {appInfo});
  }

  /**
   * @param {boolean} isApproved
   * @return {!Promise}
   */
  completeLast3pDiagnosticsInstallation(isApproved) {
    this.lastCompleteLast3pDiagnosticsInstallationApproval_ = isApproved;
    return Promise.resolve();
  }

  /** @return {?boolean} */
  getLastCompleteLast3pDiagnosticsInstallationApproval() {
    return this.lastCompleteLast3pDiagnosticsInstallationApproval_;
  }

  /**
   * @return {!Promise<{result: !Show3pDiagnosticsAppResult}>}
   */
  show3pDiagnosticsApp() {
    this.wasShow3pDiagnosticsAppCalled_ = true;
    return this.methods_.resolveMethod('show3pDiagnosticsApp');
  }

  /** @param {!Show3pDiagnosticsAppResult} result */
  setShow3pDiagnosticsAppResult(result) {
    this.methods_.setResult('show3pDiagnosticsApp', {result});
  }

  /** @return {boolean} */
  wasShow3pDiagnosticsAppCalled() {
    return this.wasShow3pDiagnosticsAppCalled_;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveError.
   * @param {!ErrorObserverRemote} remote
   */
  observeError(remote) {
    this.observables_.observe('ErrorObserver_onError', (error) => {
      remote.onError(
          /** @type {!RmadErrorCode} */ (error));
    });
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveOsUpdate.
   * @param {!OsUpdateObserverRemote} remote
   */
  observeOsUpdateProgress(remote) {
    this.observables_.observe(
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
    this.observables_.observe(
        'UpdateRoFirmwareObserver_onUpdateRoFirmwareStatusChanged',
        (status) => {
          remote.onUpdateRoFirmwareStatusChanged(
              /** @type {!UpdateRoFirmwareStatus} */ (status));
        });
    if (this.automaticallyTriggerUpdateRoFirmwareObservation_) {
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
    this.automaticallyTriggerUpdateRoFirmwareObservation_ = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveCalibration.
   * @param {!CalibrationObserverRemote} remote
   */
  observeCalibrationProgress(remote) {
    this.observables_.observe(
        'CalibrationObserver_onCalibrationUpdated', (componentStatus) => {
          remote.onCalibrationUpdated(
              /** @type {!CalibrationComponentStatus} */ (componentStatus));
        });
    this.observables_.observe(
        'CalibrationObserver_onCalibrationStepComplete', (status) => {
          remote.onCalibrationStepComplete(
              /** @type {!CalibrationOverallStatus} */ (status));
        });
    if (this.automaticallyTriggerCalibrationObservation_) {
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
    this.observables_.observe(
        'ProvisioningObserver_onProvisioningUpdated',
        (status, progress, error) => {
          remote.onProvisioningUpdated(
              /** @type {!ProvisioningStatus} */ (status),
              /** @type {number} */ (progress),
              /** @type {!ProvisioningError} */ (error));
        });
    if (this.automaticallyTriggerProvisioningObservation_) {
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
    this.automaticallyTriggerProvisioningObservation_ = true;
  }

  /**
   * Trigger calibration observations when an observer is added.
   */
  automaticallyTriggerCalibrationObservation() {
    this.automaticallyTriggerCalibrationObservation_ = true;
  }

  /**
   * Trigger OS update observations when an OS update is started.
   */
  automaticallyTriggerOsUpdateObservation() {
    this.automaticallyTriggerOsUpdateObservation_ = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveHardwareWriteProtectionState.
   * @param {!HardwareWriteProtectionStateObserverRemote} remote
   */
  observeHardwareWriteProtectionState(remote) {
    this.observables_.observe(
        'HardwareWriteProtectionStateObserver_onHardwareWriteProtectionStateChanged',
        (enabled) => {
          remote.onHardwareWriteProtectionStateChanged(
              /** @type {boolean} */ (enabled));
        });
    if (this.states_ &&
        this.automaticallyTriggerDisableWriteProtectionObservation_) {
      assert(this.stateIndex_ < this.states_.length);
      this.triggerHardwareWriteProtectionObserver(
          this.states_[this.stateIndex_].state === State.kWaitForManualWPEnable,
          3000);
    }
  }

  /**
   * Trigger a disable write protection observation when an observer is added.
   */
  automaticallyTriggerDisableWriteProtectionObservation() {
    this.automaticallyTriggerDisableWriteProtectionObservation_ = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObservePowerCableState.
   * @param {!PowerCableStateObserverRemote} remote
   */
  observePowerCableState(remote) {
    this.observables_.observe(
        'PowerCableStateObserver_onPowerCableStateChanged', (pluggedIn) => {
          remote.onPowerCableStateChanged(/** @type {boolean} */ (pluggedIn));
        });
    if (this.automaticallyTriggerPowerCableStateObservation_) {
      this.triggerPowerCableObserver(false, 1000);
      this.triggerPowerCableObserver(true, 10000);
      this.triggerPowerCableObserver(false, 15000);
    }
  }

  /**
   * Trigger a disable power cable state observations when an observer is added.
   */
  automaticallyTriggerPowerCableStateObservation() {
    this.automaticallyTriggerPowerCableStateObservation_ = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveExternalDiskState.
   * @param {!ExternalDiskStateObserverRemote} remote
   */
  observeExternalDiskState(remote) {
    this.observables_.observe(
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
    this.observables_.observe(
        'HardwareVerificationStatusObserver_onHardwareVerificationResult',
        (is_compliant, error_message) => {
          remote.onHardwareVerificationResult(
              /** @type {boolean} */ (is_compliant),
              /** @type {string} */ (error_message));
        });
    if (this.automaticallyTriggerHardwareVerificationStatusObservation_) {
      this.triggerHardwareVerificationStatusObserver(true, '', 3000);
    }
  }


  /**
   * Trigger a hardware verification observation when an observer is added.
   */
  automaticallyTriggerHardwareVerificationStatusObservation() {
    this.automaticallyTriggerHardwareVerificationStatusObservation_ = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveFinalizationStatus.
   * @param {!FinalizationObserverRemote} remote
   */
  observeFinalizationStatus(remote) {
    this.observables_.observe(
        'FinalizationObserver_onFinalizationUpdated',
        (status, progress, error) => {
          remote.onFinalizationUpdated(
              /** @type {!FinalizationStatus} */ (status),
              /** @type {number} */ (progress),
              /** @type {!FinalizationError} */ (error));
        });
    if (this.automaticallyTriggerFinalizationObservation_) {
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
    this.automaticallyTriggerFinalizationObservation_ = true;
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
      service.observables_.setObservableData(method, [result]);
      service.observables_.trigger(method);
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
    this.registerMethods_();
    this.registerObservables_();

    this.states_ = [];
    this.stateIndex_ = 0;

    // This state data is more complicated so the behavior of the get/set
    // methods is a little different than other fakes in that they don't return
    // undefined by default.
    this.components_ = [];
    this.setGetLogResult('');
    this.setSaveLogResult({'path': ''});

    this.lastCompleteLast3pDiagnosticsInstallationApproval_ = null;
    this.setGet3pDiagnosticsProviderResult(null);
  }

  /**
   * Setup method resolvers.
   * @private
   */
  registerMethods_() {
    this.methods_ = new FakeMethodResolver();

    this.methods_.register('getCurrentState');
    this.methods_.register('transitionPreviousState');

    this.methods_.register('abortRma');

    this.methods_.register('canExit');
    this.methods_.register('canGoBack');

    this.methods_.register('beginFinalization');

    this.methods_.register('trackConfiguredNetworks');
    this.methods_.register('networkSelectionComplete');

    this.methods_.register('getCurrentOsVersion');
    this.methods_.register('checkForOsUpdates');
    this.methods_.register('updateOs');
    this.methods_.register('updateOsSkipped');

    this.methods_.register('setSameOwner');
    this.methods_.register('setDifferentOwner');
    this.methods_.register('setWipeDevice');

    this.methods_.register('chooseManuallyDisableWriteProtect');
    this.methods_.register('chooseRsuDisableWriteProtect');
    this.methods_.register('getRsuDisableWriteProtectChallenge');
    this.methods_.register('getRsuDisableWriteProtectHwid');
    this.methods_.register('getRsuDisableWriteProtectChallengeQrCode');
    this.methods_.register('setRsuDisableWriteProtectCode');

    this.methods_.register('writeProtectManuallyDisabled');

    this.methods_.register('getWriteProtectDisableCompleteAction');
    this.methods_.register('confirmManualWpDisableComplete');

    this.methods_.register('shutdownForRestock');
    this.methods_.register('continueFinalizationAfterRestock');

    this.methods_.register('getComponentList');
    this.methods_.register('setComponentList');
    this.methods_.register('reworkMainboard');

    this.methods_.register('roFirmwareUpdateComplete');

    this.methods_.register('getRegionList');
    this.methods_.register('getSkuList');
    this.methods_.register('getCustomLabelList');
    this.methods_.register('getOriginalSerialNumber');
    this.methods_.register('getOriginalRegion');
    this.methods_.register('getOriginalSku');
    this.methods_.register('getOriginalCustomLabel');
    this.methods_.register('getOriginalDramPartNumber');
    this.methods_.register('getOriginalFeatureLevel');
    this.methods_.register('setDeviceInformation');

    this.methods_.register('getCalibrationComponentList');
    this.methods_.register('getCalibrationSetupInstructions');
    this.methods_.register('startCalibration');
    this.methods_.register('runCalibrationStep');
    this.methods_.register('continueCalibration');
    this.methods_.register('calibrationComplete');

    this.methods_.register('retryProvisioning');
    this.methods_.register('provisioningComplete');

    this.methods_.register('retryFinalization');
    this.methods_.register('finalizationComplete');

    this.methods_.register('writeProtectManuallyEnabled');

    this.methods_.register('getLog');
    this.methods_.register('saveLog');
    this.methods_.register('getPowerwashRequired');
    this.methods_.register('endRma');

    // Critical error handling
    this.methods_.register('criticalErrorExitToLogin');
    this.methods_.register('criticalErrorReboot');

    this.methods_.register('shutDownAfterHardwareError');

    this.methods_.register('get3pDiagnosticsProvider');
    this.methods_.register('getInstallable3pDiagnosticsAppPath');
    this.methods_.register('installLastFound3pDiagnosticsApp');
    this.methods_.register('show3pDiagnosticsApp');
  }

  /**
   * Setup observables.
   * @private
   */
  registerObservables_() {
    if (this.observables_) {
      this.observables_.stopAllTriggerIntervals();
    }
    this.observables_ = new FakeObservables();
    this.observables_.register('ErrorObserver_onError');
    this.observables_.register('OsUpdateObserver_onOsUpdateProgressUpdated');
    this.observables_.register(
        'UpdateRoFirmwareObserver_onUpdateRoFirmwareStatusChanged');
    this.observables_.register('CalibrationObserver_onCalibrationUpdated');
    this.observables_.register('CalibrationObserver_onCalibrationStepComplete');
    this.observables_.register('ProvisioningObserver_onProvisioningUpdated');
    this.observables_.register(
        'HardwareWriteProtectionStateObserver_onHardwareWriteProtectionStateChanged');
    this.observables_.register(
        'PowerCableStateObserver_onPowerCableStateChanged');
    this.observables_.register(
        'ExternalDiskStateObserver_onExternalDiskStateChanged');
    this.observables_.register(
        'HardwareVerificationStatusObserver_onHardwareVerificationResult');
    this.observables_.register('FinalizationObserver_onFinalizationUpdated');
  }

  /**
   * @param {string} method
   * @param {!State} expectedState
   * @return {!Promise<!{stateResult: !StateResult}>}
   * @private
   */
  getNextStateForMethod_(method, expectedState) {
    if (this.states_.length === 0) {
      this.setFakeStateForMethod_(
          method, State.kUnknown, false, false, RmadErrorCode.kRmaNotRequired);
    } else if (this.stateIndex_ >= this.states_.length - 1) {
      // It should not be possible for stateIndex_ to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex_ < this.states_.length);
      const state = this.states_[this.stateIndex_];
      this.setFakeStateForMethod_(
          method, state.state, state.canExit, state.canGoBack,
          RmadErrorCode.kTransitionFailed);
    } else if (this.states_[this.stateIndex_].state !== expectedState) {
      // Error: Called in wrong state.
      const state = this.states_[this.stateIndex_];
      this.setFakeStateForMethod_(
          method, state.state, state.canExit, state.canGoBack,
          RmadErrorCode.kRequestInvalid);
    } else {
      // Success.
      this.stateIndex_++;
      if (method === 'chooseManuallyDisableWriteProtect') {
        // A special case so that choosing manual WP disable sends you to the
        // appropriate page in the fake app.
        this.stateIndex_++;
      }
      const state = this.states_[this.stateIndex_];
      this.setFakeStateForMethod_(
          method, state.state, state.canExit, state.canGoBack, state.error);
    }
    return this.methods_.resolveMethodWithDelay(
        method, this.resolveMethodDelayMs_);
  }

  /**
   * Sets the value that will be returned when calling getCurrent().
   * @param {!State} state
   * @param {boolean} canExit,
   * @param {boolean} canGoBack,
   * @param {!RmadErrorCode} error
   * @private
   */
  setFakeCurrentState_(state, canExit, canGoBack, error) {
    this.setFakeStateForMethod_(
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
  setFakePrevState_(state, canExit, canGoBack, error) {
    this.setFakeStateForMethod_(
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
  setFakeStateForMethod_(method, state, canExit, canGoBack, error) {
    this.methods_.setResult(
        method, /** @type {{stateResult: !StateResult}} */ ({
          stateResult: {
            state: state,
            canExit: canExit,
            canGoBack: canGoBack,
            error: error,
          },
        }));
  }
}
