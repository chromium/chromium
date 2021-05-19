// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert.m.js';

import {CalibrationComponent, CalibrationObserver, Component, ComponentRepairState, ComponentType, CurrentState, ErrorObserver, HardwareWriteProtectionStateObserver, NextState, PowerCableStateObserver, PrevState, ProvisioningObserver, ProvisioningStep, RmadErrorCode, RmaState, ShimlessRmaServiceInterface, State} from './shimless_rma_types.js';

/** @implements {ShimlessRmaServiceInterface} */
export class FakeShimlessRmaService {
  constructor() {
    this.methods_ = new FakeMethodResolver();
    this.observables_ = new FakeObservables();

    /**
     * The list of states for this RMA flow.
     * @private {!Array<!State>}
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
     * The current device serial number.
     * @private {string}
     */
    this.serialNumber_ = '';

    /**
     * The result of calling setSerialNumber.
     * @private {!RmadErrorCode}
     */
    this.setSerialNumberResult_ = RmadErrorCode.kOk;

    /**
     * The current index into the regions provided by getRegionList().
     * Use setGetRegionListResult([region,....]) to update the list of regions.
     * @private {number}
     */
    this.regionIndex_ = 0;

    /**
     * The result of calling setRegion.
     * @private {!RmadErrorCode}
     */
    this.setRegionResult_ = RmadErrorCode.kOk;

    /**
     * The current index into the skus provided by getSkuList().
     * Use setGetSkuListResult([sku,....]) to update the list of skus.
     * @private {number}
     */
    this.skuIndex_ = 0;

    /**
     * The result of calling setSku.
     * @private {!RmadErrorCode}
     */
    this.setSkuResult_ = RmadErrorCode.kOk;

    this.reset();
  }

  /**
   * Set the ordered list of states end error codes for this fake.
   * Setting an empty list (the default) returns kRmaNotRequired for any state
   * function.
   * getNextState and getPrevState will move through the fake state through the
   * list, and return kTransitionFailed if it would move off either end.
   * getCurrentState always return the state at the current index.
   *
   * @param {!Array<!State>} states
   */
  setStates(states) {
    this.states_ = states;
    this.stateIndex_ = 0;
  }

