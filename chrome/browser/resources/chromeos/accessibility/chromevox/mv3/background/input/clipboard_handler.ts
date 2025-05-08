// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to handle access to the clipboard data.
 */
import {Msgs} from '../../common/msgs.js';
import {OffscreenCommandType} from '../../common/offscreen_command_type.js';
import {QueueMode} from '../../common/tts_types.js';
import {ChromeVox} from '../chromevox.js';
import {ChromeVoxRange} from '../chromevox_range.js';

/** Handles accessing and tracking access to the clipboard. */
export class ClipboardHandler {
  // When forceReadNextClipboardEvent_ is true, copied data is spoken after a
  // chrome.clipboard.onClipboardDataChanged regardless of whether a DOM-based
  // clipboard 'copy' event recently occurred. See OffscreenClipboardHandler's
  // onClipboardDataChanged_.
  private forceReadNextClipboardEvent_ = false;
  static instance: ClipboardHandler;

  static init(): void {
    ClipboardHandler.instance = new ClipboardHandler();

    chrome.clipboard.onClipboardDataChanged.addListener(
        () => ClipboardHandler.instance.onClipboardDataChanged_());
  }

  private onClipboardDataChanged_(): void {
    chrome.runtime.sendMessage(
        undefined, {
          command: OffscreenCommandType.ON_CLIPBOARD_DATA_CHANGED,
          forceRead: this.forceReadNextClipboardEvent_
        },
        undefined, this.readClipboardContent_);
    this.forceReadNextClipboardEvent_ = false;
  }

  private readClipboardContent_(value: any): void {
    ChromeVox.tts.speak(
        Msgs.getMsg(value.eventType, [value.clipboardContent]),
        QueueMode.FLUSH);
    ChromeVoxRange.clearSelection();
  }

  readNextClipboardDataChange(): void {
    this.forceReadNextClipboardEvent_ = true;
  }
}
