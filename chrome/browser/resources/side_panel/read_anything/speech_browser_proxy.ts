// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface SpeechBrowserProxy {
  cancel(): void;
  getVoices(): SpeechSynthesisVoice[];
  pause(): void;
  resume(): void;
  setOnVoicesChanged(onvoiceschanged: (event: Event) => any): void;
  speak(utterance: SpeechSynthesisUtterance): void;
}

export class SpeechBrowserProxyImpl implements SpeechBrowserProxy {
  private synth_: SpeechSynthesis;

  constructor() {
    this.synth_ = window.speechSynthesis;
  }

  cancel() {
    this.synth_.cancel();
  }

  getVoices(): SpeechSynthesisVoice[] {
    return this.synth_.getVoices();
  }

  pause() {
    this.synth_.pause();
  }

  resume() {
    this.synth_.resume();
  }

  setOnVoicesChanged(onvoiceschanged: (event: Event) => any) {
    this.synth_.onvoiceschanged = onvoiceschanged;
  }

  speak(utterance: SpeechSynthesisUtterance) {
    this.synth_.speak(utterance);
  }

  static getInstance(): SpeechBrowserProxy {
    return instance || (instance = new SpeechBrowserProxyImpl());
  }

  static setInstance(obj: SpeechBrowserProxy) {
    instance = obj;
  }
}

let instance: SpeechBrowserProxy|null = null;
