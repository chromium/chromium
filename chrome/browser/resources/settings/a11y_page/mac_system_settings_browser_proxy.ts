// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface MacSystemSettingsBrowserProxy {
  openTrackpadGesturesSettings(): void;
}

export class MacSystemSettingsBrowserProxyImpl implements
    MacSystemSettingsBrowserProxy {
  openTrackpadGesturesSettings() {
    chrome.send('openTrackpadGesturesSettings');
  }

  static getInstance(): MacSystemSettingsBrowserProxy {
    return instance || (instance = new MacSystemSettingsBrowserProxyImpl());
  }

  static setInstance(obj: MacSystemSettingsBrowserProxy) {
    instance = obj;
  }
}

let instance: MacSystemSettingsBrowserProxy|null = null;
