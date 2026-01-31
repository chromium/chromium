// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// LINT.IfChange(VoiceSearchState)

export enum VoiceSearchState {
  VOICE_SEARCH_BUTTON_CLICKED = 0,
  SUCCESSFUL_TRANSCRIPT = 1,
  VOICE_SEARCH_ERROR = 2,
  VOICE_SEARCH_ERROR_AND_CANCELED = 3,
  VOICE_SEARCH_CANCELED = 4,
  MAX_VALUE = VOICE_SEARCH_CANCELED,
}

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_tasks/enums.xml:VoiceSearchState)
