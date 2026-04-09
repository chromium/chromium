// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: crbug.com/498680741 - refactor TTS code into its own directory.

import type {SpeechBrowserProxy} from './speech_browser_proxy.js';
import {SpeechBrowserProxyImpl} from './speech_browser_proxy.js';
import type {TtsClient, TtsUtterance} from './tts_client.js';
import {getFilteredVoiceList} from './tts_voice_filtering.js';
import {defaultIsGoogle, defaultIsNatural} from './voice_language_conversions.js';

// A TTS client that uses the web speech API.
export class WebSpeechTtsClient implements TtsClient {
  static initialize(): WebSpeechTtsClient {
    return new WebSpeechTtsClient();
  }

  private synth_: SpeechBrowserProxy = SpeechBrowserProxyImpl.getInstance();

  // Set to lowest priority for multiple TTSClient sorting
  readonly priority = 0;

  play(ttsUtterance: TtsUtterance): void {
    const utterance = new SpeechSynthesisUtterance(ttsUtterance.text);
    if (ttsUtterance.voice) {
      utterance.voice = ttsUtterance.voice;
    }
    if (ttsUtterance.lang) {
      utterance.lang = ttsUtterance.lang;
    }
    if (ttsUtterance.rate) {
      utterance.rate = ttsUtterance.rate;
    }
    if (ttsUtterance.volume) {
      utterance.volume = ttsUtterance.volume;
    }
    if (ttsUtterance.onEnd) {
      utterance.onend = ttsUtterance.onEnd;
    }
    if (ttsUtterance.onBoundary) {
      utterance.onboundary = ttsUtterance.onBoundary;
    }
    if (ttsUtterance.onError) {
      utterance.onerror = ttsUtterance.onError;
    }
    if (ttsUtterance.onStart) {
      utterance.onstart = ttsUtterance.onStart;
    }

    this.synth_.speak(utterance);
  }

  stop(): void {
    this.synth_.cancel();
  }

  pause(): void {
    this.synth_.pause();
  }

  resume(): void {
    this.synth_.resume();
  }

  getVoices(): SpeechSynthesisVoice[] {
    return getFilteredVoiceList(this.synth_.getVoices());
  }

  setOnVoicesChanged(callback: () => void): void {
    this.synth_.setOnVoicesChanged(callback);
  }

  isNatural(voice: SpeechSynthesisVoice): boolean {
    return defaultIsNatural(voice);
  }

  isGoogle(voice: SpeechSynthesisVoice): boolean {
    return defaultIsGoogle(voice);
  }
}
