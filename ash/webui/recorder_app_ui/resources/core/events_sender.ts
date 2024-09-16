// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModelResponseError} from './on_device_model/types.js';
import {
  ExportSettings,
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

export interface SummarizeEventParams {
  responseError: ModelResponseError|null;
  wordCount: number;
}

export interface FeedbackEventParams {
  isPositive: boolean;
}

export interface OnboardEventParams {
  speakerLabelEnableState: SpeakerLabelEnableState;
  transcriptionAvailable: boolean;
  transcriptionEnableState: TranscriptionEnableState;
}

export interface ExportEventParams {
  exportSettings: ExportSettings;
  transcriptionAvailable: boolean;
}

interface DurationOnlyPerf {
  kind: 'appStart'|'summaryModelDownload'|'transcriptionModelDownload';
}
interface RecordPerf {
  // Audio duration in milliseconds.
  audioDuration: number;
  kind: 'record';
  wordCount: number;
}
interface ModelProcessPerf {
  kind: 'summary'|'titleSuggestion';
  wordCount: number;
}
interface ExportPerf {
  kind: 'export';
  // Recording size in bytes.
  recordingSize: number;
}

export type PerfEvent = DurationOnlyPerf|ExportPerf|ModelProcessPerf|RecordPerf;

export abstract class EventsSender {
  abstract sendStartSessionEvent(params: StartSessionEventParams): void;
  abstract sendRecordEvent(params: RecordEventParams): void;
  abstract sendSuggestTitleEvent(params: SuggestTitleEventParams): void;
  abstract sendSummarizeEvent(params: SummarizeEventParams): void;
  abstract sendFeedbackTitleSuggestionEvent(params: FeedbackEventParams): void;
  abstract sendFeedbackSummaryEvent(params: FeedbackEventParams): void;
  abstract sendOnboardEvent(params: OnboardEventParams): void;
  abstract sendExportEvent(params: ExportEventParams): void;
  abstract sendPerfEvent(event: PerfEvent, duration: number): void;
}
