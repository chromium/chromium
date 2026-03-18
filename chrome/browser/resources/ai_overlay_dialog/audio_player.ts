// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handles audio playback.
 */
export class AudioPlayer {
  private readonly sampleRate: number;
  private audioContext: AudioContext|null = null;
  private nextStartTime: number = 0;

  private onDone?: () => void;
  private doneTimeout: number = 0;

  constructor(onDone?: () => void, sampleRate: number = 24000) {
    this.onDone = onDone;
    this.sampleRate = sampleRate;
    this.audioContext = new AudioContext({sampleRate: this.sampleRate});
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
    if (!this.audioContext) {
      return;
    }

    const source = this.audioContext.createBufferSource();
    source.buffer = buffer;
    source.connect(this.audioContext.destination);

    const now = this.audioContext.currentTime;
    if (this.nextStartTime < now) {
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
    this.audioContext?.close();
    this.audioContext = new AudioContext({sampleRate: this.sampleRate});
    this.nextStartTime = 0;
  }
}
