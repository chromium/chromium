// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as localStorage from '../utils/local_storage.js';
import {JsonSerializable, Key} from '../utils/local_storage.js';
import {Schema} from '../utils/schema.js';

import {effect, Signal} from './signal.js';

/**
 * Initialize a signal with value from local storage if exist, listens to
 * storage value change and write back to local storage on every signal value
 * change.
 */
export function bindSignal<
  T extends JsonSerializable,
  I extends JsonSerializable,
>(s: Signal<T>, key: Key, schema: Schema<T, I>, defaultValue: T): void {
  s.value = localStorage.get(key, schema, defaultValue);

  /**
   * Whether the window storage event happened in this microtask.
   *
   * This is needed to avoid infinite loop of the storage event triggers
   * another signal change triggers a write to localStorage.
   */
  let storageEventHappened = false;
  window.addEventListener('storage', (ev) => {
    if (storageEventHappened) {
      return;
    }
    // key is null when the whole localStorage is cleared. We also want to
    // re-read the value in that case.
    if (ev.key !== null && ev.key !== key) {
      return;
    }
    storageEventHappened = true;
    s.value = localStorage.get(key, schema, defaultValue);
    queueMicrotask(() => {
      storageEventHappened = false;
    });
  });

  effect(() => {
    // Stops setting localStorage on signal change if the localStorage is
    // changed on the same tick, to prevent infinite recursion.
    if (!storageEventHappened) {
      localStorage.set(key, schema, s.value);
    }
  });
}
