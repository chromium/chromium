// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/447427066): Move this into read_anything.mojom once the
// options are finalized and line focus is stored in prefs.
export enum LineFocusType {
  NONE = 0,
  LINE = 1,
  WINDOW = 2,
}

export interface LineFocus {
  type: LineFocusType;
  lines: number;
}
