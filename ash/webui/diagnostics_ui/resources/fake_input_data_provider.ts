// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {GetConnectedDevicesResponse} from './diagnostics_types.js';
import {KeyboardInfo} from './input.mojom-webui.js';
import {ConnectedDevicesObserverRemote, InputDataProviderInterface, InternalDisplayPowerStateObserverRemote, KeyboardObserverRemote, LidStateObserverRemote, TabletModeObserverRemote, TouchDeviceInfo} from './input_data_provider.mojom-webui.js';

/**
 * @fileoverview
 * Implements a fake version of the InputDataProvider Mojo interface.
 */

export class FakeInputDataProvider implements InputDataProviderInterface {
  private methods: FakeMethodResolver = new FakeMethodResolver();
  private observers: ConnectedDevicesObserverRemote[] = [];
  private keyboards: KeyboardInfo[] = [];
  private keyboardObservers: KeyboardObserverRemote[][] = [];
  private tabletModeObserver: TabletModeObserverRemote;
  private lidStateObserver: LidStateObserverRemote;
  private internalDisplayPowerStateObserver:
      InternalDisplayPowerStateObserverRemote;
  private touchDevices: TouchDeviceInfo[] = [];
  private moveAppToTestingScreenCalled: number = 0;
  private moveAppBackToPreviousScreenCalled: number = 0;
  private a11yTouchPassthroughState: boolean = false;
  constructor() {
    this.registerMethods();
  }

  // Resets provider to its internal state.
  reset(): void {
    this.methods = new FakeMethodResolver();
    this.observers = [];
    this.keyboards = [];
    this.touchDevices = [];
    this.moveAppToTestingScreenCalled = 0;
    this.moveAppBackToPreviousScreenCalled = 0;
    this.a11yTouchPassthroughState = false;

    this.registerMethods();
  }

  // Setup method resolvers.
  registerMethods(): void {
    this.methods.register('getConnectedDevices');
    this.methods.register('observeKeyEvents');
    this.methods.register('observeTabletMode');
    this.methods.register('observeLidState');
  }

  getConnectedDevices(): Promise<GetConnectedDevicesResponse> {
    return this.methods.resolveMethod('getConnectedDevices');
  }

  /**
   * Registers an observer for key events on the specific device.
   * @param id The ID of the keyboard to observe
   */
  observeKeyEvents(id: number, remote: KeyboardObserverRemote): void {
    if (!this.keyboardObservers[id]) {
      return;
    }
    this.keyboardObservers[id].push(remote);
  }

  observeInternalDisplayPowerState(
      remote: InternalDisplayPowerStateObserverRemote): void {
    this.internalDisplayPowerStateObserver = remote;
  }

  /**
   * Sets the internal display power state to be on.
   */
  setInternalDisplayPowerOn(): void {
    this.internalDisplayPowerStateObserver.onInternalDisplayPowerStateChanged(
        true);
  }

  /**
   * Sets the internal display power state to be off.
   */
  setInternalDisplayPowerOff(): void {
    this.internalDisplayPowerStateObserver.onInternalDisplayPowerStateChanged(
        false);
  }

  /**
   * Mock registering observer returning isTabletMode as false.
   */
  setStartWithLidClosed(): void {
    this.methods.setResult('observeLidState', {isLidOpen: false});
  }

  /**
   * Mock registering observer returning isTabletMode as true.
   */
  setStartWithLidOpen(): void {
    this.methods.setResult('observeLidState', {isLidOpen: true});
  }

  /**
   * Registers an observer for tablet mode changes and returns current tablet
   * mode.
   */
  observeLidState(remote: LidStateObserverRemote):
      Promise<{isLidOpen: boolean}> {
    this.lidStateObserver = remote;
    return this.methods.resolveMethod('observeLidState');
  }

  setLidStateOpen(): void {
    this.lidStateObserver.onLidStateChanged(true);
  }

  setLidStateClosed(): void {
    this.lidStateObserver.onLidStateChanged(false);
  }

  /**
   * Registers an observer for tablet mode changes and returns current tablet
   * mode.
   */
  observeTabletMode(remote: TabletModeObserverRemote):
      Promise<{isTabletMode: boolean}> {
    this.tabletModeObserver = remote;
    return this.methods.resolveMethod('observeTabletMode');
  }

