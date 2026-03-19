// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {AudioCapturer} from './audio_capturer.js';
import {AudioPlayer} from './audio_player.js';

/**
 * States for the API session.
 * TODO(bokan): This doesn't belong here long term but will be moved once we
 * have a more appropriate coordinator object.
 */
export enum SessionState {
  IDLE = 'idle',
  LISTENING = 'listening',
  TALKING = 'talking',
}

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

/**
 * Manages the connection and communication with the server.
 */
export class ApiSession {
  private readonly apiKey: string;
  private readonly systemInstruction: string;
  private readonly audioCapturer: AudioCapturer;
  private readonly audioPlayer: AudioPlayer;

  private ws: WebSocket|null = null;
  // TODO(bokan): the session shouldn't interact with state, the coordinator
  // should be responsible for this based on the incoming messages.
  private onStateChange: (state: SessionState) => void;
  private config_: ApiConfig|null = null;

  constructor(
      apiKey: string, systemInstruction: string,
      audioCapturer: AudioCapturer, audioPlayer: AudioPlayer,
      onStateChange: (state: SessionState) => void) {
    this.apiKey = apiKey;
    this.systemInstruction = systemInstruction;
    this.audioCapturer = audioCapturer;
    this.audioPlayer = audioPlayer;
    this.onStateChange = onStateChange;
    // TODO(bokan): 24000 Hz (the default sampleRate in AudioPlayer) happens to
    // be what we receive from the server but we should be looking at the value
    // on the mime type and recreate the AudioPlayer if necessary.
    this.audioPlayer =
        new AudioPlayer(onStateChange.bind(this, SessionState.LISTENING));
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

    this.ws.onopen = async () => {
      console.info('WebSocket Opened');
      this.onStateChange(SessionState.LISTENING);
      this.sendSetup();

      await this.audioCapturer.start(
          this.sendAudio.bind(this, this.audioCapturer.getSampleRate()));
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
      this.stop();
    };

    this.ws.onerror = (error) => {
      console.error('API WebSocket error:', error);
      this.stop();
    };
  }

  stop() {
    console.info('stop()');
    this.audioCapturer.stop();
    this.audioPlayer.stop();
    this.ws?.close();
    this.ws = null;
    this.onStateChange(SessionState.IDLE);
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

  private sendAudio(sampleRate: number, base64Data: string) {
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
    const content = msg.serverContent;
    if (!content) {
      return;
    }

    if (content.modelTurn?.parts) {
      for (const part of content.modelTurn?.parts) {
        if (part.inlineData) {
          this.onStateChange(SessionState.TALKING);
          this.audioPlayer.play(part.inlineData.data);
        }
      }
    }

    if (content.interrupted) {
      this.onStateChange(SessionState.LISTENING);
      this.audioPlayer.stop();
    }
  }
}