  /**
   * @return {!Promise<!CurrentState>}
   */
  getCurrentState() {
    // As getNextState and getPrevState can modify the result of this function
    // the result must be set at the time of the call.
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
   * @return {!Promise<!NextState>}
   */
  getNextState() {
    // As getNextState and getPrevState can modify the result of this function
    // the result must be set at the time of the call.
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
    return this.methods_.resolveMethod('getNextState');
  }

  /**
   * @return {!Promise<!PrevState>}
   */
  getPrevState() {
    // As getNextState and getPrevState can modify the result of this function
    // the result must be set at the time of the call.
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
    return this.methods_.resolveMethod('getPrevState');
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
   * @return {!Promise<!{version: string}>}
   */
  getCurrentChromeVersion() {
    return this.methods_.resolveMethod('getCurrentChromeVersion');
  }

  /**
   * @param {string} version
   */
  setGetCurrentChromeVersionResult(version) {
    this.methods_.setResult('getCurrentChromeVersion', {version: version});
  }

  /**
   * @return {!Promise<!{updateAvailable: boolean}>}
   */
  checkForChromeUpdates() {
    return this.methods_.resolveMethod('checkForChromeUpdates');
  }

  /**
   * @param {boolean} available
   */
  setCheckForChromeUpdatesResult(available) {
    this.methods_.setResult(
        'checkForChromeUpdates', {updateAvailable: available});
  }

  /**
   * @return {!Promise<!{error: !RmadErrorCode}>}
   */
  updateChrome() {
    return this.methods_.resolveMethod('updateChrome');
  }

  /**
   * @param {!RmadErrorCode} error
   */
  setUpdateChromeResult(error) {
    this.methods_.setResult('updateChrome', {error: error});
  }

  /**
   * @return {!Promise<!NextState>}
   */
  setSameOwner() {
    return this.getNextStateForMethod_(
        'setSameOwner', RmaState.kChooseDestination);
  }

  /**
   * @return {!Promise<!NextState>}
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
   * @return {!Promise<!NextState>}
   */
  manuallyDisableWriteProtect() {
    return this.getNextStateForMethod_(
        'manuallyDisableWriteProtect',
        RmaState.kChooseWriteProtectDisableMethod);
  }

  /**
   * @param {string} code
   * @return {!Promise<!NextState>}
   */
  rsuDisableWriteProtect(code) {
    return this.getNextStateForMethod_(
        'rsuDisableWriteProtect', RmaState.kChooseWriteProtectDisableMethod);
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
   * @param {!ComponentType} componentType
   * @return {!Promise<!{repairState: !ComponentRepairState}>}
   */
  toggleComponentReplaced(componentType) {
    let result = ComponentRepairState.kRepairUnknown;
    for (let component of this.components_) {
      if (component.component === componentType) {
        if (component.state === ComponentRepairState.kOriginal) {
          component.state = ComponentRepairState.kReplaced;
        } else if (component.state === ComponentRepairState.kReplaced) {
          component.state = ComponentRepairState.kOriginal;
        }
        result = component.state;
        break;
      }
    }
    this.methods_.setResult('toggleComponentReplaced', {repairState: result});
    return this.methods_.resolveMethod('toggleComponentReplaced');
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
   * @return {!Promise<!NextState>}
   */
  reimageSkipped() {
    return this.getNextStateForMethod_(
        'reimageSkipped', RmaState.kChooseFirmwareReimageMethod);
  }

  /**
   * @return {!Promise<!NextState>}
   */
  reimageFromDownload() {
    return this.getNextStateForMethod_(
        'reimageFromDownload', RmaState.kChooseFirmwareReimageMethod);
  }

  /**
   * @return {!Promise<!NextState>}
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
    this.serialNumber_ = serialNumber;
    this.methods_.setResult(
        'getOriginalSerialNumber', {serialNumber: serialNumber});
  }

  /**
   * @return {!Promise<!{serialNumber: string}>}
   */
  getSerialNumber() {
    this.methods_.setResult(
        'getSerialNumber', {serialNumber: this.serialNumber_});
    return this.methods_.resolveMethod('getSerialNumber');
  }

  /**
   * @param {string} serialNumber
   * @return {!Promise<!{error: !RmadErrorCode}>}
   */
  setSerialNumber(serialNumber) {
    if (this.setSerialNumberResult_ === RmadErrorCode.kOk) {
      this.serialNumber_ = serialNumber;
    }
    return this.methods_.resolveMethod('setSerialNumber');
  }

  /**
   * @param {!RmadErrorCode} error
   */
  setSetSerialNumberResult(error) {
    this.setSerialNumberResult_ = error;
    this.methods_.setResult('setSerialNumber', {error: error});
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
    this.regionIndex_ = regionIndex;
    this.methods_.setResult('getOriginalRegion', {regionIndex: regionIndex});
  }

  /**
   * @return {!Promise<!{regionIndex: number}>}
   */
  getRegion() {
    this.methods_.setResult('getRegion', {regionIndex: this.regionIndex_});
    return this.methods_.resolveMethod('getRegion');
  }

  /**
   * @param {number} regionIndex
   * @return {!Promise<!{error: !RmadErrorCode}>}
   */
  setRegion(regionIndex) {
    // TODO(gavindodd): Validate range of index.
    if (this.setRegionResult_ === RmadErrorCode.kOk) {
      this.regionIndex_ = regionIndex;
    }
    return this.methods_.resolveMethod('setRegion');
  }

  /**
   * @param {!RmadErrorCode} error
   */
  setSetRegionResult(error) {
    this.setRegionResult_ = error;
    this.methods_.setResult('setRegion', {error: error});
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
    this.skuIndex_ = skuIndex;
    this.methods_.setResult('getOriginalSku', {skuIndex: skuIndex});
  }

  /**
   * @return {!Promise<!{skuIndex: number}>}
   */
  getSku() {
    this.methods_.setResult('getSku', {skuIndex: this.skuIndex_});
    return this.methods_.resolveMethod('getSku');
  }

  /**
   * @param {number} skuIndex
   * @return {!Promise<!{error: !RmadErrorCode}>}
   */
  setSku(skuIndex) {
    // TODO(gavindodd): Validate range of index.
    if (this.setSkuResult_ === RmadErrorCode.kOk) {
      this.skuIndex_ = skuIndex;
    }
    return this.methods_.resolveMethod('setSku');
  }

  /**
   * @param {!RmadErrorCode} error
   */
  setSetSkuResult(error) {
    this.setSkuResult_ = error;
    this.methods_.setResult('setSku', {error: error});
  }

  /**
   * @return {!Promise<!{error: !RmadErrorCode}>}
   */
  cutoffBattery() {
    return this.methods_.resolveMethod('cutoffBattery');
  }

  /**
   * @param {!RmadErrorCode} error
   */
  setCutoffBatteryResult(error) {
    this.methods_.setResult('cutoffBattery', {error: error});
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveError.
   * @param {!ErrorObserver} remote
   */
  observeError(remote) {
    this.observables_.observe('ErrorObserver_onError', (error) => {
      remote.onError(
          /** @type {!RmadErrorCode} */ (error));
    });
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveCalibration.
   * @param {!CalibrationObserver} remote
   */
  observeCalibration(remote) {
    this.observables_.observe(
        'CalibrationObserver_onCalibrationUpdated', (component, progress) => {
          remote.onCalibrationUpdated(
              /** @type {!CalibrationComponent} */ (component),
              /** @type {number} */ (progress));
        });
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveProvisioning.
   * @param {!ProvisioningObserver} remote
   */
  observeProvisioning(remote) {
    this.observables_.observe(
        'ProvisioningObserver_onProvisioningUpdated', (step, progress) => {
          remote.onProvisioningUpdated(
              /** @type {!ProvisioningStep} */ (step),
              /** @type {number} */ (progress));
        });
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveHardwareWriteProtectionState.
   * @param {!HardwareWriteProtectionStateObserver} remote
   */
  observeHardwareWriteProtectionState(remote) {
    this.observables_.observe(
        'HardwareWriteProtectionStateObserver_onHardwareWriteProtectionStateChanged',
        (enabled) => {
          remote.onHardwareWriteProtectionStateChanged(
              /** @type {boolean} */ (enabled));
        });
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObservePowerCableState.
   * @param {!PowerCableStateObserver} remote
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
    let setDataTriggerAndResolve = function (service, resolve) {
      service.observables_.setObservableData(method, [result]);
      service.observables_.trigger(method);
      resolve();
    }
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
    this.serialNumber_ = '';
    this.setSetSerialNumberResult(RmadErrorCode.kOk);
    this.regionIndex_ = 0;
    this.setSetRegionResult(RmadErrorCode.kOk);
    this.skuIndex_ = 0;
    this.setSetSkuResult(RmadErrorCode.kOk);
  }

  /**
   * Setup method resolvers.
   * @private
   */
  registerMethods_() {
    this.methods_ = new FakeMethodResolver();

    this.methods_.register('getCurrentState');
    this.methods_.register('getNextState');
    this.methods_.register('getPrevState');

    this.methods_.register('abortRma');

    this.methods_.register('getCurrentChromeVersion');
    this.methods_.register('checkForChromeUpdates');
    this.methods_.register('updateChrome');

    this.methods_.register('setSameOwner');
    this.methods_.register('setDifferentOwner');

    this.methods_.register('manualDisableWriteProtectAvailable');
    this.methods_.register('manuallyDisableWriteProtect');
    this.methods_.register('rsuDisableWriteProtect');

    this.methods_.register('getComponentList');
    this.methods_.register('toggleComponentReplaced');

    this.methods_.register('reimageRequired');
    this.methods_.register('reimageSkipped');
    this.methods_.register('reimageFromDownload');
    this.methods_.register('reimageFromUsb');

    this.methods_.register('getRegionList');
    this.methods_.register('getSkuList');
    this.methods_.register('getOriginalSerialNumber');
    this.methods_.register('getSerialNumber');
    this.methods_.register('setSerialNumber');
    this.methods_.register('getOriginalRegion');
    this.methods_.register('getRegion');
    this.methods_.register('setRegion');
    this.methods_.register('getOriginalSku');
    this.methods_.register('getSku');
    this.methods_.register('setSku');

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
   * @returns {!Promise<!NextState>}
   */
  getNextStateForMethod_(method, expectedState) {
    if (this.states_.length === 0) {
      this.setFakeNextStateForMethod_(
          method, RmaState.kUnknown, RmadErrorCode.kRmaNotRequired);
    } else if (this.stateIndex_ >= this.states_.length - 1) {
      // It should not be possible for stateIndex_ to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex_ < this.states_.length);
      let state = this.states_[this.stateIndex_];
      this.setFakeNextStateForMethod_(
          method, state.state, RmadErrorCode.kTransitionFailed);
    } else if (this.states_[this.stateIndex_].state !== expectedState) {
      // Error: Called in wrong state.
      let state = this.states_[this.stateIndex_];
      this.setFakeNextStateForMethod_(
          method, state.state, RmadErrorCode.kRequestInvalid);
    } else {
      // Success.
      this.stateIndex_++;
      let state = this.states_[this.stateIndex_];
      this.setFakeNextStateForMethod_(method, state.state, state.error);
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
    this.methods_.setResult(
        'getCurrentState', {currentState: state, error: error});
  }

  /**
   * Sets the value that will be returned when calling state specific functions
   * that progress state. e.g. setSameOwner()
   * @private
   * @param {string} method
   * @param {!RmaState} state
   * @param {!RmadErrorCode} error
   */
  setFakeNextStateForMethod_(method, state, error) {
    this.methods_.setResult(method, {nextState: state, error: error});
  }

  /**
   * Sets the value that will be returned when calling getNextState().
   * @private
   * @param {!RmaState} state
   * @param {!RmadErrorCode} error
   */
  setFakeNextState_(state, error) {
    this.methods_.setResult('getNextState', {nextState: state, error: error});
  }

  /**
   * Sets the value that will be returned when calling getPrevState().
   * @private
   * @param {!RmaState} state
   * @param {!RmadErrorCode} error
   */
  setFakePrevState_(state, error) {
    this.methods_.setResult('getPrevState', {prevState: state, error: error});
  }
}
