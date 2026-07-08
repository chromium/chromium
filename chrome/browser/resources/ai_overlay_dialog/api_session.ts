// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {debugLog, DebugLogTag, errorLog, log, warnLog} from './logging.js';

const FILE = 'ApiSession';

/**
 * API session WebSocket protocol types.
 */

export interface FunctionDeclaration {
  name: string;
  description?: string;
  parameters?: Record<string, unknown>;
}

export interface Tool {
  functionDeclarations?: FunctionDeclaration[];
}

export interface FunctionCall {
  id: string;
  name: string;
  args: Record<string, unknown>;
}

export interface ToolCall {
  functionCalls: FunctionCall[];
}

export interface FunctionResponse {
  id: string;
  name: string;
  response: Record<string, unknown>;
  scheduling?: string;
}

interface SetupMessage {
  setup: {
    model: string,
    generationConfig: {
      responseModalities: string[],
      speechConfig: {voiceConfig: {prebuiltVoiceConfig: {voiceName: string}}},
    },
    systemInstruction?: {parts: Array<{text: string}>},
    tools?: Tool[],
    inputAudioTranscription?: Record<string, unknown>,
    outputAudioTranscription?: Record<string, unknown>,
  };
}

interface RealtimeInputMessage {
  realtimeInput: {audio: {data: string, mimeType: string}};
}

interface ClientContentMessage {
  clientContent: {
    turns: Array<{
      role: string,
      parts:
          Array<{text?: string, inlineData?: {data: string, mimeType: string}}>,
    }>,
    turnComplete?: boolean,
  };
}

interface ServerContentMessage {
  serverContent?: {
    modelTurn?: {
      parts?: Array<
               {inlineData?: {data: string, mimeType: string}, text?: string}>,
    },
    interrupted?: boolean,
    turnComplete?: boolean,
    inputTranscription?: {text?: string},
    outputTranscription?: {text?: string},
  };
  setupComplete?: Record<string, unknown>;
  toolCall?: ToolCall;
}

export interface ApiSessionConfig {
  endpointUrl: string;
  model: string;
  apiKey: string;
  systemInstruction: string;
  voiceName: string;
}

export interface ApiSessionDelegate {
  onResponse(audioData: string): void;
  onTranscription(text: string, isInput: boolean): void;
  onTurnComplete(): void;
  interrupt(): void;
  onConnectionChanged(connected: boolean): void;
  onToolCall(toolCall: ToolCall): void;
}

/**
 * Manages the connection and communication with the server.
 */
export class ApiSession {
  private readonly config: ApiSessionConfig;
  private readonly toolDefinitions: Tool[];

  private ws: WebSocket|null = null;

  // Buffers messages that are sent while the WebSocket is still in the
  // CONNECTING state. These are flushed as soon as the connection opens.
  private messageQueue: string[] = [];

  private delegate: ApiSessionDelegate;

  private setupCompletedCallback: (() => void)|null = null;

  constructor(
      config: ApiSessionConfig, toolDefinitions: Tool[],
      delegate: ApiSessionDelegate) {
    this.config = config;
    this.toolDefinitions = toolDefinitions;
    this.delegate = delegate;
  }

  // Setsup the WebSocket connection. Returns once the setup message has been
  // responded to and the connection is ready for input.
  async connect() {
    const url = `${this.config.endpointUrl}?key=${this.config.apiKey}`;
    log(FILE, `Connecting to WebSocket: ${this.config.endpointUrl}`);
    this.ws = new WebSocket(url);

    let setupCompletedReject: ((e: Error) => void)|null = null;
    const setupCompletedPromise = new Promise<void>((resolve, reject) => {
      this.setupCompletedCallback = () => {
        setupCompletedReject = null;
        resolve();
      };
      setupCompletedReject = reject;
    });

    this.ws.onopen = () => {
      log(FILE, 'WebSocket Opened');
      this.sendSetup();

      if (this.messageQueue.length > 0) {
        log(FILE, `Flushing ${this.messageQueue.length} queued messages`);
        for (const msg of this.messageQueue) {
          if (this.ws) {
            this.ws.send(msg);
          }
        }
        this.messageQueue = [];
      }
    };

    this.ws.onmessage = async (event) => {
      let jsonPayload: ServerContentMessage|null = null;
      let text = '';

      if (event.data instanceof Blob) {
        try {
          text = await event.data.text();
        } catch (e) {
          errorLog(FILE, 'Failed to decode Blob to text:', e);
          return;
        }
      } else if (typeof event.data === 'string') {
        text = event.data;
      }

      if (text) {
        try {
          jsonPayload = JSON.parse(text);
        } catch (parseError) {
          errorLog(FILE, 'JSON parse error:', parseError);
          return;
        }
      }

      if (jsonPayload) {
        debugLog(
            FILE, DebugLogTag.WEB_SOCKET_MSG,
            JSON.stringify(jsonPayload, (key, value) => {
              // Don't print the audio data so that the output is more easily
              // readable.
              return key === 'data' ? '<data>' : value;
            }, 2));

        this.handleMessage(jsonPayload);
      }
    };

    this.ws.onclose = (e) => {
      log(FILE,
          `WebSocket Closed: code=${e.code}, reason=${e.reason}, wasClean=${
              e.wasClean}`);
      if (setupCompletedReject) {
        setupCompletedReject(new Error(`WebSocket Closed: ${e.code}`));
        setupCompletedReject = null;
      }
      this.delegate.onConnectionChanged(false);
      this.stop();
    };

    this.ws.onerror = (error) => {
      errorLog(FILE, '[ApiSession] WebSocket Error:', error);
      if (setupCompletedReject) {
        setupCompletedReject(new Error('WebSocket Error'));
        setupCompletedReject = null;
      }
      this.delegate.onConnectionChanged(false);
      this.stop();
    };

    try {
      await setupCompletedPromise;
    } catch (e) {
      errorLog(FILE, 'setupCompletedPromise rejected:', e);
      throw e;
    }
  }

