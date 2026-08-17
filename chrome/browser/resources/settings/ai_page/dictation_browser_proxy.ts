// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * Proxy for interacting with the browser from the Dictation settings page.
 */
export interface DictationBrowserProxy {
  getDictationShortcut(): Promise<string>;
  setDictationShortcut(shortcut: string): Promise<boolean>;
}

/**
 * Default implementation of DictationBrowserProxy.
 */
export class DictationBrowserProxyImpl implements DictationBrowserProxy {
  getDictationShortcut(): Promise<string> {
    return sendWithPromise('getDictationShortcut');
  }

  setDictationShortcut(shortcut: string): Promise<boolean> {
    return sendWithPromise('setDictationShortcut', shortcut);
  }

  static getInstance(): DictationBrowserProxy {
    return instance || (instance = new DictationBrowserProxyImpl());
  }

  static setInstance(obj: DictationBrowserProxy) {
    instance = obj;
  }
}

let instance: DictationBrowserProxy|null = null;
