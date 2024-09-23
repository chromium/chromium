// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles validation of current permissions before performing an
 * action.
 */
import {Command} from './command.js';
import {CommandStore} from './command_store.js';

import SessionType = chrome.chromeosInfoPrivate.SessionType;

export class PermissionChecker {
  private isIncognito_: boolean;
  private isKioskSession_: boolean;

  static instance: PermissionChecker;

  private constructor() {
    this.isIncognito_ = Boolean(chrome.runtime.getManifest()['incognito']);
    this.isKioskSession_ = false;
  }

  static async init(): Promise<void> {
    PermissionChecker.instance = new PermissionChecker();
    await PermissionChecker.instance.fetchState_();
  }

  static isAllowed(command: Command): boolean {
    return PermissionChecker.instance.isAllowed_(command);
  }

  private isAllowed_(command: Command): boolean {
    if (!this.isIncognito_ && !this.isKioskSession_) {
      return true;
    }

    return !CommandStore.denySignedOut(command);
  }

  private async fetchState_(): Promise<void> {
    const result: {sessionType?: SessionType} = await new Promise(
        resolve => chrome.chromeosInfoPrivate.get(['sessionType'], resolve));
    this.isKioskSession_ = result['sessionType'] === SessionType.KIOSK;
  }
}
