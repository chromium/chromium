// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InternalKeyEvent} from '../common/internal_key_event.js';
import {OffscreenCommandType} from '../common/offscreen_command_type.js';

/**
 * Offscreen analog to BackgroundKeyboardHandler.
 */
class OffscreenBackgroundKeyboardHandler {
  static instance?: OffscreenBackgroundKeyboardHandler;

  constructor() {
    document.addEventListener(
        'keydown', (event) => this.onKeyDown_(event), false);
    document.addEventListener('keyup', (event) => this.onKeyUp_(event), false);
  }

  static init(): void {
    if (OffscreenBackgroundKeyboardHandler.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'BackgroundKeyboardHandler.';
    }
    OffscreenBackgroundKeyboardHandler.instance =
        new OffscreenBackgroundKeyboardHandler();
  }

  /**
   * Handles key down events using the offscreen DOM and forwards them to the
   * ChromeVox service worker.
   */
  private onKeyDown_(evt: KeyboardEvent): void {
    this.sendKeyEventToServiceWorker_(OffscreenCommandType.ON_KEY_DOWN, evt);
  }

  /**
   * Handles key up events using the offscreen DOM and forwards them to the
   * ChromeVox service worker.
   */
  private onKeyUp_(evt: KeyboardEvent): void {
    this.sendKeyEventToServiceWorker_(OffscreenCommandType.ON_KEY_UP, evt);
  }


  private sendKeyEventToServiceWorker_(
      command: OffscreenCommandType, evt: KeyboardEvent) {
    const extensionId = undefined;
    const message = {command, internalEvent: new InternalKeyEvent(evt)};
    const options = undefined;
    const callback = (value: any) => {
      if (value as boolean) {
        evt.preventDefault();
        evt.stopPropagation();
      }
    };
    chrome.runtime.sendMessage(extensionId, message, options, callback);
  }
}

OffscreenBackgroundKeyboardHandler.init();
