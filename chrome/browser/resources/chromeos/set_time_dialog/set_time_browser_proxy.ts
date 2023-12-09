// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used by the "Set Time" dialog. */

export interface SetTimeBrowserProxy {
  /** Notifies C++ code that it's safe to call JS functions. */
  sendPageReady(): void;

  setTimeInSeconds(timeInSeconds: number): void;

  setTimezone(timezone: string): void;

  /** Closes the dialog. */
  dialogClose(): void;

  /**
   * Notifies C++ code that done button was clicked.
   * @param timeInSeconds Seconds since epoch representing the date
   *     on the dialog inputs.
   */
  doneClicked(timeInSeconds: number): void;
}

let instance: SetTimeBrowserProxy|null = null;

export class SetTimeBrowserProxyImpl implements SetTimeBrowserProxy {
  sendPageReady(): void {
    chrome.send('setTimePageReady');
  }

  setTimeInSeconds(timeInSeconds: number): void {
    chrome.send('setTimeInSeconds', [timeInSeconds]);
  }

  setTimezone(timezone: string): void {
    chrome.send('setTimezone', [timezone]);
  }

  dialogClose(): void {
    chrome.send('dialogClose');
  }

  doneClicked(timeInSeconds: number): void {
    chrome.send('doneClicked', [timeInSeconds]);
  }

  static getInstance(): SetTimeBrowserProxy {
    return instance || (instance = new SetTimeBrowserProxyImpl());
  }

  static setInstance(obj: SetTimeBrowserProxy): void {
    instance = obj;
  }
}
