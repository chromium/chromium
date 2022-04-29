// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Informs Dictation whether the Pumpkin semantic parser is
 * available. This file will be included in the accessibility_common extension
 * when the gn argument enable_pumpkin_for_dictation is set to true.
 */

export const PumpkinAvailability = {
  /**
   * Whether to use pumpkin for this locale.
   * @param {string} locale
   * @return {boolean}
   */
  usePumpkin(locale) {
    return PumpkinAvailability.LOCALES[locale] !== undefined;
  },

  // Map from BCP-47 locale code (see dictation.cc) to directory name in
  // dictation/pumpkin_configs/ for supported Pumpkin locales.
  // TODO(crbug.com/1264544): Determine if all en* languages can be mapped to
  // en_us. Possible locales are listed in dictation.cc,
  // kWebSpeechSupportedLocales.
  /** @const {!Object<string, string>} */
  LOCALES: {
    'en-US': 'en_us',
    'en-AU': 'en_us',
    'en-CA': 'en_us',
    'en-GB': 'en_us',
    'en-GH': 'en_us',
    'en-HK': 'en_us',
    'en-IN': 'en_us',
    'en-KE': 'en_us',
    'en-NG': 'en_us',
    'en-NZ': 'en_us',
    'en-PH': 'en_us',
    'en-PK': 'en_us',
    'en-SG': 'en_us',
    'en-TZ': 'en_us',
    'en-ZA': 'en_us',
  },
};
