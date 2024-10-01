// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  // Events
  CrOSEvents_RecorderApp_AppStartPerf,
  CrOSEvents_RecorderApp_Export,
  CrOSEvents_RecorderApp_ExportPerf,
  CrOSEvents_RecorderApp_FeedbackSummary,
  CrOSEvents_RecorderApp_FeedbackTitleSuggestion,
  CrOSEvents_RecorderApp_Onboard,
  CrOSEvents_RecorderApp_Record,
  CrOSEvents_RecorderApp_RecordingSavingPerf,
  CrOSEvents_RecorderApp_StartSession,
  CrOSEvents_RecorderApp_SuggestTitle,
  CrOSEvents_RecorderApp_Summarize,
  CrOSEvents_RecorderApp_SummaryModelDownloadPerf,
  CrOSEvents_RecorderApp_SummaryPerf,
  CrOSEvents_RecorderApp_TitleSuggestionPerf,
  CrOSEvents_RecorderApp_TranscriptionModelDownloadPerf,
  // Enums
  CrOSEvents_RecorderAppAudioFormat,
  CrOSEvents_RecorderAppMicrophoneType,
  CrOSEvents_RecorderAppModelFeedback,
  CrOSEvents_RecorderAppModelResultStatus,
  CrOSEvents_RecorderAppSpeakerLabelEnableState,
  CrOSEvents_RecorderAppSummaryEnableState,
  CrOSEvents_RecorderAppTranscriptFormat,
  CrOSEvents_RecorderAppTranscriptionEnableState,
  CrOSEvents_RecorderAppTranscriptionLocale,
} from 'chrome://resources/ash/common/metrics/structured_events.js';
import {
  record,
} from 'chrome://resources/ash/common/metrics/structured_metrics_service.js';

