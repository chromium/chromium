// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {ConnectedDevicesObserverRemote, ConnectionType, GetConnectedDevicesResponse, InputDataProviderInterface, KeyboardInfo, KeyboardObserverRemote, TouchDeviceInfo, TouchDeviceType} from './diagnostics_types.js';

/**
 * @fileoverview
 * Implements a fake version of the InputDataProvider Mojo interface.
 */

/** @implements {InputDataProviderInterface} */
export class FakeInputDataProvider {
  constructor() {
    this.methods_ = new FakeMethodResolver();
    /** @private {!Array<!ConnectedDevicesObserverRemote>} */
    this.observers_ = [];
    /** @private {!Array<!KeyboardInfo>} */
    this.keyboards_ = [];
    /** @private {!Array<!Array<!KeyboardObserverRemote>>} */
    this.keyboard_observers_ = [];
    /** @private {!Array<!TouchDeviceInfo>} */
    this.touchDevices_ = [];

    this.registerMethods();
  }

  /** Resets provider to its internal state. */
  reset() {
    this.methods_ = new FakeMethodResolver();
    this.observers_ = [];
    this.keyboards_ = [];
    this.touchDevices_ = [];

    this.registerMethods();
  }

  /**
   * Setup method resolvers.
   */
  registerMethods() {
    this.methods_.register('getConnectedDevices');
    this.methods_.register('observeKeyEvents');
  }

  /**
   * @return {!Promise<!GetConnectedDevicesResponse>}
   */
  getConnectedDevices() {
    return this.methods_.resolveMethod('getConnectedDevices');
  }

  /**
   * Registers an observer for key events on the specific device.
   * @param {number} id The ID of the keyboard to observe
   * @param {!KeyboardObserverRemote} remote
   */
  observeKeyEvents(id, remote) {
    if (!this.keyboard_observers_[id]) {
      return;
    }
    this.keyboard_observers_[id].push(remote);
  }

  /**
   * Sets the values that will be returned when calling getConnectedDevices(),
   * but does not notify connected device observers of the changes.
   * @param {!Array<!KeyboardInfo>} keyboards
   * @param {!Array<!TouchDeviceInfo>} touchDevices
   */
  setFakeConnectedDevices(keyboards, touchDevices) {
    this.keyboards_ = keyboards;
    this.touchDevices_ = touchDevices;
    this.methods_.setResult('getConnectedDevices',
                            {keyboards: [...keyboards],
                             touchDevices: [...touchDevices]});
  }

  /**
   * Registers an observer for the set of connected devices.
   * @param {!ConnectedDevicesObserverRemote} remote
   */
  observeConnectedDevices(remote) {
    this.observers_.push(remote);
  }

  /**
   * Fakes the connection of a keyboard to the system, notifying observers
   * appropriately.
   * @param {!KeyboardInfo} keyboard
   */
  addFakeConnectedKeyboard(keyboard) {
    this.keyboards_.push(keyboard);
    this.keyboard_observers_[keyboard.id] = [];
    this.methods_.setResult('getConnectedDevices',
                            {keyboards: [...this.keyboards_],
                             touchDevices: [...this.touchDevices_]});

    for (const observer of this.observers_) {
      observer.onKeyboardConnected(keyboard);
    }
  }

  /**
   * Fakes the disconnection of a keyboard from the system, notifying observers
   * appropriately.
   * @param {number} id The ID of the keyboard to remove
   */
  removeFakeConnectedKeyboardById(id) {
    this.keyboards_ = this.keyboards_.filter((device) => device.id !== id);
    delete this.keyboard_observers_[id];

    for (const observer of this.observers_) {
      observer.onKeyboardDisconnected(id);
    }
  }

  /**
   * Fakes the connection of a touch device to the system, notifying observers
   * appropriately.
   * @param {!TouchDeviceInfo} touchDevice
   */
  addFakeConnectedTouchDevice(touchDevice) {
    this.touchDevices_.push(touchDevice);
    this.methods_.setResult('getConnectedDevices',
                            {keyboards: this.keyboards_,
                             touchDevices: this.touchDevices_});

    for (const observer of this.observers_) {
      observer.onTouchDeviceConnected(touchDevice);
    }
  }

  /**
   * Fakes the disconnection of a touch device from the system, notifying
   * observers appropriately.
   * @param {number} id The ID of the touch device to remove
   */
  removeFakeConnectedTouchDeviceById(id) {
    this.touchDevices_ =
      this.touchDevices_.filter((device) => device.id !== id);

    for (const observer of this.observers_) {
      observer.onTouchDeviceDisconnected(id);
    }
  }
}
