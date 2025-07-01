// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to handle access to the clipboard data.
 */
import {Msgs} from '../../common/msgs.js';
import {OffscreenBridge} from '../../common/offscreen_bridge.js';
import type {ClipboardData} from '../../common/offscreen_bridge_constants.js';
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

  private async onClipboardDataChanged_(): Promise<void> {
    const data: ClipboardData = await OffscreenBridge.onClipboardDataChanged(
        this.forceReadNextClipboardEvent_);
    if (data.eventType === undefined) {
      return;
    }
    ChromeVox.tts.speak(
        Msgs.getMsg(data.eventType, [data.clipboardContent || '']),
        QueueMode.FLUSH);
    ChromeVoxRange.clearSelection();
    this.forceReadNextClipboardEvent_ = false;
  }

  readNextClipboardDataChange(): void {
    this.forceReadNextClipboardEvent_ = true;
  }
}
