// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {PageCallbackRouter} from './ai_overlay_dialog.mojom-webui.js';
import {ApiSession} from './api_session.js';
import type {ApiSessionDelegate} from './api_session.js';
import type {PageContext} from './page_context_manager.js';
import {PageContextManager} from './page_context_manager.js';
import {buildSystemInstruction} from './persona.js';

/* A bundle of information about how to initialize the model's personality */
export interface Persona {
  id: string;
  name: string;
  persona: string;
  voice: string;
}

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
  private readonly apiKey: string;
  private readonly uiDelegate: UiDelegate;
  private readonly pageContextManager: PageContextManager =
      new PageContextManager(() => this.didUpdatePageContent());
  private readonly persona: Persona;

  private session: ApiSession|null = null;
  private state: State = State.STOPPED;
  private currentInput: string = '';
  private currentOutput: string = '';

  constructor(
      apiKey: string, persona: Persona, uiDelegate: UiDelegate,
      router: PageCallbackRouter, initialPageContext?: PageContext) {
    console.info(`Conversation with ${persona.name}`, persona);
    this.apiKey = apiKey;
    this.persona = persona;
    this.uiDelegate = uiDelegate;

    if (initialPageContext) {
      this.pageContextManager.didChangePage(
          initialPageContext.url, initialPageContext.title,
          initialPageContext.content);
    }

    router.didChangePage.addListener(
        (url: string, title: string|null, content: string|null) =>
            this.pageContextManager.didChangePage(url, title, content));
    router.updateCurrentPageContext.addListener(
        (title: string, content: string) =>
            this.pageContextManager.updateCurrentPageContext(title, content));
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

  onTranscription(text: string, isInput: boolean) {
    if (!this.connected) {
      return;
    }

    if (isInput) {
      this.currentInput += text;
    } else {
      this.currentOutput += text;

      this.uiDelegate.sendToUI({
        type: 'outputTranscription',
        text: this.currentOutput,
      });
    }
  }

  onTurnComplete() {
    this.currentInput = '';
    this.currentOutput = '';
  }

  interrupt() {
    if (!this.connected) {
      return;
    }

    this.currentInput = '';
    this.currentOutput = '';
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

  /**
   * Connects to the server and establishes a new session, moves the
   * conversation into a live state once the connection is ready.
   */
  start() {
    assert(!this.connected);
    this.createNewApiSession();
  }

  /**
   * Tears down the session with the server and moves into a stopped state
   * where nothing should be happening.
   */
  stop() {
    assert(this.connected);
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

  private createNewApiSession() {
    assert(!this.session);

    const context = this.pageContextManager.pageContext;
    const systemInstruction = buildSystemInstruction(
        this.persona.persona, context?.title ?? '', context?.url || '',
        context?.content ?? undefined);

    this.session = new ApiSession(this.apiKey, systemInstruction, this);
    this.session.connect();
  }

  private didUpdatePageContent() {
    if (!this.connected) {
      return;
    }

    assert(this.session);
    this.session.stop();
    this.session = null;

    this.createNewApiSession();
  }
}
