// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Stores ambient related states.
 */
export interface AmbientState {
  ambientModeEnabled: boolean;
}

export function emptyState(): AmbientState {
  return {
    ambientModeEnabled: false,
  };
}
