// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'settings-dark-mode-subpage' is the setting subpage containing
 *  dark mode settings to switch between dark and light mode, theming,
 *  and a scheduler.
 */
Polymer({
  is: 'settings-dark-mode-subpage',

  behaviors: [
    // TODO(crbug.com/1217436): add search to dark mode.
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /**
   * @return {string}
   * @private
   */
  getDarkModeOnOffLabel_() {
    return this.i18n(
        this.getPref('ash.dark_mode.enabled').value ? 'darkModeOn' :
                                                      'darkModeOff');
  },
});