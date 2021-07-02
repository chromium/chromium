// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';

import {BatteryChargeStatus, BatteryChargeStatusObserverRemote, BatteryHealth, BatteryHealthObserverRemote, BatteryInfo, CpuUsage, CpuUsageObserverRemote, ExternalPowerSource, MemoryUsage, MemoryUsageObserverRemote, SystemDataProviderInterface, SystemInfo} from './diagnostics_types.js';

/**
 * @fileoverview
 * Implements a fake version of the SystemDataProvider mojo interface.
 */

/** @implements {SystemDataProviderInterface} */
export class FakeSystemDataProvider {
  constructor() {
    this.methods_ = new FakeMethodResolver();
    this.observables_ = new FakeObservables();

    /** @private {?Promise} */
    this.observeBatteryChargeStatusPromise_ = null;
    /** @private {?Promise} */
    this.observeBatteryHealthPromise_ = null;
    /** @private {?Promise} */
    this.observeCpuUsagePromise_ = null;
    /** @private {?Promise} */
    this.observeMemoryUsagePromise_ = null;

    this.registerMethods();
    this.registerObservables();
  }

  /**
   * @return {!Promise<!{systemInfo: !SystemInfo}>}
   */
  getSystemInfo() {
    return this.methods_.resolveMethod('getSystemInfo');
  }

  /**
   * Sets the value that will be returned when calling getSystemInfo().
   * @param {!SystemInfo} systemInfo
   */
  setFakeSystemInfo(systemInfo) {
    this.methods_.setResult('getSystemInfo', {systemInfo: systemInfo});
  }

  /**
   * Implements SystemDataProviderInterface.GetBatteryInfo.
   * @return {!Promise<!{batteryInfo: !BatteryInfo}>}
   */
  getBatteryInfo() {
    return this.methods_.resolveMethod('getBatteryInfo');
  }

  /**
   * Sets the value that will be returned when calling getBatteryInfo().
   * @param {!BatteryInfo} batteryInfo
   */
  setFakeBatteryInfo(batteryInfo) {
    this.methods_.setResult('getBatteryInfo', {batteryInfo: batteryInfo});
  }

  /**
   * Implements SystemDataProviderInterface.ObserveBatteryChargeStatus.
   * @param {!BatteryChargeStatusObserverRemote} remote
   */
  observeBatteryChargeStatus(remote) {
    this.observeBatteryChargeStatusPromise_ = this.observe_(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated',
        (batteryChargeStatus) => {
          remote.onBatteryChargeStatusUpdated(
              /** @type {!BatteryChargeStatus} */ (batteryChargeStatus));
        });
  }

  /**
   * Returns the promise for the most recent battery charge status observation.
   * @return {?Promise}
   */
  getObserveBatteryChargeStatusPromiseForTesting() {
    return this.observeBatteryChargeStatusPromise_;
  }

  /**
   * Sets the values that will observed from observeBatteryChargeStatus.
   * @param {!Array<!BatteryChargeStatus>} batteryChargeStatusList
   */
  setFakeBatteryChargeStatus(batteryChargeStatusList) {
    this.observables_.setObservableData(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated',
        batteryChargeStatusList);
  }

