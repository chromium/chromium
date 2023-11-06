// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeObservables} from '../fake_observables.js';

import {KeyEvent} from './input_device_settings.mojom-webui.js';
import {ShortcutInputObserverInterface, ShortcutInputProviderInterface} from './shortcut_input_provider.mojom-webui.js';

export class FakeShortcutInputProvider implements
    ShortcutInputProviderInterface {
  private observables: FakeObservables = new FakeObservables();

  constructor() {
    this.registerObservables();
  }

  sendKeyPressEvent(keyEvent: KeyEvent) {
    this.observables.setObservableData(
        'ShortcutInputObserver_OnShortcutInputEventPressed', [keyEvent]);
    this.observables.trigger(
        'ShortcutInputObserver_OnShortcutInputEventPressed');
  }

  sendKeyReleaseEvent(keyEvent: KeyEvent) {
    this.observables.setObservableData(
        'ShortcutInputObserver_OnShortcutInputEventReleased', [keyEvent]);
    this.observables.trigger(
        'ShortcutInputObserver_OnShortcutInputEventReleased');
  }

  startObservingShortcutInput(observer: ShortcutInputObserverInterface) {
    this.observables.observe(
        'ShortcutInputObserver_OnShortcutInputEventPressed',
        (keyEvent: KeyEvent) => {
          observer.onShortcutInputEventPressed(keyEvent);
        });
    this.observables.observe(
        'ShortcutInputObserver_OnShortcutInputEventReleased',
        (keyEvent: KeyEvent) => {
          observer.onShortcutInputEventReleased(keyEvent);
        });
  }

  stopObservingShortcutInput() {
    this.observables = new FakeObservables();
    this.registerObservables();
  }

  private registerObservables() {
    this.observables.register(
        'ShortcutInputObserver_OnShortcutInputEventPressed');
    this.observables.register(
        'ShortcutInputObserver_OnShortcutInputEventReleased');
  }
}
