// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {GetConnectedDevicesResponse} from './diagnostics_types.js';
import {ConnectedDevicesObserverRemote, InputDataProviderInterface, InternalDisplayPowerStateObserverRemote, KeyboardInfo, KeyboardObserverRemote, LidStateObserverRemote, TabletModeObserverRemote, TouchDeviceInfo} from './input_data_provider.mojom-webui.js';

/**
 * @fileoverview
 * Implements a fake version of the InputDataProvider Mojo interface.
 */

export class FakeInputDataProvider implements InputDataProviderInterface {
  private methods_: FakeMethodResolver = new FakeMethodResolver();
  private observers_: ConnectedDevicesObserverRemote[] = [];
  private keyboards_: KeyboardInfo[] = [];
  private keyboardObservers_: KeyboardObserverRemote[][] = [];
  private tabletModeObserver_: TabletModeObserverRemote;
  private lidStateObserver_: LidStateObserverRemote;
  private internalDisplayPowerStateObserver_:
      InternalDisplayPowerStateObserverRemote;
  private touchDevices_: TouchDeviceInfo[] = [];
  private moveAppToTestingScreenCalled: number = 0;
  private moveAppBackToPreviousScreenCalled: number = 0;
  private a11yTouchPassthroughState: boolean = false;
  constructor() {
    this.registerMethods();
  }

  // Resets provider to its internal state.
  reset(): void {
    this.methods_ = new FakeMethodResolver();
    this.observers_ = [];
    this.keyboards_ = [];
    this.touchDevices_ = [];
    this.moveAppToTestingScreenCalled = 0;
    this.moveAppBackToPreviousScreenCalled = 0;
    this.a11yTouchPassthroughState = false;

    this.registerMethods();
  }

  // Setup method resolvers.
  registerMethods(): void {
    this.methods_.register('getConnectedDevices');
    this.methods_.register('observeKeyEvents');
    this.methods_.register('observeTabletMode');
    this.methods_.register('observeLidState');
  }

  getConnectedDevices(): Promise<GetConnectedDevicesResponse> {
    return this.methods_.resolveMethod('getConnectedDevices');
  }

  /**
   * Registers an observer for key events on the specific device.
   * @param id The ID of the keyboard to observe
   */
  observeKeyEvents(id: number, remote: KeyboardObserverRemote): void {
    if (!this.keyboardObservers_[id]) {
      return;
    }
    this.keyboardObservers_[id].push(remote);
  }

  observeInternalDisplayPowerState(
      remote: InternalDisplayPowerStateObserverRemote): void {
    this.internalDisplayPowerStateObserver_ = remote;
  }

  /**
   * Sets the internal display power state to be on.
   */
  setInternalDisplayPowerOn(): void {
    this.internalDisplayPowerStateObserver_.onInternalDisplayPowerStateChanged(
        true);
  }

  /**
   * Sets the internal display power state to be off.
   */
  setInternalDisplayPowerOff(): void {
    this.internalDisplayPowerStateObserver_.onInternalDisplayPowerStateChanged(
        false);
  }

  /**
   * Mock registering observer returning isTabletMode as false.
   */
  setStartWithLidClosed(): void {
    this.methods_.setResult('observeLidState', {isLidOpen: false});
  }

  /**
   * Mock registering observer returning isTabletMode as true.
   */
  setStartWithLidOpen(): void {
    this.methods_.setResult('observeLidState', {isLidOpen: true});
  }

  /**
   * Registers an observer for tablet mode changes and returns current tablet
   * mode.
   */
  observeLidState(remote: LidStateObserverRemote):
      Promise<{isLidOpen: boolean}> {
    this.lidStateObserver_ = remote;
    return this.methods_.resolveMethod('observeLidState');
  }

  setLidStateOpen(): void {
    this.lidStateObserver_.onLidStateChanged(true);
  }

  setLidStateClosed(): void {
    this.lidStateObserver_.onLidStateChanged(false);
  }

  /**
   * Registers an observer for tablet mode changes and returns current tablet
   * mode.
   */
  observeTabletMode(remote: TabletModeObserverRemote):
      Promise<{isTabletMode: boolean}> {
    this.tabletModeObserver_ = remote;
    return this.methods_.resolveMethod('observeTabletMode');
  }

  /**
   * Mock starting tablet mode.
   */
  startTabletMode(): void {
    this.tabletModeObserver_.onTabletModeChanged(true);
  }

  /**
   * Mock ending tablet mode.
   */
  endTabletMode(): void {
    this.tabletModeObserver_.onTabletModeChanged(false);
  }

  /**
   * Mock registering observer returning isTabletMode as false.
   */
  setStartTesterWithClamshellMode(): void {
    this.methods_.setResult('observeTabletMode', {isTabletMode: false});
  }

  /**
   * Mock registering observer returning isTabletMode as true.
   */
  setStartTesterWithTabletMode(): void {
    this.methods_.setResult('observeTabletMode', {isTabletMode: true});
  }

  /**
   * Sets the values that will be returned when calling getConnectedDevices(),
   * but does not notify connected device observers of the changes.
   */
  setFakeConnectedDevices(
      keyboards: KeyboardInfo[], touchDevices: TouchDeviceInfo[]): void {
    this.keyboards_ = keyboards;
    this.touchDevices_ = touchDevices;
    this.methods_.setResult(
        'getConnectedDevices',
        {keyboards: [...keyboards], touchDevices: [...touchDevices]});
  }

  // Registers an observer for the set of connected devices.
  observeConnectedDevices(remote: ConnectedDevicesObserverRemote): void {
    this.observers_.push(remote);
  }

  /**
   * Fakes the connection of a keyboard to the system, notifying observers
   * appropriately.
   */
  addFakeConnectedKeyboard(keyboard: KeyboardInfo): void {
    this.keyboards_.push(keyboard);
    this.keyboardObservers_[keyboard.id] = [];
    this.methods_.setResult('getConnectedDevices', {
      keyboards: [...this.keyboards_],
      touchDevices: [...this.touchDevices_],
    });

    for (const observer of this.observers_) {
      observer.onKeyboardConnected(keyboard);
    }
  }

  /**
   * Fakes the disconnection of a keyboard from the system, notifying observers
   * appropriately.
   * @param id The ID of the keyboard to remove
   */
  removeFakeConnectedKeyboardById(id: number): void {
    this.keyboards_ = this.keyboards_.filter((device) => device.id !== id);
    delete this.keyboardObservers_[id];

    for (const observer of this.observers_) {
      observer.onKeyboardDisconnected(id);
    }
  }

  /**
   * Fakes the connection of a touch device to the system, notifying observers
   * appropriately.
   */
  addFakeConnectedTouchDevice(touchDevice: TouchDeviceInfo): void {
    this.touchDevices_.push(touchDevice);
    this.methods_.setResult(
        'getConnectedDevices',
        {keyboards: this.keyboards_, touchDevices: this.touchDevices_});

    for (const observer of this.observers_) {
      observer.onTouchDeviceConnected(touchDevice);
    }
  }

  /**
   * Fakes the disconnection of a touch device from the system, notifying
   * observers appropriately.
   * @param id The ID of the touch device to remove
   */
  removeFakeConnectedTouchDeviceById(id: number): void {
    this.touchDevices_ =
        this.touchDevices_.filter((device) => device.id !== id);

    for (const observer of this.observers_) {
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
