// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';

import {InputDeviceSettingsProviderInterface, Keyboard, KeyboardsObserverInterface} from './input_device_settings_types.js';

/**
 * @fileoverview
 * Implements a fake version of the FakePerDeviceKeyboardProvider mojo
 * interface.
 */

class FakeMethodState {
  private result = undefined;

  resolveMethod(): Promise<any> {
    const promise = new Promise((resolve) => {
      resolve(this.result);
    });
    return promise;
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

  setResult(methodName: string, result: Keyboard[]): void {
    this.getState(methodName).setResult(result);
  }

  resolveMethod(methodName: string): Promise<Keyboard[]> {
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
  }

  setFakeKeyboards(keyboards: Keyboard[]): void {
    this.methods.setResult('fakeKeyboards', keyboards);
  }

  getFakeKeyboards(): Promise<Keyboard[]> {
    return this.methods.resolveMethod('fakeKeyboards');
  }

  observeKeyboardSettings(_observer: KeyboardsObserverInterface): void {
    // TODO(yyhyyh): Implement observeKeyboardSettings().
  }
}
