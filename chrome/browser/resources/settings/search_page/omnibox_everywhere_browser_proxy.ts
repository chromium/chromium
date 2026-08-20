// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface OmniboxEverywhereBrowserProxy {
  getOmniboxEverywhereShortcut(): Promise<string>;
  setOmniboxEverywhereShortcut(shortcut: string): Promise<boolean>;
  setOmniboxEverywhereShortcutSuspensionState(shouldSuspend: boolean): void;
}

export class OmniboxEverywhereBrowserProxyImpl implements
    OmniboxEverywhereBrowserProxy {
  getOmniboxEverywhereShortcut(): Promise<string> {
    return sendWithPromise<string>('getOmniboxEverywhereShortcut');
  }

  setOmniboxEverywhereShortcut(shortcut: string): Promise<boolean> {
    return sendWithPromise<boolean>('setOmniboxEverywhereShortcut', shortcut);
  }

  setOmniboxEverywhereShortcutSuspensionState(shouldSuspend: boolean) {
    chrome.send('setOmniboxEverywhereShortcutSuspensionState', [shouldSuspend]);
  }

  static getInstance(): OmniboxEverywhereBrowserProxy {
    return instance || (instance = new OmniboxEverywhereBrowserProxyImpl());
  }

  static setInstance(obj: OmniboxEverywhereBrowserProxy) {
    instance = obj;
  }
}

let instance: OmniboxEverywhereBrowserProxy|null = null;
