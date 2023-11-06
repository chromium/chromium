// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {
  ReactiveController,
  ReactiveControllerHost,
} from 'chrome://resources/mwc/lit/index.js';

import {assert} from '../assert.js';
import * as state from '../state.js';

export class StateObserverController implements ReactiveController {
  private observer: state.StateObserver|null = null;

  value = false;

  constructor(
      private readonly host: ReactiveControllerHost,
      private readonly state: state.StateUnion) {
    this.host.addController(this);
  }

  hostConnected(): void {
    this.observer = (val: boolean) => {
      this.value = val;
      this.host.requestUpdate();
    };
    state.addObserver(this.state, this.observer);
    this.value = state.get(this.state);
    this.host.requestUpdate();
  }

  hostDisconnected(): void {
    assert(this.observer !== null);
    state.removeObserver(this.state, this.observer);
    this.observer = null;
  }
}
