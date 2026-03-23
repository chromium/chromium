// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageCallbackRouter} from './ai_overlay_dialog.mojom-webui.js';
import {ApiSession} from './api_session.js';
import type {ApiSessionDelegate} from './api_session.js';
import {PageContextManager} from './page_context_manager.js';
import {buildSystemInstruction} from './persona.js';

/**
 * States for the conversation.
 */
export enum State {
  STOPPED = 'stopped',
  LISTENING = 'listening',
  TALKING = 'talking',
}

interface UiDelegate {
  sendToUI: (msg: any) => void;
  onStateChange: (state: State, oldState: State) => void;
  onResponse: (audioData: string) => void;
}

/**
 * Conversation is the central "brains" of the AI Overlay. It manages the
 * conversation state and bridges the UI with the API session.
 */
export class Conversation implements ApiSessionDelegate {
  private session: ApiSession|null = null;
  private apiKey: string;
  private uiDelegate: UiDelegate;
  private pageContextManager: PageContextManager = new PageContextManager();

  private state: State = State.STOPPED;

  constructor(apiKey: string, uiDelegate: UiDelegate) {
    this.apiKey = apiKey;
    this.uiDelegate = uiDelegate;
  }

  get connected(): boolean {
    return this.state !== State.STOPPED;
  }

  /**
   * ApiSessionDelegate interface
   */

  onResponse(audioData: string) {
    if (!this.connected) {
      return;
    }

    this.setState(State.TALKING);
    this.uiDelegate.onResponse(audioData);
  }

  interrupt() {
    if (!this.connected) {
      return;
    }

    this.setState(State.LISTENING);
  }

  onConnectionChanged(connected: boolean) {
    if (!this.connected && connected) {
      this.setState(State.LISTENING);
    }

    // Note: the connection being torn down does not stop the conversation. This
    // is (usually) a normal occurrence as updated page context causes the API
    // session to be recreated.
  }

  /**
   * Conversation
   */

  sendAudio(sampleRate: number, data: string) {
    if (!this.connected) {
      return;
    }

    this.session?.sendAudio(sampleRate, data);
  }

  bindMojoHandlers(router: PageCallbackRouter) {
    router.invalidatePageContext.addListener(
        () => this.pageContextManager.invalidatePageContext());
    router.updateCurrentPageContext.addListener(
        (url, title, content) =>
            this.pageContextManager.updateCurrentPageContext(
                url, title, content));
  }

  /**
   * Connects to the server and establishes a new session, moves the
   * conversation into a live state once the connection is ready.
   */
  start() {
    // TODO(bokan): Rebuild the session with a new system instruction each time
    // the context changes.
    // TODO(bokan): We should aim to always have an up-to-date URL and title
    // if content is stale.
    const context = this.pageContextManager.pageContext;
    const systemInstruction = buildSystemInstruction(
        'You are a helpful assistant.', context?.title || '',
        context?.url || '', context?.content);

    this.session = new ApiSession(this.apiKey, systemInstruction, this);

    this.session.connect();
  }

  /**
   * Tears down the session with the server and moves into a stopped state
   * where nothing should be happening.
   */
  stop() {
    this.session?.stop();
    this.session = null;
    this.setState(State.STOPPED);
  }

  private setState(state: State) {
    if (this.state === state) {
      return;
    }

    const oldState = this.state;
    this.state = state;
    this.uiDelegate.onStateChange(state, oldState);
  }
}
