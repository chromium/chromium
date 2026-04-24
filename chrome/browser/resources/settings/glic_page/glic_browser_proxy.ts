// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * Information for a login permission for a given site.
 */
export interface LoginPermission {
  signonRealm: string;
  username: string;
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
  revokeActorLoginPermission(signonRealm: string, username: string):
      Promise<boolean>;
  getGlicSelectionShortcut(): Promise<string>;
  setGlicSelectionShortcut(shortcut: string): Promise<void>;
  getWebActuationToggleVisibility(): Promise<boolean>;
  getWebActuationEnabled(): Promise<boolean>;
  setWebActuationEnabled(enabled: boolean): void;
  getExperimentalTriggeringEnabled(): Promise<boolean>;
  setExperimentalTriggeringEnabled(enabled: boolean): void;
}

export class GlicBrowserProxyImpl implements GlicBrowserProxy {
  setGlicOsLauncherEnabled(enabled: boolean) {
    chrome.send('setGlicOsLauncherEnabled', [enabled]);
  }

  getGlicShortcut() {
    return sendWithPromise<string>('getGlicShortcut');
  }

  setGlicShortcut(shortcut: string) {
    return sendWithPromise<void>('setGlicShortcut', shortcut);
  }

  getGlicFocusToggleShortcut() {
    return sendWithPromise<string>('getGlicFocusToggleShortcut');
  }

  setGlicFocusToggleShortcut(shortcut: string) {
    return sendWithPromise<void>('setGlicFocusToggleShortcut', shortcut);
  }

  setShortcutSuspensionState(shouldSuspend: boolean) {
    chrome.send('setShortcutSuspensionState', [shouldSuspend]);
  }

  getDisallowedByAdmin() {
    return sendWithPromise<boolean>('getGlicDisallowedByAdmin');
  }

  getActorLoginPermissions() {
    return sendWithPromise<LoginPermission[]>('getActorLoginPermissions');
  }

  revokeActorLoginPermission(signonRealm: string, username: string) {
    return sendWithPromise<boolean>(
        'revokeActorLoginPermission', signonRealm, username);
  }

  getGlicSelectionShortcut() {
    return sendWithPromise<string>('getGlicSelectionShortcut');
  }

  setGlicSelectionShortcut(shortcut: string) {
    return sendWithPromise<void>('setGlicSelectionShortcut', shortcut);
  }

  getWebActuationToggleVisibility() {
    return sendWithPromise<boolean>('getWebActuationToggleVisibility');
  }

  getWebActuationEnabled() {
    return sendWithPromise<boolean>('getWebActuationEnabled');
  }

  setWebActuationEnabled(enabled: boolean) {
    chrome.send('setWebActuationEnabled', [enabled]);
  }

  getExperimentalTriggeringEnabled() {
    return sendWithPromise<boolean>('getExperimentalTriggeringEnabled');
  }

  setExperimentalTriggeringEnabled(enabled: boolean) {
    chrome.send('setExperimentalTriggeringEnabled', [enabled]);
  }

  static getInstance(): GlicBrowserProxy {
    return instance || (instance = new GlicBrowserProxyImpl());
  }

  static setInstance(obj: GlicBrowserProxy) {
    instance = obj;
  }
}

let instance: GlicBrowserProxy|null = null;
