// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from '../assert.js';

import {
  StreamConstraints,
  toMediaStreamConstraints,
} from './stream_constraints.js';

/**
 * The singleton instance of StreamManagerChrome. Initialized by the first
 * invocation of getInstance().
 */
let instance: StreamManagerChrome|null = null;

/**
 * Monitors device change and provides different listener callbacks for
 * device changes. It also provides streams for different modes.
 */
export class StreamManagerChrome {
  private captureStream: MediaStream|null = null;

  static getInstance(): StreamManagerChrome {
    if (instance === null) {
      instance = new StreamManagerChrome();
    }
    return instance;
  }

  async prepare(constraints: StreamConstraints): Promise<void> {
    this.captureStream = await navigator.mediaDevices.getUserMedia(
        toMediaStreamConstraints(constraints));
  }

  getCaptureStream(): MediaStream|null {
    return this.captureStream;
  }

  stopCaptureStream(): void {
    if (this.captureStream !== null) {
      assertExists(this.captureStream.getVideoTracks()[0]).stop();
      this.captureStream.getAudioTracks()[0]?.stop();
      this.captureStream = null;
    }
  }
}
