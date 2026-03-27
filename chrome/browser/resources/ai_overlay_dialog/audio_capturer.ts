// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AudioPlayer} from './audio_player.js';
import {encodeFloat32ToPcmBase64} from './audio_utils.js';

function log(msg: string, ...args: any[]) {
  console.info(
      `[${performance.now().toFixed(2)}] [AudioCapturer] ${msg}`, ...args);
}

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
  // This promise is used to sequence multiple injections.
  private sendPromise: Promise<void> = Promise.resolve();
  // This is used to pause the silence loop while audio is being injected.
  private isInjecting: boolean = false;

  private audioPlayer: AudioPlayer;

  private get isStopped(): boolean {
    return !this.onAudioCallback;
  }

  constructor() {
    this.audioPlayer = new AudioPlayer();
  }

  /* Sets up internal state and outputs silence until audio data is provided. */
  start(onAudioCallback: (data: string) => void): Promise<boolean> {
    this.onAudioCallback = onAudioCallback;
    this.audioContext =
        new AudioContext({sampleRate: BlobAudioCapturer.SAMPLE_RATE});

    // Start a continuous loop of silence to mimic a real microphone.
    this.startSilenceLoop();

    return Promise.resolve(true);
  }

  private async startSilenceLoop() {
    const chunkMs = 250;  // Increased to 250ms to reduce empty packet spam
    const chunkSize = BlobAudioCapturer.SAMPLE_RATE * (chunkMs / 1000);
    const silentChunk = new Float32Array(chunkSize);
    const silentBase64 = encodeFloat32ToPcmBase64(silentChunk);

    log(`Started mock silence heartbeat (${chunkMs}ms)`);
    let nextTick = performance.now();

    while (!this.isStopped) {
      if (!this.isInjecting && this.onAudioCallback) {
        this.onAudioCallback(silentBase64);
      }

      nextTick += chunkMs;
      const delay = Math.max(0, nextTick - performance.now());
      await new Promise(resolve => setTimeout(resolve, delay));
    }
    log('Stopped mock silence heartbeat');
  }

  stop() {
    this.audioContext?.close();
    this.audioContext = null;
    this.onAudioCallback = null;
    this.isInjecting = false;
  }

  getSampleRate() {
    return BlobAudioCapturer.SAMPLE_RATE;
  }

  /* Sends the audio data to the callback. */
  async send(blob: Blob): Promise<void> {
    // Chain the send operation to the previous one to prevent interleaving.
    this.sendPromise = this.sendPromise.then(() => this.sendInternal(blob));
    return this.sendPromise;
  }

  private async sendInternal(blob: Blob): Promise<void> {
    if (!this.onAudioCallback || !this.audioContext) {
      log('BlobAudioCapturer not ready');
      return;
    }

    const arrayBuffer = await blob.arrayBuffer();
    log(`Decoding audio data of size ${arrayBuffer.byteLength}`);
    let audioBuffer: AudioBuffer;
    try {
      audioBuffer = await this.audioContext.decodeAudioData(arrayBuffer);
    } catch (e) {
      log('Failed to decode audio data', e);
      return;
    }

    log(`Injecting mock prompt: ${audioBuffer.duration.toFixed(2)}s, rate ${
        audioBuffer.sampleRate}Hz`);
    this.isInjecting = true;

    // Play back the audio so we can hear what's sent.
    this.audioPlayer.playBuffer(audioBuffer);

    const float32Data = audioBuffer.getChannelData(0);

    const chunkMs = 40;  // 40ms chunks for smoother delivery
    const chunkSize = BlobAudioCapturer.SAMPLE_RATE * (chunkMs / 1000);
    let chunksSent = 0;
    let nextTick = performance.now();

    for (let i = 0; i < float32Data.length; i += chunkSize) {
      if (this.isStopped) {
        log('Mock audio injection stopped early');
        break;
      }

      const chunk = float32Data.slice(i, i + chunkSize);
      this.onAudioCallback?.(encodeFloat32ToPcmBase64(chunk));
      chunksSent++;

      nextTick += chunkMs;
      const delay = Math.max(0, nextTick - performance.now());
      await new Promise(resolve => setTimeout(resolve, delay));
    }

    // Send a period of silence (e.g., 2 seconds) to ensure the server's Voice
    // Activity Detection (VAD) detects the end of speech, sent in realistic
    // chunks rather than all at once.
    const silenceDurationMs = 2000;
    const silenceChunks = Math.ceil(silenceDurationMs / chunkMs);
    const silentChunk = new Float32Array(chunkSize);
    const silentBase64 = encodeFloat32ToPcmBase64(silentChunk);

    for (let i = 0; i < silenceChunks; i++) {
      if (this.isStopped) {
        break;
      }
      this.onAudioCallback?.(silentBase64);
      nextTick += chunkMs;
      const delay = Math.max(0, nextTick - performance.now());
      await new Promise(resolve => setTimeout(resolve, delay));
    }

    log(`Finished injecting mock prompt, sent ${
        chunksSent} chunks. Resuming silence.`);
    this.isInjecting = false;
  }
}
