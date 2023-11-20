// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeObservables} from '../fake_observables.js';

import {KeyEvent} from './input_device_settings.mojom-webui.js';
import {ShortcutInputObserverInterface, ShortcutInputProviderInterface} from './shortcut_input_provider.mojom-webui.js';

export class FakeShortcutInputProvider implements
    ShortcutInputProviderInterface {
  private observables: FakeObservables = new FakeObservables();
  // TODO(jimmyxgong): Remove this when prerewrittenKeyEvent is displayed as
  // an element in `shortcut_input.html`.
  private prerewrittenKeyEvent: KeyEvent;

  constructor() {
    this.registerObservables();
  }

  getPrerewrittenKeyEvent(): KeyEvent {
    return this.prerewrittenKeyEvent;
  }

  sendKeyPressEvent(prerewrittenKeyEvent: KeyEvent, keyEvent: KeyEvent) {
    this.observables.setObservableData(
        'ShortcutInputObserver_OnShortcutInputEventPressed',
        [{prerewrittenKeyEvent, keyEvent}]);
    this.observables.trigger(
        'ShortcutInputObserver_OnShortcutInputEventPressed');
  }

  sendKeyReleaseEvent(prerewrittenKeyEvent: KeyEvent, keyEvent: KeyEvent) {
    this.observables.setObservableData(
        'ShortcutInputObserver_OnShortcutInputEventReleased',
        [{prerewrittenKeyEvent, keyEvent}]);
    this.observables.trigger(
        'ShortcutInputObserver_OnShortcutInputEventReleased');
  }

  startObservingShortcutInput(observer: ShortcutInputObserverInterface) {
    this.observables.observe(
        'ShortcutInputObserver_OnShortcutInputEventPressed',
        (detail: {prerewrittenKeyEvent: KeyEvent, keyEvent: KeyEvent}) => {
          observer.onShortcutInputEventPressed(
              detail.prerewrittenKeyEvent, detail.keyEvent);
          this.prerewrittenKeyEvent = detail.prerewrittenKeyEvent;
        });
    this.observables.observe(
        'ShortcutInputObserver_OnShortcutInputEventReleased',
        (detail: {prerewrittenKeyEvent: KeyEvent, keyEvent: KeyEvent}) => {
          observer.onShortcutInputEventReleased(
              detail.prerewrittenKeyEvent, detail.keyEvent);
          this.prerewrittenKeyEvent = detail.prerewrittenKeyEvent;
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
