// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to handle access to the clipboard data.
 */
import {Msgs} from '../../common/msgs.js';
import {QueueMode} from '../../common/tts_types.js';

import {ChromeVox} from '../chromevox.js';
import {ChromeVoxRange} from '../chromevox_range.js';


/** Handles accessing and tracking access to the clipboard. */
export class ClipboardHandler {
  private lastClipboardEvent_?: string;
  static instance: ClipboardHandler;

  static init(): void {
    ClipboardHandler.instance = new ClipboardHandler();

    chrome.clipboard.onClipboardDataChanged.addListener(
        () => ClipboardHandler.instance.onClipboardDataChanged_());

    document.addEventListener(
        'copy',
        event => ClipboardHandler.instance.onClipboardCopyEvent_(event));
  }

  /** Processes the copy clipboard event. */
  private onClipboardCopyEvent_(evt: ClipboardEvent): void {
    // This should always be 'copy', but is still important to set for the below
    // extension event.
    this.lastClipboardEvent_ = evt.type;
  }

  private onClipboardDataChanged_(): void {
    // A DOM-based clipboard event always comes before this Chrome extension
    // clipboard event. We only care about 'copy' events, which gets set above.
    if (!this.lastClipboardEvent_) {
      return;
    }

    const eventType = this.lastClipboardEvent_;
    this.lastClipboardEvent_ = undefined;

    const textarea = document.createElement('textarea');
    document.body.appendChild(textarea);
    textarea.focus();
    document.execCommand('paste');
    const clipboardContent = textarea.value;
    textarea.remove();
    ChromeVox.tts.speak(
        Msgs.getMsg(eventType, [clipboardContent]), QueueMode.FLUSH);
    ChromeVoxRange.clearSelection();
  }

  readNextClipboardDataChange(): void {
    this.lastClipboardEvent_ = 'copy';
  }
}
