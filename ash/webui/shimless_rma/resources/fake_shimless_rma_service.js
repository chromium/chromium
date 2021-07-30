// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {CalibrationComponent, CalibrationObserverRemote, Component, ComponentRepairStatus, ComponentType, ErrorObserverRemote, HardwareWriteProtectionStateObserverRemote, PowerCableStateObserverRemote, ProvisioningObserverRemote, ProvisioningStep, QrCode, RmadErrorCode, RmaState, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

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
     * Control automatically triggering provisioning observations.
     * @private {boolean}
     */
    this.automaticallyTriggerProvisioningObservation_ = false;

    /**
     * Control automatically triggering calibration observations.
     * @private {boolean}
     */
    this.automaticallyTriggerCalibrationObservation_ = false;

    this.reset();
  }

  /**
   * Set the ordered list of states end error codes for this fake.
   * Setting an empty list (the default) returns kRmaNotRequired for any state
   * function.
   * transitionNextState and transitionPreviousState will move through the fake
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
    // As transitionNextState and transitionPreviousState can modify the result
    // of this function the result must be set at the time of the call.
    if (this.states_.length === 0) {
      this.setFakeCurrentState_(
          RmaState.kUnknown, RmadErrorCode.kRmaNotRequired);
    } else {
      // It should not be possible for stateIndex_ to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex_ < this.states_.length);
      let state = this.states_[this.stateIndex_];
      this.setFakeCurrentState_(state.state, state.error);
    }
    return this.methods_.resolveMethod('getCurrentState');
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  transitionNextState() {
    // As transitionNextState and transitionPreviousState can modify the result
    // of this function the result must be set at the time of the call.
    if (this.states_.length === 0) {
      this.setFakeNextState_(RmaState.kUnknown, RmadErrorCode.kRmaNotRequired);
    } else if (this.stateIndex_ >= this.states_.length - 1) {
      // It should not be possible for stateIndex_ to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex_ < this.states_.length);
      let state = this.states_[this.stateIndex_];
      this.setFakeNextState_(state.state, RmadErrorCode.kTransitionFailed);
    } else {
      this.stateIndex_++;
      let state = this.states_[this.stateIndex_];
      this.setFakeNextState_(state.state, state.error);
    }
    return this.methods_.resolveMethod('transitionNextState');
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  transitionPreviousState() {
    // As transitionNextState and transitionPreviousState can modify the result
    // of this function the result must be set at the time of the call.
    if (this.states_.length === 0) {
      this.setFakePrevState_(RmaState.kUnknown, RmadErrorCode.kRmaNotRequired);
    } else if (this.stateIndex_ === 0) {
      // It should not be possible for stateIndex_ to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex_ < this.states_.length);
      let state = this.states_[this.stateIndex_];
      this.setFakePrevState_(state.state, RmadErrorCode.kTransitionFailed);
    } else {
      this.stateIndex_--;
      let state = this.states_[this.stateIndex_];
      this.setFakePrevState_(state.state, state.error);
    }
    return this.methods_.resolveMethod('transitionPreviousState');
  }

  /**
   * @return {!Promise<!{error: !RmadErrorCode}>}
   */
  abortRma() {
    return this.methods_.resolveMethod('abortRma');
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
        'beginFinalization', RmaState.kWelcomeScreen);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  networkSelectionComplete() {
    return this.getNextStateForMethod_(
        'networkSelectionComplete', RmaState.kConfigureNetwork);
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
   * @return {!Promise<!{updateAvailable: boolean}>}
   */
  checkForOsUpdates() {
    return this.methods_.resolveMethod('checkForOsUpdates');
  }

  /**
   * @param {boolean} available
   */
  setCheckForOsUpdatesResult(available) {
    this.methods_.setResult('checkForOsUpdates', {updateAvailable: available});
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  updateOs() {
    return this.getNextStateForMethod_('updateOs', RmaState.kUpdateOs);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  updateOsSkipped() {
    return this.getNextStateForMethod_('updateOsSkipped', RmaState.kUpdateOs);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  setSameOwner() {
    return this.getNextStateForMethod_(
        'setSameOwner', RmaState.kChooseDestination);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  setDifferentOwner() {
    return this.getNextStateForMethod_(
        'setDifferentOwner', RmaState.kChooseDestination);
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
        RmaState.kChooseWriteProtectDisableMethod);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  chooseRsuDisableWriteProtect() {
    return this.getNextStateForMethod_(
        'chooseRsuDisableWriteProtect',
        RmaState.kChooseWriteProtectDisableMethod);
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
        'setRsuDisableWriteProtectCode', RmaState.kEnterRSUWPDisableCode);
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
        'setComponentList', RmaState.kSelectComponents);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  reworkMainboard() {
    return this.getNextStateForMethod_(
        'reworkMainboard', RmaState.kSelectComponents);
  }

  /**
   * @return {!Promise<!{required: boolean}>}
   */
  reimageRequired() {
    return this.methods_.resolveMethod('reimageRequired');
  }

  /**
   * @param {boolean} required
   */
  setReimageRequiredResult(required) {
    this.methods_.setResult('reimageRequired', {required: required});
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  reimageSkipped() {
    return this.getNextStateForMethod_(
        'reimageSkipped', RmaState.kChooseFirmwareReimageMethod);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  reimageFromDownload() {
    return this.getNextStateForMethod_(
        'reimageFromDownload', RmaState.kChooseFirmwareReimageMethod);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  reimageFromUsb() {
    return this.getNextStateForMethod_(
        'reimageFromUsb', RmaState.kChooseFirmwareReimageMethod);
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
   * @return {!Promise<!{skus: !Array<string>}>}
   */
  getSkuList() {
    return this.methods_.resolveMethod('getSkuList');
  }

  /**
   * @param {!Array<string>} skus
   */
  setGetSkuListResult(skus) {
    this.methods_.setResult('getSkuList', {skus: skus});
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
   * @param {string} serialNumber
   * @param {number} regionIndex
   * @param {number} skuIndex
   * @return {!Promise<!StateResult>}
   */
  setDeviceInformation(serialNumber, regionIndex, skuIndex) {
    // TODO(gavindodd): Validate range of region and sku.
    return this.getNextStateForMethod_(
        'setDeviceInformation', RmaState.kUpdateDeviceInformation);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  finalizeAndReboot() {
    return this.getNextStateForMethod_(
        'finalizeAndReboot', RmaState.kRepairComplete);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  finalizeAndShutdown() {
    return this.getNextStateForMethod_(
        'finalizeAndShutdown', RmaState.kRepairComplete);
  }

  /**
   * @return {!Promise<!StateResult>}
   */
  cutoffBattery() {
    return this.getNextStateForMethod_(
        'cutoffBattery', RmaState.kRepairComplete);
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
   * Implements ShimlessRmaServiceInterface.ObserveCalibration.
   * @param {!CalibrationObserverRemote} remote
   */
  observeCalibrationProgress(remote) {
    this.observables_.observe(
        'CalibrationObserver_onCalibrationUpdated', (component, progress) => {
          remote.onCalibrationUpdated(
              /** @type {!CalibrationComponent} */ (component),
              /** @type {number} */ (progress));
        });
    if (this.automaticallyTriggerCalibrationObservation_) {
      this.triggerCalibrationObserver(
          CalibrationComponent.kAccelerometer, 100, 1500);
    }
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveProvisioning.
   * @param {!ProvisioningObserverRemote} remote
   */
  observeProvisioningProgress(remote) {
    this.observables_.observe(
        'ProvisioningObserver_onProvisioningUpdated', (step, progress) => {
          remote.onProvisioningUpdated(
              /** @type {!ProvisioningStep} */ (step),
              /** @type {number} */ (progress));
        });
    if (this.automaticallyTriggerProvisioningObservation_) {
      // Fake progress over 4 seconds.
      this.triggerProvisioningObserver(
          ProvisioningStep.kInProgress, 0.25, 1000);
      this.triggerProvisioningObserver(ProvisioningStep.kInProgress, 0.5, 2000);
      this.triggerProvisioningObserver(
          ProvisioningStep.kInProgress, 0.75, 3000);
      this.triggerProvisioningObserver(
          ProvisioningStep.kProvisioningComplete, 1.0, 4000);
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
    if (this.automaticallyTriggerDisableWriteProtectionObservation_) {
      this.triggerHardwareWriteProtectionObserver(false, 3000);
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
   * Causes the calibration observer to fire after a delay.
   * @param {!CalibrationComponent} component
   * @param {number} progress
   * @param {number} delayMs
   */
  triggerCalibrationObserver(component, progress, delayMs) {
    return this.triggerObserverAfterMs(
        'CalibrationObserver_onCalibrationUpdated', [component, progress],
        delayMs);
  }

  /**
   * Causes the provisioning observer to fire after a delay.
   * @param {!ProvisioningStep} step
   * @param {number} progress
   * @param {number} delayMs
   */
  triggerProvisioningObserver(step, progress, delayMs) {
    return this.triggerObserverAfterMs(
        'ProvisioningObserver_onProvisioningUpdated', [step, progress],
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
   * Causes an observer to fire after a delay.
   * @param {string} method
   * @param {!T} result
   * @param {number} delayMs
   * @template T
   */
  triggerObserverAfterMs(method, result, delayMs) {
    let setDataTriggerAndResolve = function(service, resolve) {
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
  }

  /**
   * Setup method resolvers.
   * @private
   */
  registerMethods_() {
    this.methods_ = new FakeMethodResolver();

    this.methods_.register('getCurrentState');
    this.methods_.register('transitionNextState');
    this.methods_.register('transitionPreviousState');

    this.methods_.register('abortRma');

    this.methods_.register('beginFinalization');

    this.methods_.register('networkSelectionComplete');

    this.methods_.register('getCurrentOsVersion');
    this.methods_.register('checkForOsUpdates');
    this.methods_.register('updateOs');
    this.methods_.register('updateOsSkipped');

    this.methods_.register('setSameOwner');
    this.methods_.register('setDifferentOwner');

    this.methods_.register('chooseManuallyDisableWriteProtect');
    this.methods_.register('chooseRsuDisableWriteProtect');
    this.methods_.register('getRsuDisableWriteProtectChallenge');
    this.methods_.register('getRsuDisableWriteProtectChallengeQrCode');
    this.methods_.register('setRsuDisableWriteProtectCode');

    this.methods_.register('getComponentList');
    this.methods_.register('setComponentList');
    this.methods_.register('reworkMainboard');

    this.methods_.register('reimageRequired');
    this.methods_.register('reimageSkipped');
    this.methods_.register('reimageFromDownload');
    this.methods_.register('reimageFromUsb');

    this.methods_.register('getRegionList');
    this.methods_.register('getSkuList');
    this.methods_.register('getOriginalSerialNumber');
    this.methods_.register('getOriginalRegion');
    this.methods_.register('getOriginalSku');
    this.methods_.register('setDeviceInformation');

    this.methods_.register('finalizeAndReboot');
    this.methods_.register('finalizeAndShutdown');
    this.methods_.register('cutoffBattery');
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
    this.observables_.register('CalibrationObserver_onCalibrationUpdated');
    this.observables_.register('ProvisioningObserver_onProvisioningUpdated');
    this.observables_.register(
        'HardwareWriteProtectionStateObserver_onHardwareWriteProtectionStateChanged');
    this.observables_.register(
        'PowerCableStateObserver_onPowerCableStateChanged');
  }

  /**
   * @private
   * @param {string} method
   * @param {!RmaState} expectedState
   * @returns {!Promise<!StateResult>}
   */
  getNextStateForMethod_(method, expectedState) {
    if (this.states_.length === 0) {
      this.setFakeStateForMethod_(
          method, RmaState.kUnknown, RmadErrorCode.kRmaNotRequired);
    } else if (this.stateIndex_ >= this.states_.length - 1) {
      // It should not be possible for stateIndex_ to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex_ < this.states_.length);
      let state = this.states_[this.stateIndex_];
      this.setFakeStateForMethod_(
          method, state.state, RmadErrorCode.kTransitionFailed);
    } else if (this.states_[this.stateIndex_].state !== expectedState) {
      // Error: Called in wrong state.
      let state = this.states_[this.stateIndex_];
      this.setFakeStateForMethod_(
          method, state.state, RmadErrorCode.kRequestInvalid);
    } else {
      // Success.
      this.stateIndex_++;
      let state = this.states_[this.stateIndex_];
      this.setFakeStateForMethod_(method, state.state, state.error);
    }
    return this.methods_.resolveMethod(method);
  }

  /**
   * Sets the value that will be returned when calling getCurrent().
   * @private
   * @param {!RmaState} state
   * @param {!RmadErrorCode} error
   */
  setFakeCurrentState_(state, error) {
    this.setFakeStateForMethod_('getCurrentState', state, error);
  }

  /**
   * Sets the value that will be returned when calling transitionNextState().
   * @private
   * @param {!RmaState} state
   * @param {!RmadErrorCode} error
   */
  setFakeNextState_(state, error) {
    this.setFakeStateForMethod_('transitionNextState', state, error);
  }

  /**
   * Sets the value that will be returned when calling
   * transitionPreviousState().
   * @private
   * @param {!RmaState} state
   * @param {!RmadErrorCode} error
   */
  setFakePrevState_(state, error) {
    this.setFakeStateForMethod_('transitionPreviousState', state, error);
  }

  /**
   * Sets the value that will be returned when calling state specific functions
   * that update state. e.g. setSameOwner()
   * @private
   * @param {string} method
   * @param {!RmaState} state
   * @param {!RmadErrorCode} error
   */
  setFakeStateForMethod_(method, state, error) {
    this.methods_.setResult(
        method, /** @type {!StateResult} */ ({state: state, error: error}));
  }
}
