// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains all public type interface of signal.
 *
 * Note that all types are re-exported by signal.ts, and user outside of the
 * signal implementation should import from that file instead.
 */

import {produce} from '../../utils/draft.js';

export abstract class ReadonlySignal<T> {
  /**
   * Gets the value of the signal.
   *
   * When used in computed or effect, the signal will be added as a dependency
   * of the calling computed / effect.
   */
  abstract get value(): T;

  /**
   * Gets the number of children.
   *
   * This is for unit test only.
   */
  abstract numChildrenForTesting(): number;

  /**
   * Gets the value without tracking it as dependency.
   */
  abstract peek(): T;
  // TODO(pihsun): subscribe(), ...
}

export abstract class Signal<T> extends ReadonlySignal<T> {
  /**
   * Sets the value of the signal.
   */
  abstract override set value(newValue: T);

  /**
   * Updates the signal value based on the old value.
   *
   * This is a shortcut of `this.value = updater(this.value);`.
   *
   * Note that the updater should returns a new value without updating the
   * value in place, since current implementation relies on object identity for
   * change detection.
   */
  update(updater: (val: T) => T): void {
    this.value = updater(this.peek());
  }

  /**
   * "Mutates" the signal value.
   *
   * This is a shortcut of `this.value = produce(this.value, recipe);`.
   *
   * Note that the value isn't actually mutated in place but immutably updated
   * via draft.ts.
   */
  mutate(recipe: (draft: T) => void): void {
    this.value = produce(this.peek(), recipe);
  }

  // TODO(pihsun): other needed functions.
}

export type Computed<T> = ReadonlySignal<T>;

export type Dispose = () => void;
