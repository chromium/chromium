// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert.m.js';

import {CalibrationComponentStatus, CalibrationObserverRemote, CalibrationOverallStatus, CalibrationSetupInstruction, CalibrationStatus, Component, ComponentType, ErrorObserverRemote, FinalizationError, FinalizationObserverRemote, FinalizationStatus, HardwareVerificationStatusObserverRemote, HardwareWriteProtectionStateObserverRemote, OsUpdateObserverRemote, OsUpdateOperation, PowerCableStateObserverRemote, ProvisioningError, ProvisioningObserverRemote, ProvisioningStatus, QrCode, RmadErrorCode, ShimlessRmaServiceInterface, ShutdownMethod, State, StateResult, UpdateErrorCode, UpdateRoFirmwareObserverRemote, UpdateRoFirmwareStatus, WriteProtectDisableCompleteAction} from './shimless_rma_types.js';

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
   * @return {!Promise<!StateResult>}
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
          state.state, state.canCancel, state.canGoBack, state.error);
    }
    return this.methods_.resolveMethodWithDelay(
        'getCurrentState', this.resolveMethodDelayMs_);
  }

  /**
   * @return {!Promise<!StateResult>}
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
          state.state, state.canCancel, state.canGoBack,
          RmadErrorCode.kTransitionFailed);
    } else {
      this.stateIndex_--;
      const state = this.states_[this.stateIndex_];
      this.setFakePrevState_(
          state.state, state.canCancel, state.canGoBack, state.error);
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
   * @return {!Promise<!StateResult>}
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
   * @return {!Promise<!StateResult>}
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
   * @param {string} version
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
          OsUpdateOperation.kCheckingForUpdate, 0.1, 500);
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kUpdateAvailable, 0.3, 1000);
      this.triggerOsUpdateObserver(OsUpdateOperation.kDownloading, 0.5, 1500);
      this.triggerOsUpdateObserver(OsUpdateOperation.kVerifying, 0.7, 2000);
      this.triggerOsUpdateObserver(OsUpdateOperation.kFinalizing, 0.9, 2500);
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kUpdatedNeedReboot, 1.0, 3000);
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
   * @return {!Promise<!StateResult>}
   */
  updateOsSkipped() {
    return this.getNextStateForMethod_('updateOsSkipped', State.kUpdateOs);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  setSameOwner() {
    return this.getNextStateForMethod_(
        'setSameOwner', State.kChooseDestination);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  setDifferentOwner() {
    return this.getNextStateForMethod_(
        'setDifferentOwner', State.kChooseDestination);
  }

  /**
   * @param {boolean} shouldWipeDevice
   * @return {!Promise<!StateResult>}
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
   * @return {!Promise<!StateResult>}
   */
  chooseManuallyDisableWriteProtect() {
    return this.getNextStateForMethod_(
        'chooseManuallyDisableWriteProtect',
        State.kChooseWriteProtectDisableMethod);
  }

  /**
   * @return {!Promise<!StateResult>}
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
    this.methods_.setResult(
        'getRsuDisableWriteProtectHwid', {hwid: hwid});
  }

  /**
   * @return {!Promise<!{qrCode: QrCode}>}
   */
  getRsuDisableWriteProtectChallengeQrCode() {
    return this.methods_.resolveMethod(
        'getRsuDisableWriteProtectChallengeQrCode');
  }

  /**
   * @param {!QrCode} qrCode
   */
  setGetRsuDisableWriteProtectChallengeQrCodeResponse(qrCode) {
    this.methods_.setResult(
        'getRsuDisableWriteProtectChallengeQrCode', {qrCode: qrCode});
  }

  /**
   * @param {string} code
   * @return {!Promise<!StateResult>}
   */
  setRsuDisableWriteProtectCode(code) {
    return this.getNextStateForMethod_(
        'setRsuDisableWriteProtectCode', State.kEnterRSUWPDisableCode);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  writeProtectManuallyDisabled() {
    return this.getNextStateForMethod_(
        'writeProtectManuallyDisabled', State.kWaitForManualWPDisable);
  }

  /**
   * @return {!Promise<!{displayUrl: string, qrCode: ?QrCode}>}
   */
  getWriteProtectManuallyDisabledInstructions() {
    return this.methods_.resolveMethod(
        'getWriteProtectManuallyDisabledInstructions');
  }

  /**
   * @param {string} displayUrl
   * @param {!QrCode} qrCode
   */
  setGetWriteProtectManuallyDisabledInstructionsResult(displayUrl, qrCode) {
    this.methods_.setResult(
        'getWriteProtectManuallyDisabledInstructions',
        {displayUrl: displayUrl, qrCode: qrCode});
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
   * @return {!Promise<!StateResult>}
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
   * @return {!Promise<!StateResult>}
   */
  setComponentList(components) {
    return this.getNextStateForMethod_(
        'setComponentList', State.kSelectComponents);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  reworkMainboard() {
    return this.getNextStateForMethod_(
        'reworkMainboard', State.kSelectComponents);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  roFirmwareUpdateComplete() {
    return this.getNextStateForMethod_(
        'roFirmwareUpdateComplete', State.kUpdateRoFirmware);
  }

  /**
   * @return {!Promise<!StateResult>}
   *
   */
  shutdownForRestock() {
    return this.getNextStateForMethod_('shutdownForRestock', State.kRestock);
  }

  /**
   * @return {!Promise<!StateResult>}
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
   * @return {!Promise<!{whiteLabels: !Array<string>}>}
   */
  getWhiteLabelList() {
    return this.methods_.resolveMethod('getWhiteLabelList');
  }

  /**
   * @param {!Array<string>} whiteLabels
   */
  setGetWhiteLabelListResult(whiteLabels) {
    this.methods_.setResult('getWhiteLabelList', {whiteLabels: whiteLabels});
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
   * @return {!Promise<!{whiteLabelIndex: number}>}
   */
  getOriginalWhiteLabel() {
    return this.methods_.resolveMethod('getOriginalWhiteLabel');
  }

  /**
   * @param {number} whiteLabelIndex
   */
  setGetOriginalWhiteLabelResult(whiteLabelIndex) {
    this.methods_.setResult(
        'getOriginalWhiteLabel', {whiteLabelIndex: whiteLabelIndex});
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
   * @param {string} serialNumber
   * @param {number} regionIndex
   * @param {number} skuIndex
   * @param {number} whiteLabelIndex
   * @param {string} dramPartNumber
   * @return {!Promise<!StateResult>}
   */
  setDeviceInformation(
      serialNumber, regionIndex, skuIndex, whiteLabelIndex, dramPartNumber) {
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
   * @return {!Promise<!StateResult>}
   */
  startCalibration(unused) {
    return this.getNextStateForMethod_(
        'startCalibration', State.kCheckCalibration);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  runCalibrationStep() {
    return this.getNextStateForMethod_(
        'runCalibrationStep', State.kSetupCalibration);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  continueCalibration() {
    return this.getNextStateForMethod_(
        'continueCalibration', State.kRunCalibration);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  calibrationComplete() {
    return this.getNextStateForMethod_(
        'calibrationComplete', State.kRunCalibration);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  retryProvisioning() {
    return this.getNextStateForMethod_(
        'retryProvisioning', State.kProvisionDevice);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  provisioningComplete() {
    return this.getNextStateForMethod_(
        'provisioningComplete', State.kProvisionDevice);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  finalizationComplete() {
    return this.getNextStateForMethod_('finalizationComplete', State.kFinalize);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  retryFinalization() {
    return this.getNextStateForMethod_('retryFinalization', State.kFinalize);
  }

  /**
   * @return {!Promise<!StateResult>}
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
   * @return {!Promise<!StateResult>}
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
            progress: 0.0
          },
          1000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationInProgress,
            progress: 0.2
          },
          2000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationInProgress,
            progress: 0.4
          },
          3000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationInProgress,
            progress: 0.6
          },
          4000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationInProgress,
            progress: 0.8
          },
          5000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kLidAccelerometer,
            status: CalibrationStatus.kCalibrationWaiting,
            progress: 0.0
          },
          6000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationComplete,
            progress: 0.5
          },
          7000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationFailed,
            progress: 1.0
          },
          8000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseGyroscope,
            status: CalibrationStatus.kCalibrationSkip,
            progress: 1.0
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
          ProvisioningStatus.kInProgress, 0.25, 1000);
      this.triggerProvisioningObserver(
          ProvisioningStatus.kInProgress, 0.5, 2000);
      this.triggerProvisioningObserver(
          ProvisioningStatus.kInProgress, 0.75, 3000);
      this.triggerProvisioningObserver(ProvisioningStatus.kComplete, 1.0, 4000);
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
          FinalizationStatus.kInProgress, 0.25, 1000);
      this.triggerFinalizationObserver(
          FinalizationStatus.kInProgress, 0.75, 2000);
      this.triggerFinalizationObserver(FinalizationStatus.kComplete, 1.0, 3000);
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
   * @param {number} delayMs
   */
  triggerOsUpdateObserver(operation, progress, delayMs) {
    return this.triggerObserverAfterMs(
        'OsUpdateObserver_onOsUpdateProgressUpdated', [operation, progress],
        delayMs);
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
   * @param {number} delayMs
   */
  triggerProvisioningObserver(status, progress, delayMs) {
    return this.triggerObserverAfterMs(
        'ProvisioningObserver_onProvisioningUpdated', [status, progress],
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
   * @param {number} delayMs
   */
  triggerFinalizationObserver(status, progress, delayMs) {
    return this.triggerObserverAfterMs(
        'FinalizationObserver_onFinalizationUpdated', [status, progress],
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

    this.methods_.register('canCancel');
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
    this.methods_.register('getWriteProtectManuallyDisabledInstructions');
    this.methods_.register(
        'setGetWriteProtectManuallyDisabledInstructionsResult');

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
    this.methods_.register('getWhiteLabelList');
    this.methods_.register('getOriginalSerialNumber');
    this.methods_.register('getOriginalRegion');
    this.methods_.register('getOriginalSku');
    this.methods_.register('getOriginalWhiteLabel');
    this.methods_.register('getOriginalDramPartNumber');
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
    this.methods_.register('getPowerwashRequired');
    this.methods_.register('endRma');

    // Critical error handling
    this.methods_.register('criticalErrorExitToLogin');
    this.methods_.register('criticalErrorReboot');
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
        'HardwareVerificationStatusObserver_onHardwareVerificationResult');
    this.observables_.register('FinalizationObserver_onFinalizationUpdated');
  }

  /**
   * @param {string} method
   * @param {!State} expectedState
   * @returns {!Promise<!StateResult>}
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
          method, state.state, state.canCancel, state.canGoBack,
          RmadErrorCode.kTransitionFailed);
    } else if (this.states_[this.stateIndex_].state !== expectedState) {
      // Error: Called in wrong state.
      const state = this.states_[this.stateIndex_];
      this.setFakeStateForMethod_(
          method, state.state, state.canCancel, state.canGoBack,
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
          method, state.state, state.canCancel, state.canGoBack, state.error);
    }
    return this.methods_.resolveMethodWithDelay(
        method, this.resolveMethodDelayMs_);
  }

  /**
   * Sets the value that will be returned when calling getCurrent().
   * @param {!State} state
   * @param {boolean} canCancel,
   * @param {boolean} canGoBack,
   * @param {!RmadErrorCode} error
   * @private
   */
  setFakeCurrentState_(state, canCancel, canGoBack, error) {
    this.setFakeStateForMethod_(
        'getCurrentState', state, canCancel, canGoBack, error);
  }

  /**
   * Sets the value that will be returned when calling
   * transitionPreviousState().
   * @param {!State} state
   * @param {boolean} canCancel,
   * @param {boolean} canGoBack,
   * @param {!RmadErrorCode} error
   * @private
   */
  setFakePrevState_(state, canCancel, canGoBack, error) {
    this.setFakeStateForMethod_(
        'transitionPreviousState', state, canCancel, canGoBack, error);
  }

  /**
   * Sets the value that will be returned when calling state specific functions
   * that update state. e.g. setSameOwner()
   * @param {string} method
   * @param {!State} state
   * @param {boolean} canCancel,
   * @param {boolean} canGoBack,
   * @param {!RmadErrorCode} error
   * @private
   */
  setFakeStateForMethod_(method, state, canCancel, canGoBack, error) {
    this.methods_.setResult(method, /** @type {!StateResult} */ ({
                              state: state,
                              canCancel: canCancel,
                              canGoBack: canGoBack,
                              error: error
                            }));
  }
}
