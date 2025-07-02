// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Messenger} from '../messenger.js';
import {OffscreenCommandType} from '../offscreen_command_type.js';

/**
 * Offscreen way to play sounds for dictaiton.
 */
class OffscreenAudio {
  private cancelTone_: HTMLAudioElement =
      new Audio('dictation/earcons/null_selection.wav');
  private startTone_: HTMLAudioElement =
      new Audio('dictation/earcons/audio_initiate.wav');
  private endTone_: HTMLAudioElement =
      new Audio('dictation/earcons/audio_end.wav');

  constructor() {
    Messenger.registerHandler(
        OffscreenCommandType.DICTATION_PLAY_CANCEL,
        () => this.cancelTone_.play());
    Messenger.registerHandler(
        OffscreenCommandType.DICTATION_PLAY_START,
        () => this.startTone_.play());
    Messenger.registerHandler(
        OffscreenCommandType.DICTATION_PLAY_END, () => this.endTone_.play());
  }
}

export {OffscreenAudio};
