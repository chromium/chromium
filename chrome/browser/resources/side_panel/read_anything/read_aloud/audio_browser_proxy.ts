// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

import {EventForwarder} from '../content/read_anything_types.js';

// Browser proxy for C++ ReadAnythingAppController (chrome.readingMode) state,
// preferences, and actions related to audio and speech synthesis (such as
// speech rate, granularity, playing state, and voice packs).
//
// Distinct from SpeechBrowserProxy, which proxies the Web Speech API
// (window.speechSynthesis) for speech playback and voice management.
export interface AudioBrowserProxy {
  //////////////////////////////////////////////////////////////////////////////
  // Incoming events (C++ -> TypeScript):

  onLockScreen: ChromeEvent<() => void>;
  onTabMuteStateChange: ChromeEvent<(muted: boolean) => void>;
  onTtsEngineInstalled: ChromeEvent<() => void>;
  readingModeWillClose: ChromeEvent<() => void>;
  setPlayOnOpen: ChromeEvent<(playOnOpen: boolean) => void>;
  updateVoicePackStatus: ChromeEvent<(lang: string, status: string) => void>;

  //////////////////////////////////////////////////////////////////////////////
  // Outgoing calls (TypeScript -> C++):

  getSpeechRate(): number;
  getHighlightGranularity(): number;
  getAutoHighlighting(): number;
  getWordHighlighting(): number;
  getPhraseHighlighting(): number;
  getSentenceHighlighting(): number;
  getNoHighlighting(): number;
  isPhraseHighlightingEnabled(): boolean;
  getDisplayNameForLocale(locale: string, displayLocale: string): string;
  getDefaultLanguageForSpeech(): string;
  getBaseLanguageForSpeech(): string;
  getStoredVoice(): string;
  getLanguagesEnabledInPref(): string[];
  isHighlightOn(): boolean;
  onSpeechRateChange(rate: number): void;
  onHighlightGranularityChanged(granularity: number): void;
  onVoiceChange(voice: string, lang: string): void;
  onLanguagePrefChange(lang: string, enabled: boolean): void;
  sendGetVoicePackInfoRequest(lang: string): void;
  sendInstallVoicePackRequest(lang: string): void;
  sendUninstallVoiceRequest(lang: string): void;
  onIsSpeechActiveChanged(isSpeechActive: boolean): void;
  onIsAudioCurrentlyPlayingChanged(isAudioCurrentlyPlaying: boolean): void;
  onSpeechEngineFirstStall(): void;
  onSpeechEngineStalled(): void;
  getKeyboardShortcutStopSource(): number;
  getEngineErrorStopSource(): number;
  getEngineInterruptStopSource(): number;
  getContentFinishedStopSource(): number;
  getPauseButtonStopSource(): number;
  getUnexpectedUpdateContentStopSource(): number;
}

export class AudioBrowserProxyImpl implements AudioBrowserProxy {
  onLockScreen = new EventForwarder<() => void>();
  onTabMuteStateChange = new EventForwarder<(muted: boolean) => void>();
  onTtsEngineInstalled = new EventForwarder<() => void>();
  readingModeWillClose = new EventForwarder<() => void>();
  setPlayOnOpen = new EventForwarder<(playOnOpen: boolean) => void>();
  updateVoicePackStatus =
      new EventForwarder<(lang: string, status: string) => void>();

  constructor() {
    chrome.readingMode.onLockScreen = () => {
      this.onLockScreen.forward();
    };

    chrome.readingMode.onTabMuteStateChange = (muted: boolean) => {
      this.onTabMuteStateChange.forward(muted);
    };

    chrome.readingMode.onTtsEngineInstalled = () => {
      this.onTtsEngineInstalled.forward();
    };

    chrome.readingMode.readingModeWillClose = () => {
      this.readingModeWillClose.forward();
    };

    chrome.readingMode.setPlayOnOpen = (playOnOpen: boolean) => {
      this.setPlayOnOpen.forward(playOnOpen);
    };

    chrome.readingMode.updateVoicePackStatus =
        (lang: string, status: string) => {
          this.updateVoicePackStatus.forward(lang, status);
        };
  }

