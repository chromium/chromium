// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isSeaPenEnabled} from './load_time_booleans.js';

let transitionsEnabled = isSeaPenEnabled();

export function getTransitionEnabled() {
  return transitionsEnabled;
}

export function setTransitionsEnabled(enabled: boolean) {
  transitionsEnabled = enabled;
}

export async function maybeDoPageTransition(func: () => void): Promise<void> {
  if (transitionsEnabled) {
    return document.startViewTransition(func).finished;
  }
  func();
}
