// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {PageCallbackRouter} from './ai_overlay_dialog.mojom-webui.js';
import {ApiSession} from './api_session.js';
import type {ApiSessionConfig, ApiSessionDelegate, Tool, ToolCall} from './api_session.js';
import {Journal} from './journal.js';
import {debugLog, DebugLogTag, errorLog, log} from './logging.js';
import type {PageContext, PageContextChangeEvent} from './page_context_manager.js';
import {PageContextChangeType, PageContextManager} from './page_context_manager.js';
import {buildContextPrimingTurn, buildSystemInstruction, formatPageVisitHistory, formatTranscript} from './persona.js';


const FILE = 'Conversation';
import type {AiOverlayToolsRemote} from './tools.mojom-webui.js';
import {ToolExecutor} from './tools/tool_executor.js';

/**
 * Information about how to initialize the model's personality. Corresponds to
 * the persona JSON defined in the bundle.
 */
export interface Persona {
  id: string;
  name: string;
  nicknames: string[];
  persona: string;
  voice: string;
}

/**
 * Information about setup for the model connection. Corresponds to the
 * api_config JSON defined in the bundle.
 */
export interface ApiConfig {
  endpointUrl: string;
  model: string;
  apiKey: string;
}

/**
 * Configuration information used to initialize a Conversation object.
 */
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

export interface OutputTranscriptionMessage {
  type: 'outputTranscription';
  text: string;
}

interface UiDelegate {
  sendToUI: (msg: OutputTranscriptionMessage) => void;
  onStateChange: (state: State, oldState: State) => void;
  onResponse: (audioData: string) => void;
}

/**
 * Conversation is the central "brains" of the AI Overlay. It manages the
 * conversation state and bridges the UI with the API session.
 */
export class Conversation implements ApiSessionDelegate {
  private readonly uiDelegate: UiDelegate;
  private readonly toolExecutor: ToolExecutor;
  private readonly pageContextManager: PageContextManager =
      new PageContextManager();
  private readonly config: ConversationConfig;
  private readonly journal: Journal = new Journal(this.pageContextManager);

  private session: ApiSession|null = null;
  private toolDefinitions: Tool[] = [];
  private state: State = State.STOPPED;
  private currentInput: string = '';
  private currentOutput: string = '';
  private mockAudioEndTime: number = 0;

  constructor(
      config: ConversationConfig, uiDelegate: UiDelegate,
      toolsRemote: AiOverlayToolsRemote, router: PageCallbackRouter,
      initialPageContext?: PageContext) {
    const assistantName = config.persona.name;
    log(FILE, `Conversation with ${assistantName}, config`, config);
    this.config = config;
    this.uiDelegate = uiDelegate;
    this.toolExecutor = new ToolExecutor(
        toolsRemote, this.pageContextManager, this.journal, assistantName);

    this.pageContextManager.registerListener((event) => {
      this.onPageContextChange(event);
    });

    if (initialPageContext) {
      this.pageContextManager.createNewPageContext(
          initialPageContext.url, initialPageContext.title,
          initialPageContext.content);
    }

    router.didChangePage.addListener(
        (url: string, title: string|null, content: string|null) =>
            this.pageContextManager.createNewPageContext(url, title, content));
    router.updateCurrentPageContext.addListener(
        (title: string, content: string) =>
            this.pageContextManager.updateCurrentPageContext(title, content));
  }

  get connected(): boolean {
    return this.state !== State.STOPPED;
  }

