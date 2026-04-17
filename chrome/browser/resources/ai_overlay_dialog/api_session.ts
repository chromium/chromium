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
      speechConfig: {
        voiceConfig: {
          prebuiltVoiceConfig: {
            voiceName: string,
          },
        },
      },
    },
    systemInstruction?: {
      parts: Array<{text: string}>,
    },
    tools?: Tool[],
    inputAudioTranscription?: {},
    outputAudioTranscription?: {},
  };
}

interface RealtimeInputMessage {
  realtimeInput: {
    audio: {
      data: string,
      mimeType: string,
    },
  };
}

interface ServerContentMessage {
  serverContent?: {
    modelTurn?: {
      parts?: Array<{
             inlineData?: {
               data: string,
               mimeType: string,
             },
             text?: string,
           }>,
    },
    interrupted?: boolean,
    turnComplete?: boolean,
    inputTranscription?: {
      text?: string,
    },
    outputTranscription?: {
      text?: string,
    },
  };
  setupComplete?: {};
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

  // Buffers audio messages that are sent while the WebSocket is still in the
  // CONNECTING state. These are flushed as soon as the connection opens.
  private audioQueue: RealtimeInputMessage[] = [];

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
    this.ws = new WebSocket(url);

    const setupCompletedPromise =
        new Promise<void>(resolve => this.setupCompletedCallback = resolve);

    this.ws.onopen = () => {
      log(FILE, 'WebSocket Opened');
      this.sendSetup();

      if (this.audioQueue.length > 0) {
        log(FILE, `Flushing ${this.audioQueue.length} queued audio chunks`);
        for (const msg of this.audioQueue) {
          this.ws?.send(JSON.stringify(msg));
        }
        this.audioQueue = [];
      }
    };

    this.ws.onmessage = async (event) => {
      let jsonPayload: ServerContentMessage|null = null;
      if (event.data instanceof Blob) {
        try {
          const text = await event.data.text();
          jsonPayload = JSON.parse(text);
        } catch (e) {
          errorLog(FILE, 'WebSocket Failed message decode: ', e);
          return;
        }
      } else if (typeof event.data === 'string') {
        jsonPayload = JSON.parse(event.data);
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
      this.delegate.onConnectionChanged(false);
      this.stop();
    };

    this.ws.onerror = (error) => {
      errorLog(FILE, '[ApiSession] WebSocket Error:', error);
      this.delegate.onConnectionChanged(false);
      this.stop();
    };

    await setupCompletedPromise;
  }

  stop() {
    log(FILE, 'stop()');
    this.ws?.close();
    this.ws = null;
    this.audioQueue = [];
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
        systemInstruction: {
          parts: [{
            text: this.config.systemInstruction,
          }],
        },
        tools: this.toolDefinitions,
        inputAudioTranscription: {},
        outputAudioTranscription: {},
      },
    };
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
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(msg));
    } else if (this.ws?.readyState === WebSocket.CONNECTING) {
      this.audioQueue.push(msg);
    }
  }

  sendToolResponse(responses: FunctionResponse[]) {
    const msg = {
      toolResponse: {
        functionResponses: responses.map(response => ({
                                           id: response.id,
                                           name: response.name,
                                           response: response.response,
                                           scheduling: response.scheduling,
                                         })),
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
