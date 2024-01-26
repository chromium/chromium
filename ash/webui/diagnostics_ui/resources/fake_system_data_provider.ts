// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert.js';

import {BatteryChargeStatus, BatteryChargeStatusObserverRemote, BatteryHealth, BatteryHealthObserverRemote, BatteryInfo, CpuUsage, CpuUsageObserverRemote, MemoryUsage, MemoryUsageObserverRemote, SystemDataProviderInterface, SystemInfo} from './system_data_provider.mojom-webui.js';

/**
 * @fileoverview
 * Implements a fake version of the SystemDataProvider mojo interface.
 */

/**
 * Type for methods needed for the fake SystemDataProvider implementation.
 */
export type FakeSystemDataProviderInterface = SystemDataProviderInterface&{
  setFakeBatteryChargeStatus(batteryChargeStatusList: BatteryChargeStatus[]):
      void,
  setFakeBatteryHealth(batteryHealthList: BatteryHealth[]): void,
  setFakeBatteryInfo(batteryInfo: BatteryInfo): void,
  setFakeCpuUsage(cpuUsageList: CpuUsage[]): void,
  setFakeMemoryUsage(memoryUsageList: MemoryUsage[]): void,
  setFakeSystemInfo(systemInfo: SystemInfo): void,
};

export class FakeSystemDataProvider implements FakeSystemDataProviderInterface {
  private methods: FakeMethodResolver = new FakeMethodResolver();
  private observables: FakeObservables = new FakeObservables();
  private observeBatteryChargeStatusPromise: Promise<void>|null = null;
  private observeBatteryHealthPromise: Promise<void>|null = null;
  private observeCpuUsagePromise: Promise<void>|null = null;
  private observeMemoryUsagePromise: Promise<void>|null = null;

  constructor() {
    this.registerMethods();
    this.registerObservables();
  }

  getSystemInfo(): Promise<{systemInfo: SystemInfo}> {
    return this.methods.resolveMethod('getSystemInfo');
  }

  // Sets the value that will be returned when calling getSystemInfo().
  setFakeSystemInfo(systemInfo: SystemInfo): void {
    this.methods.setResult('getSystemInfo', {systemInfo});
  }

  // Implements SystemDataProviderInterface.GetBatteryInfo.
  getBatteryInfo(): Promise<{batteryInfo: BatteryInfo}> {
    return this.methods.resolveMethod('getBatteryInfo');
  }

  // Sets the value that will be returned when calling getBatteryInfo().
  setFakeBatteryInfo(batteryInfo: BatteryInfo): void {
    this.methods.setResult('getBatteryInfo', {batteryInfo});
  }

  // Implements SystemDataProviderInterface.ObserveBatteryChargeStatus.
  observeBatteryChargeStatus(remote: BatteryChargeStatusObserverRemote): void {
    this.observeBatteryChargeStatusPromise = this.observe(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated',
        (batteryChargeStatus) => {
          remote.onBatteryChargeStatusUpdated(
              /** @type {!BatteryChargeStatus} */ (batteryChargeStatus));
        });
  }

  // Returns the promise for the most recent battery charge status observation.
  getObserveBatteryChargeStatusPromiseForTesting(): Promise<void> {
    assert(this.observeBatteryChargeStatusPromise);
    return this.observeBatteryChargeStatusPromise;
  }

  /**
   * Sets the values that will observed from observeBatteryChargeStatus.
   */
  setFakeBatteryChargeStatus(batteryChargeStatusList: BatteryChargeStatus[]):
      void {
    this.observables.setObservableData(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated',
        batteryChargeStatusList);
  }

