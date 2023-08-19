// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const overridableKey = Symbol();

const overrideMap = new Map<unknown, unknown>();

// The type is only used for differentiating the return type of
// overridableFunction, so setOverride is always called with something that is
// valid.
type Overridable<T> = T&{[overridableKey]: true};

/**
 * Marks a function as overridable by local dev with `setOverride`.
 */
export function overridableFunction<Args extends unknown[], Ret>(
    func: (...args: Args) => Ret): Overridable<(...args: Args) => Ret> {
  return Object.assign(function wrapper(...args: Args): Ret {
    if (overrideMap.has(wrapper)) {
      // We guarantee that the overrideMap stores the same type as the original
      // function by the setOverride.
      // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
      return (overrideMap.get(wrapper) as (...args: Args) => Ret)(...args);
    }
    return func(...args);
  }, {[overridableKey]: true} as const);
}

/**
 * Override a function previously marked as `overridableFunction`.
 */
export function setOverride<T>(original: Overridable<T>, target: T): void {
  overrideMap.set(original, target);
}
