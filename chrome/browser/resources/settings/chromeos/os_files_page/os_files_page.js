// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-files-page' is the settings page containing files settings.
 *
 */
Polymer({
  is: 'os-settings-files-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.SMB_SHARES) {
          map.set(settings.routes.SMB_SHARES.path, '#smbShares');
        }
        return map;
      },
    },

  },

  /** @private */
  onTapSmbShares_: function() {
    settings.navigateTo(settings.routes.SMB_SHARES);
  },
});
