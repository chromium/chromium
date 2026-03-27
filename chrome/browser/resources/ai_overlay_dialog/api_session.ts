// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kLogWebSocketMessages = false;

/**
 * API session WebSocket protocol types.
 */

export interface FunctionDeclaration {
  name: string;
  description?: string;
  parameters?: any;
}

export interface Tool {
  functionDeclarations?: FunctionDeclaration[];
}

export interface FunctionCall {
  id: string;
  name: string;
  args: any;
}

export interface ToolCall {
  functionCalls: FunctionCall[];
}

export interface FunctionResponse {
  id: string;
  name: string;
  response: any;
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
    mediaChunks: Array<{
      data: string,
      mimeType: string,
    }>,
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

export interface ApiConfig {
  endpointUrl: string;
  model: string;
  apiKey: string;
}

export interface ApiSessionDelegate {
  onResponse(audioData: string): void;
  onTranscription(text: string, isInput: boolean): void;
  onTurnComplete(): void;
  interrupt(): void;
  onConnectionChanged(connected: boolean): void;
  onToolCall(toolCall: ToolCall): void;
}

function log(msg: string, ...args: any[]) {
  console.info(
      `[${performance.now().toFixed(2)}] [ApiSession] ${msg}`, ...args);
}

/**
 * Manages the connection and communication with the server.
 */
export class ApiSession {
  private readonly systemInstruction: string;
  private readonly config: ApiConfig;
  private readonly toolDefinitions: Tool[];

  private ws: WebSocket|null = null;

  private delegate: ApiSessionDelegate;

  constructor(
      systemInstruction: string, config: ApiConfig, toolDefinitions: Tool[],
      delegate: ApiSessionDelegate) {
    this.systemInstruction = systemInstruction;
    this.config = config;
    this.toolDefinitions = toolDefinitions;
    this.delegate = delegate;
  }

  connect() {
    const url = `${this.config.endpointUrl}?key=${this.config.apiKey}`;
    this.ws = new WebSocket(url);

    this.ws.onopen = () => {
      log('WebSocket Opened');
      this.sendSetup();
    };

    this.ws.onmessage = async (event) => {
      let jsonPayload: any;
      if (event.data instanceof Blob) {
        try {
          const text = await event.data.text();
          jsonPayload = JSON.parse(text);
        } catch (e) {
          console.error('WebSocket Failed message decode: ', e);
          return;
        }
      } else if (typeof event.data === 'string') {
        jsonPayload = JSON.parse(event.data);
      }

      if (jsonPayload) {
        // Seeing all messages in the socket can be useful but is very verbose
        // so it's behind a bool to avoid flooding the console during normal
        // usage.
        if (kLogWebSocketMessages) {
          console.info(JSON.stringify(jsonPayload, (key, value) => {
            // Don't print the audio data so that the output is more easily
            // readable.
            return key === 'data' ? '<data>' : value;
          }, 2));
        }

        this.handleMessage(jsonPayload);
      }
    };

    this.ws.onclose = (e) => {
      log('WebSocket Closed: ', e);
      this.delegate.onConnectionChanged(false);
      this.stop();
    };

    this.ws.onerror = (error) => {
      console.error('WebSocket Error:', error);
      this.delegate.onConnectionChanged(false);
      this.stop();
    };
  }

  stop() {
    log('stop()');
    this.ws?.close();
    this.ws = null;
  }

  private sendSetup() {
    const setup: SetupMessage = {
      setup: {
        model: this.config.model,
        generationConfig: {
          responseModalities: ['AUDIO'],
          speechConfig: {
            voiceConfig: {prebuiltVoiceConfig: {voiceName: 'Puck'}},
          },
        },
        systemInstruction: {
          parts: [{
            text: this.systemInstruction,
          }],
        },
        tools: this.toolDefinitions,
        inputAudioTranscription: {},
        outputAudioTranscription: {},
      },
    };
    log('Sending Setup Message', setup);
    this.ws?.send(JSON.stringify(setup));
  }

  sendAudio(sampleRate: number, base64Data: string) {
    const msg: RealtimeInputMessage = {
      realtimeInput: {
        mediaChunks: [{
          data: base64Data,
          mimeType: `audio/pcm;rate=${sampleRate}`,
        }],
      },
    };
    this.ws?.send(JSON.stringify(msg));
  }

  sendToolResponse(responses: FunctionResponse[]) {
    const msg = {
      toolResponse: {
        functionResponses: responses.map(response => ({
                                           id: response.id,
                                           name: response.name,
                                           response: response.response,
                                         })),
      },
    };
    log('Sending Tool Response', msg);
    this.ws?.send(JSON.stringify(msg));
  }

  private handleMessage(msg: ServerContentMessage) {
    if (msg.setupComplete) {
      log('SetupComplete');
      this.delegate.onConnectionChanged(true);
      return;
    }

    if (msg.toolCall) {
      log('Received toolCall', msg.toolCall);
      this.delegate.onToolCall(msg.toolCall);
      return;
    }

    const content = msg.serverContent;
    if (!content) {
      return;
    }

    if (content.inputTranscription?.text) {
      log('Input transcription:', content.inputTranscription.text);
      this.delegate.onTranscription(content.inputTranscription.text, true);
    }

    if (content.outputTranscription?.text) {
      log('Output transcription:', content.outputTranscription.text);
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
      log('TurnComplete');
      this.delegate.onTurnComplete();
    }

    if (content.interrupted) {
      log('Interrupted');
      this.delegate.interrupt();
    }
  }
}
