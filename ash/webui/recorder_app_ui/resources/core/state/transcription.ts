// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {createTranscriptionModelDownloadPerf} from '../events_sender.js';
import {usePlatformHandler} from '../lit/context.js';
import {LanguageCode} from '../soda/language_info.js';
import {assertExhaustive} from '../utils/assert.js';

import {settings, TranscriptionEnableState} from './settings.js';

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
 * Installs Soda and starts download perf event.
 */
export function installSoda(language: LanguageCode): void {
  const platformHandler = usePlatformHandler();
  // Records download events initiated from UI download buttons.
  if (platformHandler.getSodaState(language).value.kind === 'notInstalled') {
    platformHandler.perfLogger.start(
      createTranscriptionModelDownloadPerf(language),
    );
  }
  // TODO: b/375306309 -  Install only if the state is `notInstalled` after the
  // `OnSodaUninstalled` event is implemented and there's no inconsistent soda
  // state.
  void platformHandler.installSoda(language);
}

/**
 * Enables transcription.
 */
export function enableTranscriptionSkipConsentCheck(): void {
  settings.mutate((s) => {
    s.transcriptionEnabled = TranscriptionEnableState.ENABLED;
  });
  const platformHandler = usePlatformHandler();
  const selectedLanguage = platformHandler.getSelectedLanguage();
  if (selectedLanguage !== null) {
    installSoda(selectedLanguage);
  }
}

/**
 * Set transcription language and install Soda.
 */
export function setTranscriptionLanguage(language: LanguageCode): void {
  settings.mutate((s) => {
    s.transcriptionLanguage = language;
  });
  installSoda(language);
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
      enableTranscriptionSkipConsentCheck();
      return true;
    case TranscriptionEnableState.UNKNOWN:
    case TranscriptionEnableState.DISABLED_FIRST:
      return false;
    default:
      assertExhaustive(settings.value.transcriptionEnabled);
  }
}

/**
 * Enables transcription if the user consent is already given.
 *
 * Returns false if the transcription hasn't been enabled before and needs to
 * ask for user consent before enabling.
 *
 * @return Boolean indicating whether `TranscriptionEnabled` is enabled.
 */
export function enableTranscription(): boolean {
  switch (settings.value.transcriptionEnabled) {
    case TranscriptionEnableState.ENABLED:
      return true;
    case TranscriptionEnableState.DISABLED:
      enableTranscriptionSkipConsentCheck();
      return true;
    case TranscriptionEnableState.UNKNOWN:
    case TranscriptionEnableState.DISABLED_FIRST:
      return false;
    default:
      assertExhaustive(settings.value.transcriptionEnabled);
  }
}
