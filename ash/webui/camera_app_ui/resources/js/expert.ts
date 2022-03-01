// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as localStorage from './models/local_storage.js';
import * as state from './state.js';

/**
 * Enables or disables expert mode.
 *
 * @param enable Whether to enable or disable expert mode.
 */
export function setExpertMode(enable: boolean): void {
  state.set(state.State.EXPERT, enable);
  localStorage.set('expert', enable);
}

/**
 * Toggles expert mode.
 */
export function toggleExpertMode(): void {
  const newState = !state.get(state.State.EXPERT);
  setExpertMode(newState);
}
