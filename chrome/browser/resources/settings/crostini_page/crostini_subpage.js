// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-subpage' is the settings subpage for managing Crostini.
 */

Polymer({
  is: 'settings-crostini-subpage',

  behaviors: [PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  observers: ['onCrostiniEnabledChanged_(prefs.crostini.enabled.value)'],

  /** @private */
  onCrostiniEnabledChanged_: function(enabled) {
    if (!enabled &&
        settings.getCurrentRoute() == settings.routes.CROSTINI_DETAILS) {
      settings.navigateToPreviousRoute();
    }
  },

  /**
   * Shows a confirmation dialog when removing crostini.
   * @param {!Event} event
   * @private
   */
  onRemoveTap_: function(event) {
    settings.CrostiniBrowserProxyImpl.getInstance().requestRemoveCrostini();
  },

  /** @private */
  onSharedPathsTap_: function(event) {
    settings.navigateTo(settings.routes.CROSTINI_SHARED_PATHS);
  },
});
