// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Informs Dictation whether the Pumpkin semantic parser is
 * available. This file will be included in the accessibility_common extension
 * when the gn argument enable_pumpkin_for_dictation is set to false.
 */

export const PumpkinAvailability = {
  /**
   * Whether Pumpkin can be used for this locale. Always returns false.
   * @param {string} locale
   * @return {boolean}
   */
  usePumpkin(locale) {
    return false;
  },

  /** @const {!Object<string, string>} */
  LOCALES: {},
};
