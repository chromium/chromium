// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  // Events
  CrOSEvents_RecorderApp_Record,
  CrOSEvents_RecorderApp_StartSession,
  // Enums
  CrOSEvents_RecorderAppMicrophoneType,
  CrOSEvents_RecorderAppSpeakerLabelEnableState,
  CrOSEvents_RecorderAppSummaryEnableState,
  CrOSEvents_RecorderAppTranscriptionEnableState,
  CrOSEvents_RecorderAppTranscriptionLocale,
} from 'chrome://resources/ash/common/metrics/structured_events.js';
import {
  record,
} from 'chrome://resources/ash/common/metrics/structured_metrics_service.js';

import {
  EventsSender as EventsSenderBase,
  RecordEventParams,
  StartSessionEventParams,
} from '../../core/events_sender.js';
import {
  SpeakerLabelEnableState,
  SummaryEnableState,
  TranscriptionEnableState,
  TranscriptionLanguage,
} from '../../core/state/settings.js';
import {assertExhaustive} from '../../core/utils/assert.js';

function getSpeakerLabelEnableState(
  transcriptionAvailable: boolean,
  speakerLabelState: SpeakerLabelEnableState,
): CrOSEvents_RecorderAppSpeakerLabelEnableState {
  if (!transcriptionAvailable) {
    return CrOSEvents_RecorderAppSpeakerLabelEnableState.NOT_AVAILABLE;
  }

  switch (speakerLabelState) {
    case SpeakerLabelEnableState.DISABLED:
      return CrOSEvents_RecorderAppSpeakerLabelEnableState.DISABLED;
    case SpeakerLabelEnableState.DISABLED_FIRST:
      return CrOSEvents_RecorderAppSpeakerLabelEnableState.DISABLED_FIRST;
    case SpeakerLabelEnableState.ENABLED:
      return CrOSEvents_RecorderAppSpeakerLabelEnableState.ENABLED;
    case SpeakerLabelEnableState.UNKNOWN:
      return CrOSEvents_RecorderAppSpeakerLabelEnableState.UNKNOWN;
    default:
      assertExhaustive(speakerLabelState);
  }
}

function getSummaryEnableState(
  featuresAvailable: boolean,
  state: SummaryEnableState,
): CrOSEvents_RecorderAppSummaryEnableState {
  if (!featuresAvailable) {
    return CrOSEvents_RecorderAppSummaryEnableState.NOT_AVAILABLE;
  }

  switch (state) {
    case SummaryEnableState.DISABLED:
      return CrOSEvents_RecorderAppSummaryEnableState.DISABLED;
    case SummaryEnableState.ENABLED:
      return CrOSEvents_RecorderAppSummaryEnableState.ENABLED;
    case SummaryEnableState.UNKNOWN:
      return CrOSEvents_RecorderAppSummaryEnableState.UNKNOWN;
    default:
      assertExhaustive(state);
  }
}

function getTranscriptionEnableState(
  transcriptionEnabled: boolean,
  state: TranscriptionEnableState,
): CrOSEvents_RecorderAppTranscriptionEnableState {
  if (!transcriptionEnabled) {
    return CrOSEvents_RecorderAppTranscriptionEnableState.NOT_AVAILABLE;
  }

  switch (state) {
    case TranscriptionEnableState.DISABLED:
      return CrOSEvents_RecorderAppTranscriptionEnableState.DISABLED;
    case TranscriptionEnableState.DISABLED_FIRST:
      return CrOSEvents_RecorderAppTranscriptionEnableState.DISABLED_FIRST;
    case TranscriptionEnableState.ENABLED:
      return CrOSEvents_RecorderAppTranscriptionEnableState.ENABLED;
    case TranscriptionEnableState.UNKNOWN:
      return CrOSEvents_RecorderAppTranscriptionEnableState.UNKNOWN;
    default:
      assertExhaustive(state);
  }
}

function convertToMicrophoneType(isInternalMicrophone: boolean
): CrOSEvents_RecorderAppMicrophoneType {
  return isInternalMicrophone ? CrOSEvents_RecorderAppMicrophoneType.INTERNAL :
                                CrOSEvents_RecorderAppMicrophoneType.EXTERNAL;
}

function convertTranscriptionLocaleType(language: TranscriptionLanguage
): CrOSEvents_RecorderAppTranscriptionLocale {
  switch (language) {
    case TranscriptionLanguage.NONE:
      return CrOSEvents_RecorderAppTranscriptionLocale.NONE;
    case TranscriptionLanguage.EN_US:
      return CrOSEvents_RecorderAppTranscriptionLocale.EN_US;
    default:
      assertExhaustive(language);
  }
}

export class EventsSender extends EventsSenderBase {
  override sendStartSessionEvent({
    speakerLabelEnableState,
    summaryAvailable,
    summaryEnableState,
    titleSuggestionAvailable,
    transcriptionAvailable,
    transcriptionEnableState,
  }: StartSessionEventParams): void {
    const speakerLabel = getSpeakerLabelEnableState(
      transcriptionAvailable,
      speakerLabelEnableState,
    );

    const summary = getSummaryEnableState(
      summaryAvailable && titleSuggestionAvailable,
      summaryEnableState,
    );

    const transcription = getTranscriptionEnableState(
      transcriptionAvailable,
      transcriptionEnableState,
    );

    const event = new CrOSEvents_RecorderApp_StartSession()
                    .setSpeakerLabelEnableState(speakerLabel)
                    .setSummaryEnableState(summary)
                    .setTranscriptionEnableState(transcription)
                    .build();

    record(event);
  }

  override sendRecordEvent(params: RecordEventParams): void {
    const mic = convertToMicrophoneType(params.isInternalMicrophone);
    const locale = convertTranscriptionLocaleType(params.transcriptionLocale);
    const speakerLabel = getSpeakerLabelEnableState(
      params.transcriptionAvailable, params.speakerLabelEnableState
    );
    const transcription = getTranscriptionEnableState(
      params.transcriptionAvailable, params.transcriptionEnableState
    );

    const event = new CrOSEvents_RecorderApp_Record()
                    .setAudioDuration(BigInt(params.audioDuration))
                    .setEverMuted(BigInt(params.everMuted))
                    .setEverPaused(BigInt(params.everPaused))
                    .setIncludeSystemAudio(BigInt(params.includeSystemAudio))
                    .setMicrophoneType(mic)
                    .setRecordDuration(BigInt(params.recordDuration))
                    .setRecordingSaved(BigInt(params.recordingSaved))
                    .setSpeakerCount(BigInt(params.speakerCount))
                    .setSpeakerLabelEnableState(speakerLabel)
                    .setTranscriptionLabelEnableState(transcription)
                    .setTranscriptionLocale(locale)
                    .setWordCount(BigInt(params.wordCount))
                    .build();

    record(event);
  }
}
