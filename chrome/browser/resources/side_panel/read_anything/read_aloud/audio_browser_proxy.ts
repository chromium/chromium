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
  onSpeechRateChange(rate: number): void;
}

export class AudioBrowserProxyImpl implements AudioBrowserProxy {
  getSpeechRate(): number {
    return chrome.readingMode.speechRate;
  }

  onSpeechRateChange(rate: number): void {
    chrome.readingMode.onSpeechRateChange(rate);
  }

  static getInstance(): AudioBrowserProxy {
    return instance || (instance = new AudioBrowserProxyImpl());
  }

  static setInstance(obj: AudioBrowserProxy) {
    instance = obj;
  }
}

let instance: AudioBrowserProxy|null = null;