  stop() {
    log(FILE, 'stop()');
    this.ws?.close();
    this.ws = null;
    this.messageQueue = [];
  }

  private sendSetup() {
    const setup: SetupMessage = {
      setup: {
        model: this.config.model,
        generationConfig: {
          responseModalities: ['AUDIO'],
          speechConfig: {
            voiceConfig:
                {prebuiltVoiceConfig: {voiceName: this.config.voiceName}},
          },
        },
      },
    };

    if (this.config.systemInstruction &&
        this.config.systemInstruction.length > 0) {
      setup.setup.systemInstruction = {
        parts: [{
          text: this.config.systemInstruction,
        }],
      };
    }
    if (this.toolDefinitions && this.toolDefinitions.length > 0) {
      setup.setup.tools = this.toolDefinitions;
    }

    log(FILE, 'Sending Setup Message', setup);
    this.ws?.send(JSON.stringify(setup));
  }

  sendAudio(sampleRate: number, base64Data: string) {
    const msg: RealtimeInputMessage = {
      realtimeInput: {
        audio: {
          data: base64Data,
          mimeType: `audio/pcm;rate=${sampleRate}`,
        },
      },
    };
    const json = JSON.stringify(msg);
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(json);
    } else if (this.ws?.readyState === WebSocket.CONNECTING) {
      this.messageQueue.push(json);
    }
  }

  /**
   * Sends a context update as a user turn without marking it as complete.
   * This is used for "priming" the model with untrusted environment data.
   */
  sendContextUpdate(text: string) {
    const msg: ClientContentMessage = {
      clientContent: {
        turns: [{
          role: 'user',
          parts: [{text}],
        }],
        turnComplete: false,
      },
    };
    const json = JSON.stringify(msg);
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(json);
    } else if (this.ws?.readyState === WebSocket.CONNECTING) {
      this.messageQueue.push(json);
    }
  }

  sendText(text: string) {
    const msg: ClientContentMessage = {
      clientContent: {
        turns: [{
          role: 'user',
          parts: [{text}],
        }],
        turnComplete: true,
      },
    };
    log(FILE, 'Sending Text Message', msg);
    const json = JSON.stringify(msg);
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(json);
    } else if (this.ws?.readyState === WebSocket.CONNECTING) {
      this.messageQueue.push(json);
    } else {
      warnLog(
          FILE,
          '[ApiSession] Dropping text message because WebSocket is not OPEN ' +
              'or CONNECTING');
    }
  }

  sendToolResponse(responses: FunctionResponse[]) {
    const functionResponses: FunctionResponse[] = [];
    for (const response of responses) {
      if (response.scheduling !== undefined) {
        functionResponses.push({
          id: response.id,
          name: response.name,
          response: response.response,
          scheduling: response.scheduling,
        });
      } else {
        functionResponses.push({
          id: response.id,
          name: response.name,
          response: response.response,
        });
      }
    }

    const msg = {
      toolResponse: {
        functionResponses,
      },
    };
    log(FILE, 'Sending Tool Response', msg);
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(msg));
    } else {
      warnLog(
          FILE,
          '[ApiSession] Dropping tool response because WebSocket is not OPEN');
    }
  }

  private handleMessage(msg: ServerContentMessage) {
    // The top-level BidiGenerateContentServerMessage acts as a union and will
    // only contain exactly one of setupComplete, toolCall, or serverContent.
    if (msg.setupComplete) {
      log(FILE, 'SetupComplete received from server.');
      assert(this.setupCompletedCallback);
      this.setupCompletedCallback();
      this.delegate.onConnectionChanged(true);
      return;
    }

    if (msg.toolCall) {
      log(FILE, 'Received toolCall', msg.toolCall);
      this.delegate.onToolCall(msg.toolCall);
      return;
    }

    const content = msg.serverContent;
    if (!content) {
      return;
    }

    // Inside serverContent, multiple fields (like modelTurn and turnComplete)
    // can be present simultaneously, so we process each independently without
    // if/else chains.
    if (content.inputTranscription?.text) {
      log(FILE, 'Input transcription:', content.inputTranscription.text);
      this.delegate.onTranscription(content.inputTranscription.text, true);
    }

    if (content.outputTranscription?.text) {
      log(FILE, 'Output transcription:', content.outputTranscription.text);
      this.delegate.onTranscription(content.outputTranscription.text, false);
    }

    if (content.modelTurn?.parts) {
      for (const part of content.modelTurn?.parts) {
        if (part.inlineData) {
          this.delegate.onResponse(part.inlineData.data);
        }
      }
    }

    if (content.turnComplete) {
      log(FILE, 'TurnComplete');
      this.delegate.onTurnComplete();
    }

    if (content.interrupted) {
      log(FILE, 'Interrupted');
      this.delegate.interrupt();
    }
  }
}
