// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ApiSession} from './api_session.js';
import type {SessionState} from './api_session.js';
import type {AudioCapturer} from './audio_capturer.js';
import type {AudioPlayer} from './audio_player.js';
import {buildSystemInstruction} from './persona.js';

interface UiDelegate {
  sendToUI: (msg: any) => void;
  onStateChange: (state: SessionState) => void;
  createAudioCapturer: () => Promise<AudioCapturer|null>;
  createAudioPlayer: () => AudioPlayer;
}

/**
 * Conversation is the central "brains" of the AI Overlay. It manages the
 * conversation state and bridges the UI with the API session.
 */
export class Conversation {
  private session: ApiSession|null = null;
  private apiKey: string;
  private uiDelegate: UiDelegate;
  private capturer: AudioCapturer|null = null;
  private player: AudioPlayer|null = null;

  constructor(apiKey: string, uiDelegate: UiDelegate) {
    this.apiKey = apiKey;
    this.uiDelegate = uiDelegate;
  }

  async start() {
    this.capturer = await this.uiDelegate.createAudioCapturer();
    if (!this.capturer) {
      console.error('No audio capturer available');
      return;
    }

    this.player = this.uiDelegate.createAudioPlayer();

    if (!this.player) {
      console.error('No audio player available');
      return;
    }

    // TODO(bokan): track current page context.
    const systemInstruction = buildSystemInstruction(
        'You are a helpful assistant.', /*title=*/ '', /*url=*/ '');

    this.session = new ApiSession(
        this.apiKey, systemInstruction, this.capturer, this.player,
        this.uiDelegate.onStateChange.bind(this.uiDelegate));

    this.session.connect();
  }

  stop() {
    this.session?.stop();
    this.session = null;
    this.capturer = null;
    this.player = null;
  }
}
