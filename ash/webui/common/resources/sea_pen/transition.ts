// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let transitionsEnabled = true;

export function getTransitionEnabled() {
  return transitionsEnabled;
}

export function setTransitionsEnabled(enabled: boolean) {
  transitionsEnabled = enabled;
}

/**
 * Wait a small amount before resolving with `value`. This can be used to
 * prevent awkward visual jank when entering a loading state so that the loading
 * state is always shown a minimum amount of time.
 */
export async function withMinimumDelay<T>(value: Promise<T>): Promise<T> {
  if (transitionsEnabled) {
    return new Promise<T>(
        resolve => window.setTimeout(() => resolve(value), 1000));
  }
  return value;
}

export async function maybeDoPageTransition(func: () => void): Promise<void> {
  if (transitionsEnabled) {
    return document.startViewTransition(func).finished;
  }
  func();
}
