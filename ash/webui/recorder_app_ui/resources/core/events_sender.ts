// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModelResponseError} from './on_device_model/types.js';
import {
  SpeakerLabelEnableState,
  SummaryEnableState,
  TranscriptionEnableState,
  TranscriptionLanguage,
} from './state/settings.js';

export interface StartSessionEventParams {
  speakerLabelEnableState: SpeakerLabelEnableState;
  summaryAvailable: boolean;
  summaryEnableState: SummaryEnableState;
  titleSuggestionAvailable: boolean;
  transcriptionAvailable: boolean;
  transcriptionEnableState: TranscriptionEnableState;
}

export interface RecordEventParams {
  audioDuration: number;
  everMuted: boolean;
  everPaused: boolean;
  includeSystemAudio: boolean;
  isInternalMicrophone: boolean;
  recordDuration: number;
  recordingSaved: boolean;
  speakerCount: number;
  speakerLabelEnableState: SpeakerLabelEnableState;
  transcriptionAvailable: boolean;
  transcriptionEnableState: TranscriptionEnableState;
  transcriptionLocale: TranscriptionLanguage;
  wordCount: number;
}

export interface SuggestTitleEventParams {
  acceptedSuggestionIndex: number;
  suggestionAccepted: boolean;
  responseError: ModelResponseError|null;
  wordCount: number;
}

export abstract class EventsSender {
  abstract sendStartSessionEvent(params: StartSessionEventParams): void;
  abstract sendRecordEvent(params: RecordEventParams): void;
  abstract sendSuggestTitleEvent(params: SuggestTitleEventParams): void;
}
