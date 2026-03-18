// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview AudioWorklet processor for capturing microphone audio.
 * Runs in a separate thread for lower latency.
 */

interface AudioWorkletProcessor {
  readonly port: MessagePort;
  process(
      inputs: Float32Array[][], outputs: Float32Array[][],
      parameters: Record<string, Float32Array>): boolean;
}

declare const AudioWorkletProcessor: {
  prototype: AudioWorkletProcessor,
  new (): AudioWorkletProcessor,
};

declare function registerProcessor(
    name: string, processorClass: new () => AudioWorkletProcessor): void;

class AudioProcessor extends AudioWorkletProcessor {
  private bufferSize: number;
  private buffer: Float32Array;
  private bufferIndex: number;

  constructor() {
    super();
    // Buffer size of 2048 samples at 16kHz is approx 128ms.
    // This reduces WebSocket message frequency from ~125Hz (every 8ms) to ~8Hz,
    // reducing CPU overhead and network congestion while maintaining acceptable
    // latency.
    // TODO(bokan): Consider making this parameterizable.
    this.bufferSize = 2048;
    this.buffer = new Float32Array(this.bufferSize);
    this.bufferIndex = 0;
  }

  override process(
      inputs: Float32Array[][], _outputs: Float32Array[][],
      _parameters: Record<string, Float32Array>): boolean {
    const input = inputs[0];

    if (input && input.length > 0) {
      const inputChannel = input[0];  // First channel (mono)

      if (inputChannel) {
        // Accumulate samples into buffer
        for (let i = 0; i < inputChannel.length; i++) {
          this.buffer[this.bufferIndex++] = inputChannel[i]!;

          // When buffer is full, send to main thread
          if (this.bufferIndex >= this.bufferSize) {
            this.flush();
          }
        }
      }
    }

    // Return true to keep the processor alive
    return true;
  }

  private flush() {
    // Send a copy of the buffer to the main thread
    const bufferToSend = this.buffer.slice(0, this.bufferSize);

    this.port.postMessage({type: 'audio', data: bufferToSend});

    // Reset for next batch
    this.bufferIndex = 0;
  }
}

registerProcessor('audio-processor', AudioProcessor);
