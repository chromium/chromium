// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {GetConnectedDevicesResponse} from './diagnostics_types.js';
import {ConnectedDevicesObserverRemote, InputDataProviderInterface, KeyboardInfo, KeyboardObserverRemote, TouchDeviceInfo} from './input_data_provider.mojom-webui.js';

/**
 * @fileoverview
 * Implements a fake version of the InputDataProvider Mojo interface.
 */

export class FakeInputDataProvider implements InputDataProviderInterface {
  private methods_: FakeMethodResolver = new FakeMethodResolver();
  private observers_: ConnectedDevicesObserverRemote[] = [];
  private keyboards_: KeyboardInfo[] = [];
  private keyboardObservers_: KeyboardObserverRemote[][] = [];
  private touchDevices_: TouchDeviceInfo[] = [];
  constructor() {
    this.registerMethods();
  }

  // Resets provider to its internal state.
  reset(): void {
    this.methods_ = new FakeMethodResolver();
    this.observers_ = [];
    this.keyboards_ = [];
    this.touchDevices_ = [];

    this.registerMethods();
  }

  // Setup method resolvers.
  registerMethods(): void {
    this.methods_.register('getConnectedDevices');
    this.methods_.register('observeKeyEvents');
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
}