  getSpeechRate(): number {
    return chrome.readingMode.speechRate;
  }

  getHighlightGranularity(): number {
    return chrome.readingMode.highlightGranularity;
  }

  getAutoHighlighting(): number {
    return chrome.readingMode.autoHighlighting;
  }

  getWordHighlighting(): number {
    return chrome.readingMode.wordHighlighting;
  }

  getPhraseHighlighting(): number {
    return chrome.readingMode.phraseHighlighting;
  }

  getSentenceHighlighting(): number {
    return chrome.readingMode.sentenceHighlighting;
  }

  getNoHighlighting(): number {
    return chrome.readingMode.noHighlighting;
  }

  isPhraseHighlightingEnabled(): boolean {
    return chrome.readingMode.isPhraseHighlightingEnabled;
  }

  getDisplayNameForLocale(locale: string, displayLocale: string): string {
    return chrome.readingMode.getDisplayNameForLocale(locale, displayLocale);
  }

  getDefaultLanguageForSpeech(): string {
    return chrome.readingMode.defaultLanguageForSpeech;
  }

  getBaseLanguageForSpeech(): string {
    return chrome.readingMode.baseLanguageForSpeech;
  }

  getStoredVoice(): string {
    return chrome.readingMode.getStoredVoice();
  }

  getLanguagesEnabledInPref(): string[] {
    return chrome.readingMode.getLanguagesEnabledInPref();
  }

  isHighlightOn(): boolean {
    return chrome.readingMode.isHighlightOn();
  }

  onSpeechRateChange(rate: number): void {
    chrome.readingMode.onSpeechRateChange(rate);
  }

  onHighlightGranularityChanged(granularity: number): void {
    chrome.readingMode.onHighlightGranularityChanged(granularity);
  }

  onVoiceChange(voice: string, lang: string): void {
    chrome.readingMode.onVoiceChange(voice, lang);
  }

  onLanguagePrefChange(lang: string, enabled: boolean): void {
    chrome.readingMode.onLanguagePrefChange(lang, enabled);
  }

  sendGetVoicePackInfoRequest(lang: string): void {
    chrome.readingMode.sendGetVoicePackInfoRequest(lang);
  }

  sendInstallVoicePackRequest(lang: string): void {
    chrome.readingMode.sendInstallVoicePackRequest(lang);
  }

  sendUninstallVoiceRequest(lang: string): void {
    chrome.readingMode.sendUninstallVoiceRequest(lang);
  }

  onIsSpeechActiveChanged(isSpeechActive: boolean): void {
    chrome.readingMode.onIsSpeechActiveChanged(isSpeechActive);
  }

  onIsAudioCurrentlyPlayingChanged(isAudioCurrentlyPlaying: boolean): void {
    chrome.readingMode.onIsAudioCurrentlyPlayingChanged(
        isAudioCurrentlyPlaying);
  }

  onSpeechEngineFirstStall(): void {
    chrome.readingMode.onSpeechEngineFirstStall();
  }

  onSpeechEngineStalled(): void {
    chrome.readingMode.onSpeechEngineStalled();
  }

  getKeyboardShortcutStopSource(): number {
    return chrome.readingMode.keyboardShortcutStopSource;
  }

  getEngineErrorStopSource(): number {
    return chrome.readingMode.engineErrorStopSource;
  }

  getEngineInterruptStopSource(): number {
    return chrome.readingMode.engineInterruptStopSource;
  }

  getContentFinishedStopSource(): number {
    return chrome.readingMode.contentFinishedStopSource;
  }

  getPauseButtonStopSource(): number {
    return chrome.readingMode.pauseButtonStopSource;
  }

  getUnexpectedUpdateContentStopSource(): number {
    return chrome.readingMode.unexpectedUpdateContentStopSource;
  }

  static getInstance(): AudioBrowserProxy {
    return instance || (instance = new AudioBrowserProxyImpl());
  }

  static setInstance(obj: AudioBrowserProxy) {
    instance = obj;
  }
}

let instance: AudioBrowserProxy|null = null;
