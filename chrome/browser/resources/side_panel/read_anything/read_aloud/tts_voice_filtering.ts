// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleToExactVoicePackLocale, defaultIsGoogle, hasEspeakIdentifier} from './voice_language_conversions.js';

// Helper for filtering the voice list.
export function getFilteredVoiceList(possibleVoices: SpeechSynthesisVoice[]):
    SpeechSynthesisVoice[] {
  let availableVoices = possibleVoices;
  if (availableVoices.some(({localService}) => localService)) {
    availableVoices = availableVoices.filter(({localService}) => localService);
  }
  // Filter out Android voices on ChromeOS. Android Speech Recognition
  // voices are technically network voices, but for some reason, some
  // voices are marked as localService voices, so filtering localService
  // doesn't filter them out. Since they can cause unexpected behavior
  // in Read Aloud, go ahead and filter them out. To avoid causing any
  // unexpected behavior outside of ChromeOS, just filter them on ChromeOS.
  if (chrome.readingMode.isChromeOsAsh) {
    availableVoices = availableVoices.filter(
        ({name}) => !name.toLowerCase().includes('android'));
    // Filter out espeak voices if there exists a Google voice in the same
    // locale.
    availableVoices = availableVoices.filter(
        voice => !hasEspeakIdentifier(voice) ||
            convertLangOrLocaleToExactVoicePackLocale(voice.lang) ===
                undefined);
  } else {
    // Group non-Google voices by language and select a default voice for each
    // language. This represents the system voice for each language.
    const languageToNonGoogleVoices =
        availableVoices.filter(voice => !defaultIsGoogle(voice))
            .reduce((map, voice) => {
              map[voice.lang] = map[voice.lang] || [];
              map[voice.lang]!.push(voice);
              return map;
            }, {} as {[language: string]: SpeechSynthesisVoice[]});
    // Only keep system voices that exactly match Google TTS supported locales,
    // or for languages for which there are no Google TTS supported locales.
    const systemVoices =
        Object.values(languageToNonGoogleVoices)
            .map((voices) => {
              const defaultVoice = voices.find(voice => voice.default);
              return defaultVoice || voices[0];
            })
            .filter(
                systemVoice => AVAILABLE_GOOGLE_TTS_LOCALES.has(
                                   systemVoice!.lang.toLowerCase()) ||
                    convertLangOrLocaleToExactVoicePackLocale(
                        systemVoice!.lang.toLowerCase()) === undefined);

    // Keep all Google voices and one system voice per language.
    availableVoices = availableVoices.filter(
        voice => defaultIsGoogle(voice) || systemVoices.includes(voice));
  }

  return availableVoices;
}
