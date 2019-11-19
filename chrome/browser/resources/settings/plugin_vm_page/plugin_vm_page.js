// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'plugin-vm-page' is the settings page for Plugin VM.
 */

Polymer({
  is: 'settings-plugin-vm-page',

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
        if (settings.routes.PLUGIN_VM_DETAILS) {
          map.set(settings.routes.PLUGIN_VM_DETAILS.path, '#plugin-vm');
        }
        if (settings.routes.PLUGIN_VM_SHARED_PATHS) {
          map.set(settings.routes.PLUGIN_VM_SHARED_PATHS.path, '#plugin-vm');
        }
        return map;
      },
    },
  },

  /** @private */
  onSubpageClick_: function(event) {
    settings.navigateTo(settings.routes.PLUGIN_VM_DETAILS);
  },
});
