// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';

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
  private keyboardObservers: KeyboardObserverInterface[] = [];
  private pointingStickObservers: PointingStickObserverInterface[] = [];
  private mouseObservers: MouseObserverInterface[] = [];
  private touchpadObservers: TouchpadObserverInterface[] = [];

  constructor() {
    // Setup method resolvers.
    this.methods.register('fakeKeyboards');
    this.methods.register('fakeTouchpads');
    this.methods.register('fakeMice');
    this.methods.register('fakePointingSticks');
  }

  setFakeKeyboards(keyboards: Keyboard[]): void {
    this.methods.setResult('fakeKeyboards', keyboards);
    this.notifyKeboardListUpdated();
  }

  async getConnectedKeyboards(): Promise<{keyboards: Keyboard[]}> {
    // TODO(wangdanny): Remove this function once https://crrev.com/c/4337720
    // is submitted.
    assertNotReached();
  }

  getConnectedKeyboardSettings(): Promise<Keyboard[]> {
    return this.methods.resolveMethod('fakeKeyboards');
  }

  setFakeTouchpads(touchpads: Touchpad[]): void {
    this.methods.setResult('fakeTouchpads', touchpads);
    this.notifyTouchpadListUpdated();
  }

  getConnectedTouchpadSettings(): Promise<Touchpad[]> {
    return this.methods.resolveMethod('fakeTouchpads');
  }

  setFakeMice(mice: Mouse[]): void {
    this.methods.setResult('fakeMice', mice);
    this.notifyMouseListUpdated();
  }

  getConnectedMouseSettings(): Promise<Mouse[]> {
    return this.methods.resolveMethod('fakeMice');
  }

  setFakePointingSticks(pointingSticks: PointingStick[]): void {
    this.methods.setResult('fakePointingSticks', pointingSticks);
    this.notifyPointingStickListUpdated();
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

  notifyKeboardListUpdated(): void {
    const keyboards = this.methods.getResult('fakeKeyboards');
    for (const observer of this.keyboardObservers) {
      observer.onKeyboardListUpdated(keyboards);
    }
  }

  notifyTouchpadListUpdated(): void {
    const touchpads = this.methods.getResult('fakeTouchpads');
    for (const observer of this.touchpadObservers) {
      observer.onTouchpadListUpdated(touchpads);
    }
  }

  notifyMouseListUpdated(): void {
    const mice = this.methods.getResult('fakeMice');
    for (const observer of this.mouseObservers) {
      observer.onMouseListUpdated(mice);
    }
  }

  notifyPointingStickListUpdated(): void {
    const pointingSticks = this.methods.getResult('fakePointingSticks');
    for (const observer of this.pointingStickObservers) {
      observer.onPointingStickListUpdated(pointingSticks);
    }
  }

  observeKeyboardSettings(observer: KeyboardObserverInterface): void {
    this.keyboardObservers.push(observer);
    this.notifyKeboardListUpdated();
  }

  observeTouchpadSettings(observer: TouchpadObserverInterface): void {
    this.touchpadObservers.push(observer);
    this.notifyTouchpadListUpdated();
  }

  observeMouseSettings(observer: MouseObserverInterface): void {
    this.mouseObservers.push(observer);
    this.notifyMouseListUpdated();
  }

  observePointingStickSettings(observer: PointingStickObserverInterface): void {
    this.pointingStickObservers.push(observer);
    this.notifyPointingStickListUpdated();
  }
}