  get pageContext(): PageContext|null {
    return this.pageContextManager.pageContext;
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
      this.journal.updateCurrentTurn(text, undefined);
    } else {
      this.currentOutput += text;
      this.journal.updateCurrentTurn(undefined, text);

      this.uiDelegate.sendToUI({
        type: 'outputTranscription',
        text: this.currentOutput,
      });
    }
  }

  onTurnComplete() {
    this.currentInput = '';
    this.currentOutput = '';
    this.journal.completeTurn();
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

  sendText(text: string) {
    if (!this.connected) {
      return;
    }

    this.session?.sendText(text);
  }

  /**
   * Connects to the server and establishes a new session, moves the
   * conversation into a live state once the connection is ready.
   */
  async start() {
    const toolDefinitionsJson = this.toolExecutor.getToolDefinitions();
    try {
      if (toolDefinitionsJson && toolDefinitionsJson.trim() !== '') {
        this.toolDefinitions = JSON.parse(toolDefinitionsJson);
      } else {
        this.toolDefinitions = [];
      }
    } catch (e) {
      errorLog(
          FILE,
          'Failed to parse toolDefinitionsJson: ' + e +
              '\nJSON start: ' + toolDefinitionsJson.substring(0, 500));
      this.toolDefinitions = [];
    }

    await this.createNewApiSession();
  }

  async onToolCall(toolCall: ToolCall) {
    const functionCalls = toolCall.functionCalls;
    const responses = [];

    for (const call of functionCalls) {
      const {name, args, id} = call;

      let scheduling: string|undefined = undefined;
      const result = await this.toolExecutor.executeTool(name, args);

      if (typeof result['scheduling'] === 'string') {
        scheduling = result['scheduling'];
        delete result['scheduling'];
      }

      responses.push({
        name,
        response: result,
        id,
        scheduling,
      });
    }

    this.session?.sendToolResponse(responses);

    // TODO(gklassen): Add a separate object that receives events like
    // `audio ended, audio started, tool call, etc` and is responsible for
    // tracking durations and logging out metrics.
    if (this.mockAudioEndTime > 0) {
      const latency = performance.now() - this.mockAudioEndTime;
      log(FILE,
          `Time between end of mock audio and tool call ` +
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

  private async createNewApiSession() {
    assert(!this.session);

    const context = this.pageContextManager.pageContext;

    const turns = this.journal.getTurnEntries();
    const pages = this.journal.getPageVisitEntries();

    const assistantName = this.config.persona.name;
    const transcript = formatTranscript(turns, assistantName);
    const pageHistory = formatPageVisitHistory(pages);

    // Behavioral rules are sent in the system instruction (Static/Trusted).
    const systemInstruction = buildSystemInstruction(this.config);

    debugLog(FILE, DebugLogTag.SYSTEM_INSTRUCTION, systemInstruction);

    const apiSessionConfig: ApiSessionConfig = {
      endpointUrl: this.config.api_config.endpointUrl,
      model: this.config.api_config.model,
      apiKey: this.config.api_config.apiKey,
      systemInstruction,
      voiceName: this.config.persona.voice,
    };

    this.session = new ApiSession(apiSessionConfig, this.toolDefinitions, this);
    await this.session.connect();

    // Untrusted page context and history are sent as a priming turn
    // (Dynamic/Untrusted).
    let content = context?.content ?? '';
    const kMaxContentSize = 10000;
    if (content.length > kMaxContentSize) {
      content = content.substring(0, kMaxContentSize) +
          '... (truncated, use get_page_content for more)';
    }

    debugLog(FILE, DebugLogTag.PAGE_CONTENT, 'Provided Context', content);

    const primingTurn = buildContextPrimingTurn(
        context?.url || '', context?.title ?? '', content, transcript,
        pageHistory);

    this.session.sendContextUpdate(primingTurn);
  }


  private onPageContextChange(event: PageContextChangeEvent) {
    if (!this.connected) {
      return;
    }

    const newPageContentBecameAvailable =
        (event.type === PageContextChangeType.NEW_PAGE ||
         !event.oldContext?.hasHadContent) &&
        event.newContext.hasHadContent;

    if (newPageContentBecameAvailable && !!this.session) {
      this.session.stop();
      this.session = null;
      this.createNewApiSession();
    }
  }
}
