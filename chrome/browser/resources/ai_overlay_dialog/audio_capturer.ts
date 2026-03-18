// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {encodeFloat32ToPcmBase64} from './audio_utils.js';

/**
 * Interface for audio capture.
 */
export interface AudioCapturer {
  start(onAudioCallback: (data: string) => void): Promise<boolean>;
  stop(): void;
  getSampleRate(): number;
}

/**
 * Handles audio capture from the microphone.
 */
export class MicrophoneAudioCapturer implements AudioCapturer {
  private audioContext: AudioContext|null = null;
  private stream: MediaStream;
  private processor: AudioWorkletNode|null = null;
  private onAudioCallback: ((data: string) => void)|null = null;
  private readonly sampleRate: number;

  constructor(stream: MediaStream, sampleRate: number = 16000) {
    this.sampleRate = sampleRate;
    this.stream = stream;
  }

  async start(onAudioCallback: (data: string) => void): Promise<boolean> {
    this.onAudioCallback = onAudioCallback;
    this.audioContext = new AudioContext({sampleRate: this.sampleRate});

    await this.audioContext.audioWorklet.addModule('audio_processor.js');

    const source = this.audioContext.createMediaStreamSource(this.stream);
    this.processor = new AudioWorkletNode(this.audioContext, 'audio-processor');

    this.processor.port.onmessage = (event) => {
      if (event.data.type === 'audio') {
        this.sendAudio(event.data.data);
      }
    };

    source.connect(this.processor);
    this.processor.connect(this.audioContext.destination);

    return true;
  }

  stop() {
    this.processor?.disconnect();
    this.stream.getTracks().forEach(track => track.stop());
    this.audioContext?.close();
    this.processor = null;
    this.audioContext = null;
    this.onAudioCallback = null;
  }

  getSampleRate(): number {
    return this.sampleRate;
  }

  private sendAudio(float32Data: Float32Array) {
    this.onAudioCallback?.(encodeFloat32ToPcmBase64(float32Data));
  }
}
