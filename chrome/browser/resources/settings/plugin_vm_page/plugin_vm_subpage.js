// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'plugin-vm-subpage' is the settings subpage for managing Plugin VM.
 */

Polymer({
  is: 'settings-plugin-vm-subpage',

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /** @private */
  onSharedPathsClick_: function() {
    settings.navigateTo(settings.routes.PLUGIN_VM_SHARED_PATHS);
  },
});
