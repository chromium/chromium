// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type MessageSender = chrome.runtime.MessageSender;

let AudioAndCopyHandlerObject;

// Number of milliseconds to wait after requesting a clipboard read
// before clipboard change and paste events are ignored.
const kClipboardReadMaxDelayMs = 1000;

// Number of milliseconds to wait after requesting a clipboard copy
// before clipboard copy events are ignored, used to clear the clipboard
// after reading data in a paste event.
const kClipboardClearMaxDelayMs = 500;

class AudioAndCopyHandler {
  private audioElement_: HTMLAudioElement;

  private lastClearClipboardDataTime_: Date;
  private lastReadClipboardDataTime_: Date;

  constructor() {
    this.audioElement_ = new Audio('earcons/null_selection.ogg');

    /**
     * The timestamp at which the last clipboard data clear was requested.
     * Used to make sure we don't clear the clipboard on a user's request,
     * but only after the clipboard was used to read selected text.
     */
    this.lastClearClipboardDataTime_ = new Date(0);

    /**
     * The timestamp at which clipboard data read was requested by the user
     * doing a "read selection" keystroke on a Google Docs app. If a
     * clipboard change event comes in within kClipboardReadMaxDelayMs,
     * Select-to-Speak will read that text out loud.
     */
    this.lastReadClipboardDataTime_ = new Date(0);

    document.addEventListener('paste', evt => {
      this.onClipboardPaste_(evt);
    });

    document.addEventListener('copy', evt => {
      this.onClipboardCopy_(evt);
    });

    // Handle messages from the service worker.
    chrome.runtime.onMessage.addListener(
        (message: any|undefined, _sender: MessageSender,
         _sendResponse: (value: any) => void) => {
          switch (message['command']) {
            case 'playNullSelectionTone':
              this.audioElement_.play();
              break;
            case 'updateLastReadClipboardDataTime':
              this.lastReadClipboardDataTime_ = new Date();
              break;
            case 'clipboardDataChanged':
              this.onClipboardDataChanged_();
              break;
          }
          return false;
        });
  }

  private onClipboardDataChanged_(): void {
    if (new Date().getTime() - this.lastReadClipboardDataTime_.getTime() <
        kClipboardReadMaxDelayMs) {
      // The data has changed, and we are ready to read it.
      // Get it using a paste.
      document.execCommand('paste');
    }
  }

  private onClipboardPaste_(evt: ClipboardEvent): void {
    if (new Date().getTime() - this.lastReadClipboardDataTime_.getTime() <
        kClipboardReadMaxDelayMs) {
      // Read the current clipboard data.
      evt.preventDefault();
      this.lastReadClipboardDataTime_ = new Date(0);
      // Clear the clipboard data by copying nothing (the current document).
      // Do this in a timeout to avoid a recursive warning per
      // https://crbug.com/363288.
      setTimeout(() => this.clearClipboard_(), 0);

      // @ts-ignore: TODO(crbug.com/270623046): clipboardData can be null.
      let content = evt.clipboardData.getData('text/plain');
      chrome.runtime.sendMessage(undefined, {
        command: 'paste',
        content,
      });
    }
  }

  private onClipboardCopy_(evt: ClipboardEvent): void {
    if (new Date().getTime() - this.lastClearClipboardDataTime_.getTime() <
        kClipboardClearMaxDelayMs) {
      // onClipboardPaste has just completed reading the clipboard for speech.
      // This is used to clear the clipboard.
      // @ts-ignore: TODO(crbug.com/270623046): clipboardData can be null.
      evt.clipboardData.setData('text/plain', '');
      evt.preventDefault();
      this.lastClearClipboardDataTime_ = new Date(0);
    }
  }

  private clearClipboard_(): void {
    this.lastClearClipboardDataTime_ = new Date();
    document.execCommand('copy');
  }
}

document.addEventListener('DOMContentLoaded', () => {
  AudioAndCopyHandlerObject = new AudioAndCopyHandler();
});