import {
  EventsSender as EventsSenderBase,
  ExportEventParams,
  FeedbackEventParams,
  OnboardEventParams,
  PerfEvent,
  RecordEventParams,
  StartSessionEventParams,
  SuggestTitleEventParams,
  SummarizeEventParams,
} from '../../core/events_sender.js';
import {ModelResponseError} from '../../core/on_device_model/types.js';
import {
  ExportAudioFormat,
  ExportSettings,
  ExportTranscriptionFormat,
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

function convertToMicrophoneType(
  isInternalMicrophone: boolean,
): CrOSEvents_RecorderAppMicrophoneType {
  return isInternalMicrophone ? CrOSEvents_RecorderAppMicrophoneType.INTERNAL :
                                CrOSEvents_RecorderAppMicrophoneType.EXTERNAL;
}

function convertTranscriptionLocaleType(
  language: TranscriptionLanguage,
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

function convertToModelResultStatus(
  responseError: ModelResponseError|null,
): CrOSEvents_RecorderAppModelResultStatus {
  if (responseError === null) {
    return CrOSEvents_RecorderAppModelResultStatus.SUCCESS;
  }

  const {
    UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG,
    UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT,
  } = CrOSEvents_RecorderAppModelResultStatus;

  switch (responseError) {
    case ModelResponseError.GENERAL:
      return CrOSEvents_RecorderAppModelResultStatus.GENERAL_ERROR;
    case ModelResponseError.UNSAFE:
      return CrOSEvents_RecorderAppModelResultStatus.UNSAFE;
    case ModelResponseError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT:
      return UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT;
    case ModelResponseError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG:
      return UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG;
    default:
      assertExhaustive(responseError);
  }
}

function convertToAudioFormat(
  settings: ExportSettings,
): CrOSEvents_RecorderAppAudioFormat {
  if (!settings.audio) {
    return CrOSEvents_RecorderAppAudioFormat.NOT_EXPORTED;
  }

  switch (settings.audioFormat) {
    case ExportAudioFormat.WEBM_ORIGINAL:
      return CrOSEvents_RecorderAppAudioFormat.WEBM_ORIGINAL;
    default:
      assertExhaustive(settings.audioFormat);
  }
}

function convertToTranscriptFormat(
  settings: ExportSettings,
  transcriptionAvailable: boolean,
): CrOSEvents_RecorderAppTranscriptFormat {
  if (!transcriptionAvailable) {
    return CrOSEvents_RecorderAppTranscriptFormat.NOT_AVAILABLE;
  } else if (!settings.transcription) {
    return CrOSEvents_RecorderAppTranscriptFormat.NOT_EXPORTED;
  }

  switch (settings.transcriptionFormat) {
    case ExportTranscriptionFormat.TXT:
      return CrOSEvents_RecorderAppTranscriptFormat.TXT;
    default:
      assertExhaustive(settings.transcriptionFormat);
  }
}

function convertToModelFeedback(
  isPositive: boolean,
): CrOSEvents_RecorderAppModelFeedback {
  return isPositive ? CrOSEvents_RecorderAppModelFeedback.POSITIVE :
                      CrOSEvents_RecorderAppModelFeedback.NEGATIVE;
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
      params.transcriptionAvailable,
      params.speakerLabelEnableState,
    );
    const transcription = getTranscriptionEnableState(
      params.transcriptionAvailable,
      params.transcriptionEnableState,
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

  override sendSuggestTitleEvent(params: SuggestTitleEventParams): void {
    const status = convertToModelResultStatus(params.responseError);
    const event =
      new CrOSEvents_RecorderApp_SuggestTitle()
        .setAcceptedSuggestionIndex(BigInt(params.acceptedSuggestionIndex))
        .setSuggestionAccepted(BigInt(params.suggestionAccepted))
        .setResultStatus(status)
        .setWordCount(BigInt(params.wordCount))
        .build();

    record(event);
  }

  override sendSummarizeEvent(params: SummarizeEventParams): void {
    const event =
      new CrOSEvents_RecorderApp_Summarize()
        .setResultStatus(convertToModelResultStatus(params.responseError))
        .setWordCount(BigInt(params.wordCount))
        .build();

    record(event);
  }

  override sendFeedbackTitleSuggestionEvent({
    isPositive,
  }: FeedbackEventParams): void {
    const event = new CrOSEvents_RecorderApp_FeedbackTitleSuggestion()
                    .setFeedback(convertToModelFeedback(isPositive))
                    .build();

    record(event);
  }

  override sendFeedbackSummaryEvent({isPositive}: FeedbackEventParams): void {
    const event = new CrOSEvents_RecorderApp_FeedbackSummary()
                    .setFeedback(convertToModelFeedback(isPositive))
                    .build();

    record(event);
  }

  override sendOnboardEvent(params: OnboardEventParams): void {
    const speakerLabel = getSpeakerLabelEnableState(
      params.transcriptionAvailable,
      params.speakerLabelEnableState,
    );
    const transcription = getTranscriptionEnableState(
      params.transcriptionAvailable,
      params.transcriptionEnableState,
    );

    const event = new CrOSEvents_RecorderApp_Onboard()
                    .setSpeakerLabelEnableState(speakerLabel)
                    .setTranscriptionEnableState(transcription)
                    .build();

    record(event);
  }

  override sendExportEvent(params: ExportEventParams): void {
    const {exportSettings, transcriptionAvailable} = params;
    const audioFormat = convertToAudioFormat(exportSettings);
    const transcriptFormat = convertToTranscriptFormat(
      exportSettings,
      transcriptionAvailable,
    );

    const event = new CrOSEvents_RecorderApp_Export()
                    .setAudioFormat(audioFormat)
                    .setTranscriptFormat(transcriptFormat)
                    .build();

    record(event);
  }

  override sendPerfEvent(event: PerfEvent, duration: number): void {
    const {kind} = event;
    switch (kind) {
      case 'appStart':
        return this.sendAppStartPerf(duration);
      case 'export':
        return this.sendExportPerf(duration, event.recordingSize);
      case 'record':
        return this.sendRecordingSavingPerf(
          duration,
          event.audioDuration,
          event.wordCount,
        );
      case 'summary':
        return this.sendSummaryPerf(duration, event.wordCount);
      case 'summaryModelDownload':
        return this.sendSummaryModelDownloadPerf(duration);
      case 'titleSuggestion':
        return this.sendTitleSuggestionPerf(duration, event.wordCount);
      case 'transcriptionModelDownload':
        // TODO: b/327538356 - Collect soda download perf.
        return this.sendTranscriptionModelDownloadPerf(duration);
      default:
        assertExhaustive(kind);
    }
  }

  private sendAppStartPerf(duration: number): void {
    const dur = BigInt(duration);
    const event =
      new CrOSEvents_RecorderApp_AppStartPerf().setDuration(dur).build();

    record(event);
  }

  private sendTranscriptionModelDownloadPerf(duration: number): void {
    const event = new CrOSEvents_RecorderApp_TranscriptionModelDownloadPerf()
                    .setDuration(BigInt(duration))
                    .build();

    record(event);
  }

  private sendSummaryModelDownloadPerf(duration: number): void {
    const event = new CrOSEvents_RecorderApp_SummaryModelDownloadPerf()
                    .setDuration(BigInt(duration))
                    .build();

    record(event);
  }

  private sendExportPerf(duration: number, recordingSize: number): void {
    const event = new CrOSEvents_RecorderApp_ExportPerf()
                    .setDuration(BigInt(duration))
                    .setRecordingSize(BigInt(recordingSize))
                    .build();

    record(event);
  }

  private sendRecordingSavingPerf(
    duration: number,
    audioDuration: number,
    wordCount: number,
  ): void {
    const event = new CrOSEvents_RecorderApp_RecordingSavingPerf()
                    .setAudioDuration(BigInt(audioDuration))
                    .setDuration(BigInt(duration))
                    .setWordCount(BigInt(wordCount))
                    .build();

    record(event);
  }

  private sendTitleSuggestionPerf(duration: number, wordCount: number): void {
    const event = new CrOSEvents_RecorderApp_TitleSuggestionPerf()
                    .setDuration(BigInt(duration))
                    .setWordCount(BigInt(wordCount))
                    .build();

    record(event);
  }

  private sendSummaryPerf(duration: number, wordCount: number): void {
    const event = new CrOSEvents_RecorderApp_SummaryPerf()
                    .setDuration(BigInt(duration))
                    .setWordCount(BigInt(wordCount))
                    .build();

    record(event);
  }
}
