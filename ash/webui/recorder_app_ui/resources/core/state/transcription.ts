// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {usePlatformHandler} from '../lit/context.js';
import {LanguageCode} from '../soda/language_info.js';
import {assertExhaustive} from '../utils/assert.js';

import {
  settings,
  TranscriptionEnableState,
} from './settings.js';

/**
 * Disables transcription.
 *
 * @param firstTime Whether users disable transcription for the first time.
 */
export function disableTranscription(firstTime = false): void {
  settings.mutate((s) => {
    s.transcriptionEnabled = firstTime ?
      TranscriptionEnableState.DISABLED_FIRST :
      TranscriptionEnableState.DISABLED;
  });
}

/**
 * Enables transcription.
 */
export function enableTranscription(): void {
  settings.mutate((s) => {
    s.transcriptionEnabled = TranscriptionEnableState.ENABLED;
  });
  const selectedLanguage = settings.value.transcriptionLanguage;
  if (selectedLanguage !== null) {
    void usePlatformHandler().installSoda(selectedLanguage);
  }
}

/**
 * Set transcription language and install Soda.
 */
export function setTranscriptionLanguage(language: LanguageCode): void {
  settings.mutate((s) => {
    s.transcriptionLanguage = language;
  });
  void usePlatformHandler().installSoda(language);
}

/**
 * Toggles `TranscriptionEnabled` state.
 *
 * Returns false if the transcription hasn't been enabled before and needs to
 * ask for user consent before enabling.
 *
 * @return Boolean indicating whether `TranscriptionEnabled` is toggled.
 */
export function toggleTranscriptionEnabled(): boolean {
  switch (settings.value.transcriptionEnabled) {
    case TranscriptionEnableState.ENABLED:
      disableTranscription();
      return true;
    case TranscriptionEnableState.DISABLED:
      enableTranscription();
      return true;
    case TranscriptionEnableState.UNKNOWN:
    case TranscriptionEnableState.DISABLED_FIRST:
      return false;
    default:
      assertExhaustive(settings.value.transcriptionEnabled);
  }
}
