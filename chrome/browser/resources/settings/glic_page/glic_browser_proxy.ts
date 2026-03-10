// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * Information for a login permission for a given site.
 */
export interface LoginPermission {
  signonRealm: string;
  username?: string;
  displayName: string;
  faviconUrl: string;
}

export interface GlicBrowserProxy {
  setGlicOsLauncherEnabled(enabled: boolean): void;
  getGlicShortcut(): Promise<string>;
  setGlicShortcut(shortcut: string): Promise<void>;
  getGlicFocusToggleShortcut(): Promise<string>;
  setGlicFocusToggleShortcut(shortcut: string): Promise<void>;
  setShortcutSuspensionState(isSuspended: boolean): void;
  getDisallowedByAdmin(): Promise<boolean>;
  /**
   * Get the list of actor login permissions.
   */
  getActorLoginPermissions(): Promise<LoginPermission[]>;
  /**
   * Revoke actor login permission for a given signonRealm.
   * @param signonRealm The signon realm for which to revoke the permission.
   */
  revokeActorLoginPermission(signonRealm: string): void;
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

  getGlicFocusToggleShortcut() {
    return sendWithPromise('getGlicFocusToggleShortcut');
  }

  setGlicFocusToggleShortcut(shortcut: string) {
    return sendWithPromise('setGlicFocusToggleShortcut', shortcut);
  }

  setShortcutSuspensionState(shouldSuspend: boolean) {
    chrome.send('setShortcutSuspensionState', [shouldSuspend]);
  }

  getDisallowedByAdmin() {
    return sendWithPromise('getGlicDisallowedByAdmin');
  }

  getActorLoginPermissions() {
    return sendWithPromise('getActorLoginPermissions');
  }

  revokeActorLoginPermission(signonRealm: string) {
    chrome.send('revokeActorLoginPermission', [signonRealm]);
  }

  static getInstance(): GlicBrowserProxy {
    return instance || (instance = new GlicBrowserProxyImpl());
  }

  static setInstance(obj: GlicBrowserProxy) {
    instance = obj;
  }
}

let instance: GlicBrowserProxy|null = null;
