// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines Localization, a Polymer behavior to help localize
 * tutorial content.
 */

/** @polymerBehavior */
export const Localization = {
  /**
   * Returns the message with the given message id from the ChromeVox namespace.
   * @param {string} id The id of the string.
   * @param {Array<string>=} opt_subs Substitution strings.
   * @return {string} The localized message.
   */
  getMsg(id, opt_subs) {
    const message = chrome.i18n.getMessage('chromevox_' + id, opt_subs);
    if (message === undefined || message === '') {
      throw new Error('Invalid ChromeVox message id: ' + id);
    }

    return message;
  },
};

export class LocalizationInterface {
  /**
   * @param {string} id
   * @param {Array<string>=} opt_subs
   * @return {string}
   */
  getMsg(id, opt_subs) {}
}
