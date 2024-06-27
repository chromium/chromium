// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {computed, Signal} from './signal.js';

/**
 * Derive a signal from the sub-object of the original signal.
 */
export function signalSlice<T, K extends keyof T>(
  signal: Signal<T>,
  key: K,
): Signal<T[K]> {
  return computed({
    get: (): T[K] => signal.value[key],
    set: (val: T[K]) => {
      signal.update((s) => ({
                      ...s,
                      [key]: val,
                    }));
    },
  });
}
