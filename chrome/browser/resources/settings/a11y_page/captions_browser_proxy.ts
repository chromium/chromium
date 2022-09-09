// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the Chrome captions section to
 * interact with the browser. Used on operating system that is not Chrome OS.
 */

export interface CaptionsBrowserProxy {
  /**
   * Open the native captions system dialog.
   */
  openSystemCaptionsDialog(): void;
}

export class CaptionsBrowserProxyImpl implements CaptionsBrowserProxy {
  openSystemCaptionsDialog() {
    chrome.send('openSystemCaptionsDialog');
  }

  static getInstance(): CaptionsBrowserProxy {
    return instance || (instance = new CaptionsBrowserProxyImpl());
  }

  static setInstance(obj: CaptionsBrowserProxy) {
    instance = obj;
  }
}

let instance: CaptionsBrowserProxy|null = null;
