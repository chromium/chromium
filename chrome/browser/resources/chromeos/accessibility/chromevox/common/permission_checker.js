// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles validation of current permissions before performing an
 * action.
 */
import {Command, CommandStore} from './command_store.js';

const SessionType = chrome.chromeosInfoPrivate.SessionType;

export class PermissionChecker {
  /** @private */
  constructor() {
    /** @private {boolean} */
    this.isIncognito_ = Boolean(chrome.runtime.getManifest()['incognito']);

    /** @private {boolean} */
    this.isKioskSession_ = false;
  }

  static async init() {
    PermissionChecker.instance = new PermissionChecker();
    await PermissionChecker.instance.fetchState_();
  }

  /**
   * @param {!Command} command
   * @return {boolean}
   */
  static isAllowed(command) {
    return PermissionChecker.instance.isAllowed_(command);
  }

  /**
   * @param {!Command} command
   * @return {boolean}
   * @private
   */
  isAllowed_(command) {
    if (!this.isIncognito_ && !this.isKioskSession_) {
      return true;
    }

    return !CommandStore.COMMAND_DATA[command] ||
        !CommandStore.COMMAND_DATA[command].denySignedOut;
  }

  /** @private */
  async fetchState_() {
    const result = await new Promise(
        resolve => chrome.chromeosInfoPrivate.get(['sessionType'], resolve));
    this.isKioskSession_ = result['sessionType'] === SessionType.KIOSK;
  }
}