  /**
   * Mock starting tablet mode.
   */
  startTabletMode(): void {
    this.tabletModeObserver.onTabletModeChanged(true);
  }

  /**
   * Mock ending tablet mode.
   */
  endTabletMode(): void {
    this.tabletModeObserver.onTabletModeChanged(false);
  }

  /**
   * Mock registering observer returning isTabletMode as false.
   */
  setStartTesterWithClamshellMode(): void {
    this.methods.setResult('observeTabletMode', {isTabletMode: false});
  }

  /**
   * Mock registering observer returning isTabletMode as true.
   */
  setStartTesterWithTabletMode(): void {
    this.methods.setResult('observeTabletMode', {isTabletMode: true});
  }

  /**
   * Sets the values that will be returned when calling getConnectedDevices(),
   * but does not notify connected device observers of the changes.
   */
  setFakeConnectedDevices(
      keyboards: KeyboardInfo[], touchDevices: TouchDeviceInfo[]): void {
    this.keyboards = keyboards;
    this.touchDevices = touchDevices;
    this.methods.setResult(
        'getConnectedDevices',
        {keyboards: [...keyboards], touchDevices: [...touchDevices]});
  }

  // Registers an observer for the set of connected devices.
  observeConnectedDevices(remote: ConnectedDevicesObserverRemote): void {
    this.observers.push(remote);
  }

  /**
   * Fakes the connection of a keyboard to the system, notifying observers
   * appropriately.
   */
  addFakeConnectedKeyboard(keyboard: KeyboardInfo): void {
    this.keyboards.push(keyboard);
    this.keyboardObservers[keyboard.id] = [];
    this.methods.setResult('getConnectedDevices', {
      keyboards: [...this.keyboards],
      touchDevices: [...this.touchDevices],
    });

    for (const observer of this.observers) {
      observer.onKeyboardConnected(keyboard);
    }
  }

  /**
   * Fakes the disconnection of a keyboard from the system, notifying observers
   * appropriately.
   * @param id The ID of the keyboard to remove
   */
  removeFakeConnectedKeyboardById(id: number): void {
    this.keyboards = this.keyboards.filter((device) => device.id !== id);
    delete this.keyboardObservers[id];

    for (const observer of this.observers) {
      observer.onKeyboardDisconnected(id);
    }
  }

  /**
   * Fakes the connection of a touch device to the system, notifying observers
   * appropriately.
   */
  addFakeConnectedTouchDevice(touchDevice: TouchDeviceInfo): void {
    this.touchDevices.push(touchDevice);
    this.methods.setResult(
        'getConnectedDevices',
        {keyboards: this.keyboards, touchDevices: this.touchDevices});

    for (const observer of this.observers) {
      observer.onTouchDeviceConnected(touchDevice);
    }
  }

  /**
   * Fakes the disconnection of a touch device from the system, notifying
   * observers appropriately.
   * @param id The ID of the touch device to remove
   */
  removeFakeConnectedTouchDeviceById(id: number): void {
    this.touchDevices = this.touchDevices.filter((device) => device.id !== id);

    for (const observer of this.observers) {
      observer.onTouchDeviceDisconnected(id);
    }
  }

  /**
   * Fakes the function to move the Diagnostics app to the testing touchscreen.
   */
  moveAppToTestingScreen(): void {
    this.moveAppToTestingScreenCalled++;
  }

  /**
   * Returns the number of times moveAppToTestingScreen function is called.
   */
  getMoveAppToTestingScreenCalled(): number {
    return this.moveAppToTestingScreenCalled;
  }

  /**
   * Fakes the function to move the Diagnostics app back to previous screen.
   */
  moveAppBackToPreviousScreen(): void {
    this.moveAppBackToPreviousScreenCalled++;
  }

  /**
   * Returns the number of times moveAppBackToPreviousScreen function is called.
   */
  getMoveAppBackToPreviousScreenCalled(): number {
    return this.moveAppBackToPreviousScreenCalled;
  }

  /*
   * Fake function to enable/disable A11y touch exploration passthough.
   */
  setA11yTouchPassthrough(enabled: boolean): void {
    this.a11yTouchPassthroughState = enabled;
  }

  /**
   * Get the current state of A11y touch passthrough.
   */
  getA11yTouchPassthroughState(): boolean {
    return this.a11yTouchPassthroughState;
  }
}
