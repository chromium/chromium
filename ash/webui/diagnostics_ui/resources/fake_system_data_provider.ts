// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {BatteryChargeStatus, BatteryChargeStatusObserverRemote, BatteryHealth, BatteryHealthObserverRemote, BatteryInfo, CpuUsage, CpuUsageObserverRemote, MemoryUsage, MemoryUsageObserverRemote, SystemDataProviderInterface, SystemInfo} from './system_data_provider.mojom-webui.js';

/**
 * @fileoverview
 * Implements a fake version of the SystemDataProvider mojo interface.
 */

export interface FakeSystemDataProviderInterface {
  setFakeBatteryChargeStatus(batteryChargeStatusList: BatteryChargeStatus[]):
      void;
}

export class FakeSystemDataProvider implements SystemDataProviderInterface,
                                               FakeSystemDataProviderInterface {
  private methods_: FakeMethodResolver = new FakeMethodResolver();
  private observables_: FakeObservables = new FakeObservables();
  private observeBatteryChargeStatusPromise_: Promise<void>|null = null;
  private observeBatteryHealthPromise_: Promise<void>|null = null;
  private observeCpuUsagePromise_: Promise<void>|null = null;
  private observeMemoryUsagePromise_: Promise<void>|null = null;

  constructor() {
    this.registerMethods();
    this.registerObservables();
  }

  getSystemInfo(): Promise<{systemInfo: SystemInfo}> {
    return this.methods_.resolveMethod('getSystemInfo');
  }

  // Sets the value that will be returned when calling getSystemInfo().
  setFakeSystemInfo(systemInfo: SystemInfo): void {
    this.methods_.setResult('getSystemInfo', {systemInfo});
  }

  // Implements SystemDataProviderInterface.GetBatteryInfo.
  getBatteryInfo(): Promise<{batteryInfo: BatteryInfo}> {
    return this.methods_.resolveMethod('getBatteryInfo');
  }

  // Sets the value that will be returned when calling getBatteryInfo().
  setFakeBatteryInfo(batteryInfo: BatteryInfo): void {
    this.methods_.setResult('getBatteryInfo', {batteryInfo});
  }

  // Implements SystemDataProviderInterface.ObserveBatteryChargeStatus.
  observeBatteryChargeStatus(remote: BatteryChargeStatusObserverRemote): void {
    this.observeBatteryChargeStatusPromise_ = this.observe_(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated',
        (batteryChargeStatus) => {
          remote.onBatteryChargeStatusUpdated(
              /** @type {!BatteryChargeStatus} */ (batteryChargeStatus));
        });
  }

  // Returns the promise for the most recent battery charge status observation.
  getObserveBatteryChargeStatusPromiseForTesting(): Promise<void> {
    assert(this.observeBatteryChargeStatusPromise_);
    return this.observeBatteryChargeStatusPromise_;
  }

  /**
   * Sets the values that will observed from observeBatteryChargeStatus.
   */
  setFakeBatteryChargeStatus(batteryChargeStatusList: BatteryChargeStatus[]):
      void {
    this.observables_.setObservableData(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated',
        batteryChargeStatusList);
  }

  // Causes the battery charge status observer to fire.
  triggerBatteryChargeStatusObserver(): void {
    this.observables_.trigger(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated');
  }

  // Implements SystemDataProviderInterface.ObserveBatteryHealth.
  observeBatteryHealth(remote: BatteryHealthObserverRemote): void {
    this.observeBatteryHealthPromise_ = this.observe_(
        'BatteryHealthObserver_onBatteryHealthUpdated', (batteryHealth) => {
          remote.onBatteryHealthUpdated(
              /** @type {!BatteryHealth} */ (batteryHealth));
        });
  }

  // Returns the promise for the most recent battery health observation.
  getObserveBatteryHealthPromiseForTesting(): Promise<void> {
    assert(this.observeBatteryHealthPromise_);
    return this.observeBatteryHealthPromise_;
  }

  // Sets the values that will observed from observeBatteryHealth.
  setFakeBatteryHealth(batteryHealthList: BatteryHealth[]): void {
    this.observables_.setObservableData(
        'BatteryHealthObserver_onBatteryHealthUpdated', batteryHealthList);
  }

  // Causes the battery health observer to fire.
  triggerBatteryHealthObserver(): void {
    this.observables_.trigger('BatteryHealthObserver_onBatteryHealthUpdated');
  }

  // Implements SystemDataProviderInterface.ObserveCpuUsage.
  observeCpuUsage(remote: CpuUsageObserverRemote): void {
    this.observeCpuUsagePromise_ =
        this.observe_('CpuUsageObserver_onCpuUsageUpdated', (cpuUsage) => {
          remote.onCpuUsageUpdated(
              /** @type {!CpuUsage} */ (cpuUsage));
        });
  }

  // Returns the promise for the most recent cpu usage observation.
  getObserveCpuUsagePromiseForTesting(): Promise<void> {
    assert(this.observeCpuUsagePromise_);
    return this.observeCpuUsagePromise_;
  }

  // Sets the values that will observed from observeCpuUsage.
  setFakeCpuUsage(cpuUsageList: CpuUsage[]): void {
    this.observables_.setObservableData(
        'CpuUsageObserver_onCpuUsageUpdated', cpuUsageList);
  }

  // Causes the CPU usage observer to fire.
  triggerCpuUsageObserver(): void {
    this.observables_.trigger('CpuUsageObserver_onCpuUsageUpdated');
  }

  // Implements SystemDataProviderInterface.ObserveMemoryUsage.
  observeMemoryUsage(remote: MemoryUsageObserverRemote): void {
    this.observeCpuUsagePromise_ = this.observe_(
        'MemoryUsageObserver_onMemoryUsageUpdated', (memoryUsage) => {
          remote.onMemoryUsageUpdated(
              /** @type {!MemoryUsage} */ (memoryUsage));
        });
  }

  // Returns the promise for the most recent memory usage observation.
  getObserveMemoryUsagePromiseForTesting(): Promise<void> {
    assert(this.observeCpuUsagePromise_);
    return this.observeCpuUsagePromise_;
  }

  // Sets the values that will observed from ObserveCpuUsage.
  setFakeMemoryUsage(memoryUsageList: MemoryUsage[]): void {
    this.observables_.setObservableData(
        'MemoryUsageObserver_onMemoryUsageUpdated', memoryUsageList);
  }

  // Causes the memory usage observer to fire.
  triggerMemoryUsageObserver(): void {
    this.observables_.trigger('MemoryUsageObserver_onMemoryUsageUpdated');
  }

  // Make the observables fire automatically on various intervals.
  startTriggerIntervals(): void {
    this.observables_.startTriggerOnInterval(
        'CpuUsageObserver_onCpuUsageUpdated', 1000);
    this.observables_.startTriggerOnInterval(
        'MemoryUsageObserver_onMemoryUsageUpdated', 5000);
    this.observables_.startTriggerOnInterval(
        'BatteryHealthObserver_onBatteryHealthUpdated', 30000);
    this.observables_.startTriggerOnInterval(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated', 30000);
  }

  // Stop automatically triggering observables.
  stopTriggerIntervals(): void {
    this.observables_.stopAllTriggerIntervals();
  }

  // Setup method resolvers.
  registerMethods(): void {
    this.methods_.register('getSystemInfo');
    this.methods_.register('getBatteryInfo');
  }

  // Setup observables.
  registerObservables(): void {
    this.observables_.register(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated');
    this.observables_.register('BatteryHealthObserver_onBatteryHealthUpdated');
    this.observables_.register('CpuUsageObserver_onCpuUsageUpdated');
    this.observables_.register('MemoryUsageObserver_onMemoryUsageUpdated');
  }

  // Disables all observers and resets provider to its initial state.
  reset(): void {
    this.observables_.stopAllTriggerIntervals();

    this.methods_ = new FakeMethodResolver();
    this.observables_ = new FakeObservables();

    this.registerMethods();
    this.registerObservables();
  }

  // Sets up an observer for methodName.
  private observe_(methodName: string, callback: (T: any) => void):
      Promise<void> {
    return new Promise((resolve) => {
      this.observables_.observe(methodName, callback);
      this.observables_.trigger(methodName);
      resolve();
    });
  }
}
