// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface GlicBrowserProxy {
  setGlicOsLauncherEnabled(enabled: boolean): void;
  getGlicShortcut(): Promise<string>;
  setGlicShortcut(shortcut: string): Promise<void>;
  setShortcutSuspensionState(isSuspended: boolean): void;
}

export class GlicBrowserProxyImpl implements GlicBrowserProxy {
  setGlicOsLauncherEnabled(enabled: boolean) {
    chrome.send('setGlicOsLauncherEnabled', [enabled]);
  }

  getGlicShortcut() {
    return sendWithPromise('getGlicShortcut');
  }

  setGlicShortcut(shortcut: string) {
    return sendWithPromise('setGlicShortcut', shortcut);
  }

  setShortcutSuspensionState(shouldSuspend: boolean) {
    chrome.send('setShortcutSuspensionState', [shouldSuspend]);
  }

  static getInstance(): GlicBrowserProxy {
    return instance || (instance = new GlicBrowserProxyImpl());
  }

  static setInstance(obj: GlicBrowserProxy) {
    instance = obj;
  }
}

let instance: GlicBrowserProxy|null = null;
