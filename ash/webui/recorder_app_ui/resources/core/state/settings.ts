// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {bindSignal} from '../reactive/local_storage.js';
import {signal} from '../reactive/signal.js';
import * as localStorage from '../utils/local_storage.js';
import {Infer, z} from '../utils/schema.js';

export enum RecordingSortType {
  DATE = 'DATE',
  NAME = 'NAME',
}

/**
 * The state of whether user have enabled the transcription.
 *
 * Whether the transcription is available / ready should be queried from the
 * platform handler.
 *
 * Valid state transitions:
 * * ENABLED -> DISABLED
 * * DISABLED -> ENABLED
 * * DISABLED_FIRST -> ENABLED
 * * UNKNOWN -> DISABLED_FIRST, ENABLED.
 */
export enum TranscriptionEnableState {
  /**
   * The transcription is enabled by user.
   */
  ENABLED = 'ENABLED',

  /**
   * The transcription is disabled by user and user have never enabled
   * transcription.
   *
   * This is a separate state since an additional confirmation dialog will be
   * shown only when user never enabled transcription before.
   */
  DISABLED_FIRST = 'DISABLED_FIRST',

  /*
   * The transcription is disabled by user after have been enabled at least
   * once.
   */
  DISABLED = 'DISABLED',

  /**
   * The transcription preference for user is still unknown.
   */
  UNKNOWN = 'UNKNOWN',
}

/**
 * The state of whether user have enabled summary.
 *
 * Whether the summary model is available / ready should be queried from the
 * platform handler, and this state only reflects the user choice.
 *
 * Valid state transitions:
 * * ENABLED -> DISABLED
 * * DISABLED -> ENABLED
 * * UNKNOWN -> DISABLED, ENABLED.
 */
export enum SummaryEnableState {
  /**
   * Summary is enabled by user.
   */
  ENABLED = 'ENABLED',

  /**
   * Summary is disabled by user.
   */
  DISABLED = 'DISABLED',

  /**
   * Summary enable/disable preference is still unknown.
   */
  UNKNOWN = 'UNKNOWN',
}

/**
 * The state of whether user have enabled speaker label.
 *
 * We need to ask for consent when user first transitions from UNKNOWN to
 * ENABLED.
 *
 * Valid state transitions:
 * * ENABLED -> DISABLED
 * * DISABLED -> ENABLED
 * * DISABLED_FIRST -> ENABLED
 * * UNKNOWN -> DISABLED_FIRST, ENABLED.
 */
export enum SpeakerLabelEnableState {
  /**
   * Speaker label is enabled by user.
   */
  ENABLED = 'ENABLED',

  /**
   * The speaker label is disabled by user and user have never enabled
   * speaker label.
   *
   * This is a separate state since an additional confirmation dialog will be
   * shown only when user never enabled speaker label before.
   */
  DISABLED_FIRST = 'DISABLED_FIRST',

  /**
   * Speaker label is disabled by user.
   */
  DISABLED = 'DISABLED',

  /**
   * Speaker label enable/disable preference is still unknown.
   */
  UNKNOWN = 'UNKNOWN',
}

export enum ExportAudioFormat {
  // TODO: b/344784478 - Add other supported formats. Might need ffmpeg to
  // convert.
  // TODO: b/344784478 - webm that we recorded directly is not ideal for export
  // format, since it doesn't have the length metadata when played in
  // backlight.
  WEBM_ORIGINAL = 'WEBM_ORIGINAL',
}

export enum ExportTranscriptionFormat {
  // TODO: b/344784478 - Add other supported formats.
  TXT = 'TXT',
}

/**
 * Language code used for transcription.
 *
 * This is temporarily listed since the only supported language is en-US, and
 * should be replaced with the type from `LanguageCode` in
 * components/soda/constants.h.
 */
export enum TranscriptionLanguage {
  NONE = 0,
  EN_US = 1,
}

export const exportSettingsSchema = z.object({
  // Whether audio should be exported.
  audio: z.boolean(),
  // Audio format for export.
  audioFormat: z.nativeEnum(ExportAudioFormat),
  // Whether transcription should be exported.
  transcription: z.boolean(),
  // Transcription format for export.
  transcriptionFormat: z.nativeEnum(ExportTranscriptionFormat),
});

export type ExportSettings = Infer<typeof exportSettingsSchema>;

export const settingsSchema = z.object({
  exportSettings: exportSettingsSchema,
  includeSystemAudio: z.boolean(),
  keepScreenOn: z.withDefault(z.boolean(), false),
  onboardingDone: z.boolean(),
  recordingSortType: z.nativeEnum(RecordingSortType),
  transcriptionEnabled: z.nativeEnum(TranscriptionEnableState),
  summaryEnabled: z.nativeEnum(SummaryEnableState),
  speakerLabelEnabled: z.withDefault(
    z.nativeEnum(SpeakerLabelEnableState),
    SpeakerLabelEnableState.UNKNOWN,
  ),
  systemAudioConsentDone: z.withDefault(z.boolean(), false),
});

type Settings = Infer<typeof settingsSchema>;

const defaultSettings: Settings = {
  exportSettings: {
    audio: true,
    audioFormat: ExportAudioFormat.WEBM_ORIGINAL,
    transcription: false,
    transcriptionFormat: ExportTranscriptionFormat.TXT,
  },
  includeSystemAudio: false,
  keepScreenOn: false,
  onboardingDone: false,
  recordingSortType: RecordingSortType.DATE,
  transcriptionEnabled: TranscriptionEnableState.UNKNOWN,
  summaryEnabled: SummaryEnableState.UNKNOWN,
  speakerLabelEnabled: SpeakerLabelEnableState.UNKNOWN,
  systemAudioConsentDone: false,
};

export const settings = signal(defaultSettings);

/**
 * Initializes settings related states.
 *
 * This binds the state with value from localStorage.
 */
export function init(): void {
  bindSignal(
    settings,
    localStorage.Key.SETTINGS,
    settingsSchema,
    defaultSettings,
  );
}
