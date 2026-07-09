// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handles audio playback.
 */
export class AudioPlayer {
  private readonly sampleRate: number;
  private audioContext: AudioContext|null = null;
  private analyser: AnalyserNode|null = null;
  private nextStartTime: number = 0;
  private sources: AudioBufferSourceNode[] = [];

  private onDone?: () => void;
  private onStart?: () => void;
  private doneTimeout: number = 0;

  constructor(
      onStart?: () => void, onDone?: () => void, sampleRate: number = 24000) {
    this.onDone = onDone;
    this.onStart = onStart;
    this.sampleRate = sampleRate;
    this.audioContext = new AudioContext({sampleRate: this.sampleRate});
    this.analyser = this.audioContext.createAnalyser();
    this.analyser.fftSize = 256;
  }

  getEnergy(): number {
    if (!this.analyser) {
      return 0;
    }
    const dataArray = new Float32Array(this.analyser.fftSize);
    this.analyser.getFloatTimeDomainData(dataArray);
    let sumSquares = 0.0;
    for (let i = 0; i < dataArray.length; i++) {
      const val = dataArray[i]!;
      sumSquares += val * val;
    }
    return Math.sqrt(sumSquares / dataArray.length);
  }

  play(base64Data: string) {
    if (!this.audioContext) {
      return;
    }

    // Decode Base64 to Int16 PCM
    const binaryString = atob(base64Data);
    const bytes = new Uint8Array(binaryString.length);
    for (let i = 0; i < binaryString.length; i++) {
      bytes[i] = binaryString.charCodeAt(i);
    }

    const int16Data = new Int16Array(bytes.buffer);
    const float32Data = new Float32Array(int16Data.length);
    for (let i = 0; i < int16Data.length; i++) {
      float32Data[i] = int16Data[i]! / 32768;
    }

    // Create AudioBuffer and play
    const buffer =
        this.audioContext.createBuffer(1, float32Data.length, this.sampleRate);
    buffer.getChannelData(0).set(float32Data);

    this.playBuffer(buffer);
  }

  playBuffer(buffer: AudioBuffer) {
    if (!this.audioContext || !this.analyser) {
      return;
    }

    const source = this.audioContext.createBufferSource();
    source.buffer = buffer;
    source.connect(this.analyser);
    source.connect(this.audioContext.destination);

    source.onended = () => {
      const index = this.sources.indexOf(source);
      if (index > -1) {
        this.sources.splice(index, 1);
      }
    };
    this.sources.push(source);

    const now = this.audioContext.currentTime;
    if (this.nextStartTime < now) {
      if (this.onStart) {
        this.onStart();
      }
      this.nextStartTime = now;
    }

    source.start(this.nextStartTime);
    this.nextStartTime += buffer.duration;

    if (this.onDone) {
      clearTimeout(this.doneTimeout);
      this.doneTimeout =
          setTimeout(this.onDone, (this.nextStartTime - now) * 1000);
    }
  }

  stop() {
    if (this.onDone) {
      this.onDone();
    }
    for (const source of this.sources) {
      source.disconnect();
      source.stop();
    }
    this.sources = [];
    this.nextStartTime = 0;
    clearTimeout(this.doneTimeout);
  }
}