  // Causes the battery charge status observer to fire.
  triggerBatteryChargeStatusObserver(): void {
    this.observables.trigger(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated');
  }

  // Implements SystemDataProviderInterface.ObserveBatteryHealth.
  observeBatteryHealth(remote: BatteryHealthObserverRemote): void {
    this.observeBatteryHealthPromise = this.observe(
        'BatteryHealthObserver_onBatteryHealthUpdated', (batteryHealth) => {
          remote.onBatteryHealthUpdated(
              /** @type {!BatteryHealth} */ (batteryHealth));
        });
  }

  // Returns the promise for the most recent battery health observation.
  getObserveBatteryHealthPromiseForTesting(): Promise<void> {
    assert(this.observeBatteryHealthPromise);
    return this.observeBatteryHealthPromise;
  }

  // Sets the values that will observed from observeBatteryHealth.
  setFakeBatteryHealth(batteryHealthList: BatteryHealth[]): void {
    this.observables.setObservableData(
        'BatteryHealthObserver_onBatteryHealthUpdated', batteryHealthList);
  }

  // Causes the battery health observer to fire.
  triggerBatteryHealthObserver(): void {
    this.observables.trigger('BatteryHealthObserver_onBatteryHealthUpdated');
  }

  // Implements SystemDataProviderInterface.ObserveCpuUsage.
  observeCpuUsage(remote: CpuUsageObserverRemote): void {
    this.observeCpuUsagePromise =
        this.observe('CpuUsageObserver_onCpuUsageUpdated', (cpuUsage) => {
          remote.onCpuUsageUpdated(
              /** @type {!CpuUsage} */ (cpuUsage));
        });
  }

  // Returns the promise for the most recent cpu usage observation.
  getObserveCpuUsagePromiseForTesting(): Promise<void> {
    assert(this.observeCpuUsagePromise);
    return this.observeCpuUsagePromise;
  }

  // Sets the values that will observed from observeCpuUsage.
  setFakeCpuUsage(cpuUsageList: CpuUsage[]): void {
    this.observables.setObservableData(
        'CpuUsageObserver_onCpuUsageUpdated', cpuUsageList);
  }

  // Causes the CPU usage observer to fire.
  triggerCpuUsageObserver(): void {
    this.observables.trigger('CpuUsageObserver_onCpuUsageUpdated');
  }

  // Implements SystemDataProviderInterface.ObserveMemoryUsage.
  observeMemoryUsage(remote: MemoryUsageObserverRemote): void {
    this.observeCpuUsagePromise = this.observe(
        'MemoryUsageObserver_onMemoryUsageUpdated', (memoryUsage) => {
          remote.onMemoryUsageUpdated(
              /** @type {!MemoryUsage} */ (memoryUsage));
        });
  }

  // Returns the promise for the most recent memory usage observation.
  getObserveMemoryUsagePromiseForTesting(): Promise<void> {
    assert(this.observeCpuUsagePromise);
    return this.observeCpuUsagePromise;
  }

  // Sets the values that will observed from ObserveCpuUsage.
  setFakeMemoryUsage(memoryUsageList: MemoryUsage[]): void {
    this.observables.setObservableData(
        'MemoryUsageObserver_onMemoryUsageUpdated', memoryUsageList);
  }

  // Causes the memory usage observer to fire.
  triggerMemoryUsageObserver(): void {
    this.observables.trigger('MemoryUsageObserver_onMemoryUsageUpdated');
  }

  // Make the observables fire automatically on various intervals.
  startTriggerIntervals(): void {
    this.observables.startTriggerOnInterval(
        'CpuUsageObserver_onCpuUsageUpdated', 1000);
    this.observables.startTriggerOnInterval(
        'MemoryUsageObserver_onMemoryUsageUpdated', 5000);
    this.observables.startTriggerOnInterval(
        'BatteryHealthObserver_onBatteryHealthUpdated', 30000);
    this.observables.startTriggerOnInterval(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated', 30000);
  }

  // Stop automatically triggering observables.
  stopTriggerIntervals(): void {
    this.observables.stopAllTriggerIntervals();
  }

  // Setup method resolvers.
  registerMethods(): void {
    this.methods.register('getSystemInfo');
    this.methods.register('getBatteryInfo');
  }

  // Setup observables.
  registerObservables(): void {
    this.observables.register(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated');
    this.observables.register('BatteryHealthObserver_onBatteryHealthUpdated');
    this.observables.register('CpuUsageObserver_onCpuUsageUpdated');
    this.observables.register('MemoryUsageObserver_onMemoryUsageUpdated');
  }

  // Disables all observers and resets provider to its initial state.
  reset(): void {
    this.observables.stopAllTriggerIntervals();

    this.methods = new FakeMethodResolver();
    this.observables = new FakeObservables();

    this.registerMethods();
    this.registerObservables();
  }

  // Sets up an observer for methodName.
  private observe(methodName: string, callback: (T: any) => void):
      Promise<void> {
    return new Promise((resolve) => {
      this.observables.observe(methodName, callback);
      this.observables.trigger(methodName);
      resolve();
    });
  }
}
