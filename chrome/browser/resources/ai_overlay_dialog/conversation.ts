// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageCallbackRouter} from './ai_overlay_dialog.mojom-webui.js';
import {ApiSession} from './api_session.js';
import type {SessionState} from './api_session.js';
import type {AudioCapturer} from './audio_capturer.js';
import type {AudioPlayer} from './audio_player.js';
import {PageContextManager} from './page_context_manager.js';
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
  private pageContextManager: PageContextManager = new PageContextManager();

  constructor(apiKey: string, uiDelegate: UiDelegate) {
    this.apiKey = apiKey;
    this.uiDelegate = uiDelegate;
  }

  bindMojoHandlers(router: PageCallbackRouter) {
    router.invalidatePageContext.addListener(
        () => this.pageContextManager.invalidatePageContext());
    router.updateCurrentPageContext.addListener(
        (url, title, content) =>
            this.pageContextManager.updateCurrentPageContext(
                url, title, content));
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

    // TODO(bokan): Rebuild the session with a new system instruction each time
    // the context changes.
    // TODO(bokan): We should aim to always have an up-to-date URL and title
    // if content is stale.
    const context = this.pageContextManager.pageContext;
    const systemInstruction = buildSystemInstruction(
        'You are a helpful assistant.', context?.title || '',
        context?.url || '', context?.content);

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
