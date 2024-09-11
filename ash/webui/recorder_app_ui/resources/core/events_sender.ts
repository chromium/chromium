// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  SpeakerLabelEnableState,
  SummaryEnableState,
  TranscriptionEnableState,
} from './state/settings.js';

export interface StartSessionEventParams {
  speakerLabelEnableState: SpeakerLabelEnableState;
  summaryAvailable: boolean;
  summaryEnableState: SummaryEnableState;
  titleSuggestionAvailable: boolean;
  transcriptionAvailable: boolean;
  transcriptionEnableState: TranscriptionEnableState;
}

export abstract class EventsSender {
  abstract sendStartSessionEvent(params: StartSessionEventParams): void;
}
