// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Browser proxy for C++ ReadAnythingAppController (chrome.readingMode) state,
// preferences, and actions related to audio and speech synthesis (such as
// speech rate, granularity, playing state, and voice packs).
//
// Distinct from SpeechBrowserProxy, which proxies the Web Speech API
// (window.speechSynthesis) for speech playback and voice management.
export interface AudioBrowserProxy {
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
  onSpeechRateChange(rate: number): void;
  onHighlightGranularityChanged(granularity: number): void;
}

export class AudioBrowserProxyImpl implements AudioBrowserProxy {
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

  onSpeechRateChange(rate: number): void {
    chrome.readingMode.onSpeechRateChange(rate);
  }

  onHighlightGranularityChanged(granularity: number): void {
    chrome.readingMode.onHighlightGranularityChanged(granularity);
  }

  static getInstance(): AudioBrowserProxy {
    return instance || (instance = new AudioBrowserProxyImpl());
  }

  static setInstance(obj: AudioBrowserProxy) {
    instance = obj;
  }
}

let instance: AudioBrowserProxy|null = null;
