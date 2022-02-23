// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Stores theme related states.
 */
export interface ThemeState {
  darkModeEnabled: boolean;
}

export function emptyState(): ThemeState {
  return {
    darkModeEnabled: false,
  };
}
