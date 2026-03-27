// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {PageCallbackRouter, PageHandlerRemote} from './ai_overlay_dialog.mojom-webui.js';
import {ApiSession} from './api_session.js';
import type {ApiConfig, ApiSessionDelegate, Tool, ToolCall} from './api_session.js';
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

export interface ConversationConfig {
  system_instruction: string;
  persona: Persona;
  api_config: ApiConfig;
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
  private readonly uiDelegate: UiDelegate;
  private readonly pageHandler: PageHandlerRemote;
  private readonly pageContextManager: PageContextManager =
      new PageContextManager(() => this.didUpdatePageContent());
  private readonly config: ConversationConfig;

  private session: ApiSession|null = null;
  private toolDefinitions: Tool[] = [];
  private state: State = State.STOPPED;
  private currentInput: string = '';
  private currentOutput: string = '';
  private mockAudioEndTime: number = 0;

  constructor(
      config: ConversationConfig, uiDelegate: UiDelegate,
      pageHandler: PageHandlerRemote, router: PageCallbackRouter,
      initialPageContext?: PageContext) {
    console.info(`Conversation with ${config.persona.name}, config`, config);
    this.config = config;
    this.uiDelegate = uiDelegate;
    this.pageHandler = pageHandler;

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
  async start() {
    const {toolDefinitionsJson} = await this.pageHandler.getToolDefinitions();
    this.toolDefinitions = JSON.parse(toolDefinitionsJson);

    this.createNewApiSession();
  }

  async onToolCall(toolCall: ToolCall) {
    const functionCalls = toolCall.functionCalls;
    const responses = [];

    for (const call of functionCalls) {
      const {name, args, id} = call;
      let result: any = {success: false};

      try {
        const jsonArgs = JSON.stringify(args);
        const {jsonResult} = await this.pageHandler.executeTool(name, jsonArgs);
        result = JSON.parse(jsonResult);
      } catch (e) {
        console.error(`Error executing tool ${name}:`, e);
      }

      responses.push({
        name,
        response: result,
        id,
      });
    }

    this.session?.sendToolResponse(responses);

    // TODO(gklassen): Add a separate object that receives events like
    // `audio ended, audio started, tool call, etc` and is responsible for
    // tracking durations and logging out metrics.
    if (this.mockAudioEndTime > 0) {
      const latency = performance.now() - this.mockAudioEndTime;
      console.info(
          `[Conversation] Time between end of mock audio and tool call ` +
          `completion: ${latency.toFixed(2)}ms`);
      this.mockAudioEndTime = 0;
    }
  }

  markMockAudioEndTime(time: number) {
    this.mockAudioEndTime = time;
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
        this.config, context?.title ?? '', context?.url || '',
        context?.content ?? undefined);

    console.info('System Instruction', systemInstruction);

    this.session = new ApiSession(
        systemInstruction, this.config.api_config, this.toolDefinitions, this);
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
