// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AudioPlayer} from './audio_player.js';
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

/**
 * An audio capturer that uses a blob audio file to capture audio. Used as a
 * development aid when a microphone isn't available or convenient.
 */
export class BlobAudioCapturer implements AudioCapturer {
  private static readonly SAMPLE_RATE = 16000;
  private audioContext: AudioContext|null = null;
  private onAudioCallback: ((data: string) => void)|null = null;
  private isStopped: boolean = false;

  private audioPlayer: AudioPlayer;

  constructor() {
    this.audioPlayer = new AudioPlayer();
  }

  /* Sets up internal state but doesn't actually play the audio. */
  start(onAudioCallback: (data: string) => void): Promise<boolean> {
    this.onAudioCallback = onAudioCallback;
    this.isStopped = false;
    this.audioContext =
        new AudioContext({sampleRate: BlobAudioCapturer.SAMPLE_RATE});

    return Promise.resolve(true);
  }

  stop() {
    this.isStopped = true;
    this.audioContext?.close();
    this.audioContext = null;
    this.onAudioCallback = null;
  }

  getSampleRate() {
    return BlobAudioCapturer.SAMPLE_RATE;
  }

  /* Sends the audio data to the callback. */
  async send(blob: Blob): Promise<void> {
    if (!this.onAudioCallback || !this.audioContext) {
      return;
    }

    const arrayBuffer = await blob.arrayBuffer();
    let audioBuffer: AudioBuffer;
    try {
      audioBuffer = await this.audioContext.decodeAudioData(arrayBuffer);
    } catch (e) {
      console.error('Failed to decode audio data', e);
      return;
    }

    console.info('Sending mock prompt');

    // Play back the audio so we can hear what's sent.
    this.audioPlayer.playBuffer(audioBuffer);

    const float32Data = audioBuffer.getChannelData(0);

    // 100ms at sample rate
    const chunkSize = BlobAudioCapturer.SAMPLE_RATE * 0.1;

    for (let i = 0; i < float32Data.length; i += chunkSize) {
      if (this.isStopped) {
        break;
      }

      const chunk = float32Data.slice(i, i + chunkSize);
      this.onAudioCallback?.(encodeFloat32ToPcmBase64(chunk));

      // Simulate real-time by waiting 100ms
      await new Promise(resolve => setTimeout(resolve, 100));
    }
    console.info('Finished sending mock prompt');
  }
}
