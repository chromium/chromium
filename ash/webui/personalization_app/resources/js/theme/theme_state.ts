// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Stores theme related states.
 */
export interface ThemeState {
  colorModeAutoScheduleEnabled: boolean|null;
  darkModeEnabled: boolean|null;
}

export function emptyState(): ThemeState {
  return {
    colorModeAutoScheduleEnabled: null,
    darkModeEnabled: null,

  };
}