  /**
   * Causes the battery charge status observer to fire.
   */
  triggerBatteryChargeStatusObserver() {
    this.observables_.trigger(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated');
  }

  /**
   * Implements SystemDataProviderInterface.ObserveBatteryHealth.
   * @param {!BatteryHealthObserverRemote} remote
   */
  observeBatteryHealth(remote) {
    this.observeBatteryHealthPromise_ = this.observe_(
        'BatteryHealthObserver_onBatteryHealthUpdated', (batteryHealth) => {
          remote.onBatteryHealthUpdated(
              /** @type {!BatteryHealth} */ (batteryHealth));
        });
  }

  /**
   * Returns the promise for the most recent battery health observation.
   * @return {?Promise}
   */
  getObserveBatteryHealthPromiseForTesting() {
    return this.observeBatteryHealthPromise_;
  }

  /**
   * Sets the values that will observed from observeBatteryHealth.
   * @param {!Array<!BatteryHealth>} batteryHealthList
   */
  setFakeBatteryHealth(batteryHealthList) {
    this.observables_.setObservableData(
        'BatteryHealthObserver_onBatteryHealthUpdated', batteryHealthList);
  }

  /**
   * Causes the battery health observer to fire.
   */
  triggerBatteryHealthObserver() {
    this.observables_.trigger('BatteryHealthObserver_onBatteryHealthUpdated');
  }

  /**
   * Implements SystemDataProviderInterface.ObserveCpuUsage.
   * @param {!CpuUsageObserverRemote} remote
   */
  observeCpuUsage(remote) {
    this.observeCpuUsagePromise_ =
        this.observe_('CpuUsageObserver_onCpuUsageUpdated', (cpuUsage) => {
          remote.onCpuUsageUpdated(
              /** @type {!CpuUsage} */ (cpuUsage));
        });
  }

  /**
   * Returns the promise for the most recent cpu usage observation.
   * @return {?Promise}
   */
  getObserveCpuUsagePromiseForTesting() {
    return this.observeCpuUsagePromise_;
  }

  /**
   * Sets the values that will observed from observeCpuUsage.
   * @param {!Array<!CpuUsage>} cpuUsageList
   */
  setFakeCpuUsage(cpuUsageList) {
    this.observables_.setObservableData(
        'CpuUsageObserver_onCpuUsageUpdated', cpuUsageList);
  }

  /**
   * Causes the CPU usage observer to fire.
   */
  triggerCpuUsageObserver() {
    this.observables_.trigger('CpuUsageObserver_onCpuUsageUpdated');
  }

  /**
   *
   * Implements SystemDataProviderInterface.ObserveMemoryUsage.
   * @param {!MemoryUsageObserverRemote} remote
   */
  observeMemoryUsage(remote) {
    this.observeCpuUsagePromise_ = this.observe_(
        'MemoryUsageObserver_onMemoryUsageUpdated', (memoryUsage) => {
          remote.onMemoryUsageUpdated(
              /** @type {!MemoryUsage} */ (memoryUsage));
        });
  }

  /**
   * Returns the promise for the most recent memory usage observation.
   * @return {?Promise}
   */
  getObserveMemoryUsagePromiseForTesting() {
    return this.observeCpuUsagePromise_;
  }

  /**
   * Sets the values that will observed from ObserveCpuUsage.
   * @param {!Array<!MemoryUsage>} memoryUsageList
   */
  setFakeMemoryUsage(memoryUsageList) {
    this.observables_.setObservableData(
        'MemoryUsageObserver_onMemoryUsageUpdated', memoryUsageList);
  }

  /**
   * Causes the memory usage observer to fire.
   */
  triggerMemoryUsageObserver() {
    this.observables_.trigger('MemoryUsageObserver_onMemoryUsageUpdated');
  }

  /**
   * Make the observables fire automatically on various intervals.
   */
  startTriggerIntervals() {
    this.observables_.startTriggerOnInterval(
        'CpuUsageObserver_onCpuUsageUpdated', 1000);
    this.observables_.startTriggerOnInterval(
        'MemoryUsageObserver_onMemoryUsageUpdated', 5000);
    this.observables_.startTriggerOnInterval(
        'BatteryHealthObserver_onBatteryHealthUpdated', 30000);
    this.observables_.startTriggerOnInterval(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated', 30000);
  }

  /**
   * Stop automatically triggering observables.
   */
  stopTriggerIntervals() {
    this.observables_.stopAllTriggerIntervals();
  }

  /**
   * Setup method resolvers.
   */
  registerMethods() {
    this.methods_.register('getSystemInfo');
    this.methods_.register('getBatteryInfo');
  }

  /**
   * Setup observables.
   */
  registerObservables() {
    this.observables_.register(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated');
    this.observables_.register('BatteryHealthObserver_onBatteryHealthUpdated');
    this.observables_.register('CpuUsageObserver_onCpuUsageUpdated');
    this.observables_.register('MemoryUsageObserver_onMemoryUsageUpdated');
  }

  /**
   * Disables all observers and resets provider to its initial state.
   */
  reset() {
    this.observables_.stopAllTriggerIntervals();

    this.methods_ = new FakeMethodResolver();
    this.observables_ = new FakeObservables();

    this.registerMethods();
    this.registerObservables();
  }

  /**
   * Sets up an observer for methodName.
   * @template T
   * @param {string} methodName
   * @param {!function(!T)} callback
   * @return {!Promise}
   * @private
   */
  observe_(methodName, callback) {
    return new Promise((resolve) => {
      this.observables_.observe(methodName, callback);
      this.observables_.trigger(methodName);
      resolve();
    });
  }
}
