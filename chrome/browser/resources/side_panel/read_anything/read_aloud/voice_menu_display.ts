// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';

import type {AudioBrowserProxy} from './audio_browser_proxy.js';
// clang-format off
// <if expr="not is_chromeos">
import {hasGoogleIdentifier} from './voice_language_conversions.js';
// </if>
// clang-format on
import {getDisplayNameForLocale} from './language_display.js';
import {areVoicesEqual, hasNaturalIdentifier, NotificationType} from './voice_language_conversions.js';

// Represents an individual voice entry displayed in the voice selection menu.
export interface VoiceDropdownItem {
  title: string;
  voice: SpeechSynthesisVoice;
  selected: boolean;
  // If a preview has been initiated on a voice. This may be true before
  // the speech engine actually starts playing the preview.
  previewInitiated: boolean;
  // If a preview has actually begun playing, corresponding to .onstart
  // being called on the preview in app.ts.
  previewActuallyPlaying: boolean;
  // This ID is currently just used for testing purposes and does not ensure
  // uniqueness
  id: string;
}

// Represents a language-grouped section of voices in the voice selection menu.
export interface VoiceDropdownGroup {
  language: string;
  voices: VoiceDropdownItem[];
}

// Parameters for filtering, grouping, and mapping available voices for display.
export interface BuildVoiceDropdownGroupsParams {
  availableVoices?: SpeechSynthesisVoice[];
  enabledLangs?: string[];
  selectedVoice?: SpeechSynthesisVoice|null;
  previewVoicePlaying?: SpeechSynthesisVoice|null;
  previewVoiceInitiated?: SpeechSynthesisVoice|null;
  localeToDisplayName?: {[lang: string]: string};
}

// Resulting voice dropdown groups and metadata for roving tabindex fallback.
export interface BuildVoiceDropdownGroupsResult {
  groups: VoiceDropdownGroup[];
  hasSelectedVoice: boolean;
}

// Returns the user-facing display label for a voice.
// Outside ChromeOS, non-Google system voices are labeled with the generic
// system string. Handles null/undefined voice gracefully.
export function getVoiceTitle(voice?: SpeechSynthesisVoice|null): string {
  if (!voice) {
    return loadTimeData.getString('voiceSelectionLabel');
  }
  let title = voice.name;
  // <if expr="not is_chromeos">
  if (!hasGoogleIdentifier(voice)) {
    title = loadTimeData.getString('systemVoiceLabel');
  }
  // </if>
  return title;
}

// Sanitizes voice names for use in HTML data-test-id attributes and CSS
// selectors.
export function stringToHtmlTestId(name: string): string {
  return name.replace(/\s/g, '-').replace(/[()]/g, '');
}

// Comparator to prioritize Google Natural voices ahead of Standard/eSpeak
// voices within the same language group. Uses structural typing for voice
// containers.
export function voiceQualityRankComparator(
    item1: {voice: SpeechSynthesisVoice},
    item2: {voice: SpeechSynthesisVoice},
    ): number {
  const isItem1Natural = hasNaturalIdentifier(item1.voice);
  const isItem2Natural = hasNaturalIdentifier(item2.voice);

  if (isItem1Natural && isItem2Natural) {
    return 0;
  }
  if (!isItem1Natural && !isItem2Natural) {
    return 0;
  }
  return isItem1Natural ? -1 : 1;
}

// Determines whether the preview loading spinner should be visible.
export function isVoicePreviewSpinning(voiceDropdown: VoiceDropdownItem):
    boolean {
  return voiceDropdown.previewInitiated &&
      !voiceDropdown.previewActuallyPlaying;
}

// Filters and groups available voices by enabled languages, maps each to a
// VoiceDropdownItem, and sorts voices within each language group.
export function computeVoiceDropdown(params: BuildVoiceDropdownGroupsParams):
    BuildVoiceDropdownGroupsResult {
  const {
    availableVoices,
    enabledLangs,
    selectedVoice = null,
    previewVoicePlaying = null,
    previewVoiceInitiated = null,
    localeToDisplayName = {},
  } = params;

  if (!availableVoices || !enabledLangs) {
    return {groups: [], hasSelectedVoice: false};
  }

  const enabledLangsLowerCase: Set<string> =
      new Set(enabledLangs.map(lang => lang.toLowerCase()));
  const enabledVoices = availableVoices.filter(
      ({lang}) => lang && enabledLangsLowerCase.has(lang.toLowerCase()));

  let hasSelectedVoice = false;
  const languageToVoices =
      enabledVoices.reduce((languageToDropdownItems, voice) => {
        const isSelected = areVoicesEqual(selectedVoice, voice);
        if (isSelected) {
          hasSelectedVoice = true;
        }

        const dropdownItem: VoiceDropdownItem = {
          title: getVoiceTitle(voice),
          voice,
          id: stringToHtmlTestId(voice.name),
          selected: isSelected,
          previewActuallyPlaying: areVoicesEqual(previewVoicePlaying, voice),
          previewInitiated: areVoicesEqual(previewVoiceInitiated, voice),
        };

        const langLower = voice.lang.toLowerCase();
        const lang = localeToDisplayName[langLower] || langLower;

        if (languageToDropdownItems[lang]) {
          languageToDropdownItems[lang].push(dropdownItem);
        } else {
          languageToDropdownItems[lang] = [dropdownItem];
        }

        return languageToDropdownItems;
      }, {} as {[language: string]: VoiceDropdownItem[]});

  for (const lang of Object.keys(languageToVoices)) {
    languageToVoices[lang]!.sort(voiceQualityRankComparator);
  }

  const groups: VoiceDropdownGroup[] =
      Object.entries(languageToVoices).map(([language, voices]) => ({
                                             language,
                                             voices,
                                           }));

  return {groups, hasSelectedVoice};
}

// Resolves error notification messages for voice pack states (NO_SPACE,
// NO_INTERNET).
export function computeErrorMessages(
    currentNotifications: {[language: string]: NotificationType}|undefined,
    audioBrowserProxy: AudioBrowserProxy,
    ): string[] {
  const allocationErrors = computeMessages(
      currentNotifications, audioBrowserProxy,
      ([_, notification]) => notification === NotificationType.NO_SPACE,
      'readingModeVoiceMenuNoSpace');
  const noInternetErrors = computeMessages(
      currentNotifications, audioBrowserProxy,
      ([_, notification]) => notification === NotificationType.NO_INTERNET,
      'readingModeVoiceMenuNoInternet');
  return allocationErrors.concat(noInternetErrors);
}

// Resolves downloading notification messages for voice pack states
// (DOWNLOADING).
export function computeDownloadingMessages(
    currentNotifications: {[language: string]: NotificationType}|undefined,
    audioBrowserProxy: AudioBrowserProxy,
    ): string[] {
  return computeMessages(
      currentNotifications, audioBrowserProxy,
      ([_, notification]) => notification === NotificationType.DOWNLOADING,
      'readingModeVoiceMenuDownloading');
}

// Formats voice pack notifications using the specified string template.
function computeMessages(
    currentNotifications: {[language: string]: NotificationType}|undefined,
    audioBrowserProxy: AudioBrowserProxy,
    filterFn: (value: [string, NotificationType]) => boolean,
    message: string,
    ): string[] {
  if (!currentNotifications) {
    return [];
  }
  const entries: Array<[string, NotificationType]> =
      Object.entries(currentNotifications);
  return entries.filter(filterFn)
      .map(([lang, _]) => getDisplayNameForLocale(lang, audioBrowserProxy))
      .filter(possibleName => possibleName.length > 0)
      .map(displayName => loadTimeData.getStringF(message, displayName));
}
