// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';

import {InputDeviceSettingsProviderInterface, Keyboard, KeyboardObserverInterface, KeyboardSettings, Mouse, MouseObserverInterface, MouseSettings, PointingStick, PointingStickObserverInterface, PointingStickSettings, Touchpad, TouchpadObserverInterface, TouchpadSettings} from './input_device_settings_types.js';

/**
 * @fileoverview
 * Implements a fake version of the FakePerDeviceKeyboardProvider mojo
 * interface.
 */

interface InputDeviceType {
  fakeKeyboards: Keyboard[];
  fakeTouchpads: Touchpad[];
  fakeMice: Mouse[];
  fakePointingSticks: PointingStick[];
}

class FakeMethodState {
  private result = undefined;

  resolveMethod(): Promise<any> {
    const promise = new Promise((resolve) => {
      resolve(this.result);
    });
    return promise;
  }

  getResult(): any {
    return this.result;
  }

  setResult(result: any) {
    this.result = result;
  }
}

/**
 * Manages a map of fake async methods, their resolvers and the fake
 * return value they will produce.
 */
export class FakeMethodResolver {
  private methodMap: Map<string, FakeMethodState> = new Map();

  register(methodName: string): void {
    this.methodMap.set(methodName, new FakeMethodState());
  }

  getResult<K extends keyof InputDeviceType, T>(methodName: K):
      InputDeviceType[K] extends T? InputDeviceType[K]: never {
    return this.getState(methodName).getResult();
  }

  setResult<K extends keyof InputDeviceType, T>(
      methodName: K,
      result: InputDeviceType[K] extends T ? InputDeviceType[K]: never): void {
    this.getState(methodName).setResult(result);
  }

  resolveMethod<T extends keyof InputDeviceType>(methodName: T):
      Promise<InputDeviceType[T]> {
    return this.getState(methodName).resolveMethod();
  }

  getState(methodName: string): FakeMethodState {
    const state = this.methodMap.get(methodName);
    assert(!!state, `Method '${methodName}' not found.`);
    return state;
  }
}

export class FakeInputDeviceSettingsProvider implements
    InputDeviceSettingsProviderInterface {
  private methods: FakeMethodResolver = new FakeMethodResolver();

  constructor() {
    // Setup method resolvers.
    this.methods.register('fakeKeyboards');
    this.methods.register('fakeTouchpads');
    this.methods.register('fakeMice');
    this.methods.register('fakePointingSticks');
  }

  setFakeKeyboards(keyboards: Keyboard[]): void {
    this.methods.setResult('fakeKeyboards', keyboards);
  }

  getConnectedKeyboardSettings(): Promise<Keyboard[]> {
    return this.methods.resolveMethod('fakeKeyboards');
  }

  setFakeTouchpads(touchpads: Touchpad[]): void {
    this.methods.setResult('fakeTouchpads', touchpads);
  }

  getConnectedTouchpadSettings(): Promise<Touchpad[]> {
    return this.methods.resolveMethod('fakeTouchpads');
  }

  setFakeMice(mice: Mouse[]): void {
    this.methods.setResult('fakeMice', mice);
  }

  getConnectedMouseSettings(): Promise<Mouse[]> {
    return this.methods.resolveMethod('fakeMice');
  }

  setFakePointingSticks(pointingSticks: PointingStick[]): void {
    this.methods.setResult('fakePointingSticks', pointingSticks);
  }

  getConnectedPointingStickSettings(): Promise<PointingStick[]> {
    return this.methods.resolveMethod('fakePointingSticks');
  }

  setKeyboardSettings(id: number, settings: KeyboardSettings): void {
    const keyboards = this.methods.getResult('fakeKeyboards');
    for (const keyboard of keyboards) {
      if (keyboard.id === id) {
        keyboard.settings = settings;
      }
    }
    this.methods.setResult('fakeKeyboards', keyboards);
  }

  setMouseSettings(id: number, settings: MouseSettings): void {
    const mice = this.methods.getResult('fakeMice');
    for (const mouse of mice) {
      if (mouse.id === id) {
        mouse.settings = settings;
      }
    }
    this.methods.setResult('fakeMice', mice);
  }

  setTouchpadSettings(id: number, settings: TouchpadSettings): void {
    const touchpads = this.methods.getResult('fakeTouchpads');
    for (const touchpad of touchpads) {
      if (touchpad.id === id) {
        touchpad.settings = settings;
      }
    }
    this.methods.setResult('fakeTouchpads', touchpads);
  }

  setPointingStickSettings(id: number, settings: PointingStickSettings): void {
    const pointingSticks = this.methods.getResult('fakePointingSticks');
    for (const pointingStick of pointingSticks) {
      if (pointingStick.id === id) {
        pointingStick.settings = settings;
      }
    }
    this.methods.setResult('fakePointingSticks', pointingSticks);
  }

  observeKeyboardSettings(_observer: KeyboardObserverInterface): void {
    // TODO(yyhyyh): Implement observeKeyboardSettings().
  }

  observeTouchpadSettings(_observer: TouchpadObserverInterface): void {
    // TODO(yyhyyh): Implement observeTouchpadSettings().
  }

  observeMouseSettings(_observer: MouseObserverInterface): void {
    // TODO(yyhyyh): Implement observeMouseSettings().
  }

  observePointingStickSettings(_observer: PointingStickObserverInterface):
      void {
    // TODO(yyhyyh): Implement observePointingStickSettings().
  }
}
