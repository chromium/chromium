// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Enumerates the user interactions with the Lens Overlay.
//
// This enum must match the numbering for LensOverlayUserAction in enums.xml.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(UserAction)
export enum UserAction {
  REGION_SELECTION = 0,
  REGION_SELECTION_CHANGE = 1,
  TEXT_SELECTION = 2,
  OBJECT_CLICK = 3,
  TRANSLATE_TEXT = 4,
  COPY_TEXT = 5,
  MY_ACTIVITY = 6,
  LEARN_MORE = 7,
  SEND_FEEDBACK = 8,
  MAX_VALUE = SEND_FEEDBACK
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensOverlayUserAction)

export function recordLensOverlayInteraction(interaction: UserAction) {
  chrome.metricsPrivate.recordEnumerationValue(
      'Lens.Overlay.Overlay.UserAction', interaction, UserAction.MAX_VALUE + 1);
}
