// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {bindSignal} from '../reactive/local_storage.js';
import {signal} from '../reactive/signal.js';
import {AudioSource} from '../recording_session.js';
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
 * The following transitions are possible:
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

export const settingsSchema = z.object({
  audioSource: z.nativeEnum(AudioSource),
  onboardingDone: z.boolean(),
  recordingSortType: z.nativeEnum(RecordingSortType),
  transcriptionEnabled: z.nativeEnum(TranscriptionEnableState),
});

type Settings = Infer<typeof settingsSchema>;

const defaultSettings: Settings = {
  audioSource: AudioSource.USER_MEDIA,
  onboardingDone: false,
  recordingSortType: RecordingSortType.DATE,
  transcriptionEnabled: TranscriptionEnableState.UNKNOWN,
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
