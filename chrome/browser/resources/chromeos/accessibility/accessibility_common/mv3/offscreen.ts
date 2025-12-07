// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OffscreenAudio} from './dictation/offscreen_audio.js';
import {OffscreenPumpkinWorker} from './dictation/offscreen_pumpkin_worker.js';
import {OffscreenWebCam} from './facegaze/offscreen_web_cam.js';
import {Messenger} from './messenger.js';

/**
 * Offscreen to manage all offscreen processors in the extension.
 */
class Offscreen {
  static instance?: Offscreen;

  private offscreens_: Set<Object> = new Set();

  constructor() {
    this.offscreens_.add(new OffscreenAudio());
    this.offscreens_.add(new OffscreenPumpkinWorker());
    this.offscreens_.add(new OffscreenWebCam());
  }

  static init(): void {
    if (Offscreen.instance) {
      throw 'Error: trying to create two instances of singleton Offscreen.';
    }
    Messenger.init(Messenger.Context.OFFSCREEN);
    Offscreen.instance = new Offscreen();
  }
}

Offscreen.init();
