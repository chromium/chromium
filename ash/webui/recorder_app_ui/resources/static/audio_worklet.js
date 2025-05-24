// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* global AudioWorkletProcessor */
/* global registerProcessor */
/**
 * Processor that sends audio slice raw data to main thread via port.
 */
class AudioProcessor extends AudioWorkletProcessor {
  /**
   * @param {Float32Array[][]} inputs An array of inputs connected to the node,
   *     each item of which is, in turn, an array of channels. Each channel is
   *     a Float32Array containing 128 samples.
   *     See
   * https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletProcessor/process
   *     for reference.
   */
  process(inputs) {
    // Get audio samples from input. This only use the first channel of the
    // first input.
    // TODO(pihsun): Supports multiple channels.
    const samples = inputs[0][0];

    // The samples can be undefined when no input is connected.
    if (samples !== undefined) {
      // Send samples to main thread via port
      this.port.postMessage(samples);
    }

    return true;
  }
}

registerProcessor('audio-processor', AudioProcessor);
