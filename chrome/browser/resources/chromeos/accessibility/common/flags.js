// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Manages fetching and checking command line flags. */

/** @enum {string} */
export const FlagName = {
  CHROMEVOX_Q1_FAST_TRACK: 'enable-chromevox-q1-fast-track-features',
  MAGNIFIER_DEBUG_DRAW_RECT: 'enable-magnifier-debug-draw-rect',
  SWITCH_ACCESS_TEXT: 'enable-experimental-accessibility-switch-access-text',
};

export class Flags {
  constructor() {
    /** @private {!Object<!FlagName, boolean>} */
    this.enabled_ = {};
  }

  static async init() {
    if (Flags.instance) {
      throw new Error(
          'Cannot create two instances of Flags in the same context');
    }
    Flags.instance = new Flags();
    await Flags.instance.fetch_();
  }

  /**
   * @param {!FlagName} flagName
   * @return {boolean}
   */
  static isEnabled(flagName) {
    return Flags.instance.enabled_[flagName];
  }

  /**
   * @return {!Promise}
   * @private
   */
  async fetch_() {
    const promises = [];
    for (const flag of Object.values(FlagName)) {
      promises.push(new Promise(resolve => {
        chrome.commandLinePrivate.hasSwitch(flag, result => {
          this.enabled_[flag] = result;
          resolve();
        });
      }));
    }
    return Promise.all(promises);
  }
}
