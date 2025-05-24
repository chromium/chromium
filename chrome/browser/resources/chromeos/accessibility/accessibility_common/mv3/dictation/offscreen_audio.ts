// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OffscreenCommandType} from '../offscreen_command_type.js';

/**
 * Offscreen way to play sounds for dictaiton.
 */
class OffscreenAudio {
  static instance?: OffscreenAudio;

  private cancelTone_: HTMLAudioElement =
      new Audio('earcons/null_selection.wav');
  private startTone_: HTMLAudioElement =
      new Audio('earcons/audio_initiate.wav');
  private endTone_: HTMLAudioElement = new Audio('earcons/audio_end.wav');

  constructor() {
    chrome.runtime.onMessage.addListener(
        (message: any|undefined, _sender: chrome.runtime.MessageSender) =>
            this.handleMessageFromServiceWorker_(message));
  }

  static init(): void {
    if (OffscreenAudio.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'OffscreenAudio.';
    }
    OffscreenAudio.instance = new OffscreenAudio();
  }

  private handleMessageFromServiceWorker_(message: any|undefined): boolean {
    switch (message['command']) {
      case OffscreenCommandType.DICTATION_PLAY_CANCEL:
        this.cancelTone_.play();
        break;
      case OffscreenCommandType.DICTATION_PLAY_START:
        this.startTone_.play();
        break;
      case OffscreenCommandType.DICTATION_PLAY_END:
        this.endTone_.play();
        break;
    }
    return false;
  }
}

OffscreenAudio.init();
