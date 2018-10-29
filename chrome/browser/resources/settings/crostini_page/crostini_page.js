// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-page' is the settings page for enabling Crostini.
 * Crostini Containers run Linux inside a Termina VM, allowing
 * the user to run Linux apps on their Chromebook.
 */

Polymer({
  is: 'settings-crostini-page',

  behaviors: [I18nBehavior, PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.CROSTINI_DETAILS) {
          map.set(
              settings.routes.CROSTINI_DETAILS.path,
              '#crostini .subpage-arrow button');
        }
        if (settings.routes.CROSTINI_SHARED_PATHS) {
          map.set(
              settings.routes.CROSTINI_SHARED_PATHS.path,
              '#crostini .subpage-arrow button');
        }
        return map;
      },
    },
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEnableTap_: function(event) {
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniInstallerView();
    event.stopPropagation();
  },

  /** @private */
  onSubpageTap_: function(event) {
    if (this.prefs.crostini.enabled.value)
      settings.navigateTo(settings.routes.CROSTINI_DETAILS);
  },
});
