// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';


/**
 * API session WebSocket protocol types.
 */
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
  };
  setupComplete?: {};
}

interface ApiConfig {
  endpoint_url: string;
  model: string;
}

export interface ApiSessionDelegate {
  onResponse(audioData: string): void;
  interrupt(): void;
  onConnectionChanged(connected: boolean): void;
}

/**
 * Manages the connection and communication with the server.
 */
export class ApiSession {
  private readonly apiKey: string;
  private readonly systemInstruction: string;

  private ws: WebSocket|null = null;
  private config_: ApiConfig|null = null;

  private delegate: ApiSessionDelegate;

  constructor(
      apiKey: string, systemInstruction: string, delegate: ApiSessionDelegate) {
    this.apiKey = apiKey;
    this.systemInstruction = systemInstruction;
    this.delegate = delegate;
  }

  async connect() {
    if (!this.config_) {
      try {
        const response = await fetch('api_config.json');
        this.config_ = await response.json();
      } catch (e) {
        console.error('Failed to load api_config.json', e);
        this.stop();
        return;
      }
    }

    assert(this.config_);
    const url = `${this.config_.endpoint_url}?key=${this.apiKey}`;
    this.ws = new WebSocket(url);

    this.ws.onopen = () => {
      console.info('WebSocket Opened');
      this.sendSetup();
    };

    this.ws.onmessage = async (event) => {
      let jsonPayload: any;
      if (event.data instanceof Blob) {
        try {
          const text = await event.data.text();
          jsonPayload = JSON.parse(text);
        } catch (e) {
          console.error('Failed message decode: ', e);
          return;
        }
      } else if (typeof event.data === 'string') {
        jsonPayload = JSON.parse(event.data);
      }

      if (jsonPayload) {
        this.handleMessage(jsonPayload);
      }
    };

    this.ws.onclose = (e) => {
      console.info('WebSocket Closed: ', e);
      this.delegate.onConnectionChanged(false);
      this.stop();
    };

    this.ws.onerror = (error) => {
      console.error('API WebSocket error:', error);
      this.delegate.onConnectionChanged(false);
      this.stop();
    };
  }

  stop() {
    console.info('stop()');
    this.ws?.close();
    this.ws = null;
  }

  private sendSetup() {
    assert(this.config_);
    const setup: SetupMessage = {
      setup: {
        model: this.config_.model,
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
      },
    };
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

  private handleMessage(msg: ServerContentMessage) {
    if (msg.setupComplete) {
      console.info('ApiSession SetupComplete');
      this.delegate.onConnectionChanged(true);
      return;
    }

    const content = msg.serverContent;
    if (!content) {
      return;
    }

    if (content.modelTurn?.parts) {
      for (const part of content.modelTurn?.parts) {
        if (part.inlineData) {
          this.delegate.onResponse(part.inlineData.data);
        }
      }
    }

    if (content.interrupted) {
      this.delegate.interrupt();
    }
  }
}
