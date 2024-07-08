// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  ComputedImpl,
  Effect,
  EffectCallback,
  SignalImpl,
} from './signal/impl.js';
import {Computed, Dispose, Signal} from './signal/types.js';

export type * from './signal/types.js';

/**
 * Creates a new signal with an initial value.
 */
export function signal<T>(value: T): Signal<T> {
  return new SignalImpl(value);
}

interface ComputedOptions<T> {
  get: () => T;
  set: (val: T) => void;
}

export function computed<T>(get: () => T): Computed<T>;
export function computed<T>(options: ComputedOptions<T>): Signal<T>;

/**
 * Creates a new computed signal with the given option.
 */
export function computed<T>(
  getOrOptions: ComputedOptions<T>|(() => T),
): Computed<T>|Signal<T> {
  if (typeof getOrOptions === 'function') {
    return new ComputedImpl(getOrOptions);
  } else {
    return new ComputedImpl(getOrOptions.get, getOrOptions.set);
  }
}

/**
 * Creates a new effect that would be run when dependent signals are changed.
 *
 * TODO(pihsun): Check if there's any case we need to expose the effect
 * instance.
 * TODO(pihsun): Pass callback to register cleanup functions on effect
 * re-trigger to the callback.
 */
export function effect(callback: EffectCallback): Dispose {
  const effect = new Effect(callback);
  return () => {
    effect.dispose();
  };
}

/**
 * Batches all effects inside the callback to be run after this function
 * returns.
 */
export function batch(callback: () => void): void {
  Effect.batchDepth += 1;
  try {
    callback();
  } finally {
    Effect.batchDepth -= 1;
    Effect.processBatchedEffect();
  }
}
