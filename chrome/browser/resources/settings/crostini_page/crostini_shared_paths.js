// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-shared-paths' is the settings shared paths subpage for Crostini.
 */

Polymer({
  is: 'settings-crostini-shared-paths',

  behaviors: [PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The shared path string suitable for display in the UI.
     * @private {Array<!CrostiniSharedPath>}
     */
    sharedPaths_: Array,
  },

  observers:
      ['onCrostiniSharedPathsChanged_(prefs.crostini.shared_paths.value)'],

  /**
   * @param {!Array<string>} paths
   * @private
   */
  onCrostiniSharedPathsChanged_: function(paths) {
    settings.CrostiniBrowserProxyImpl.getInstance()
        .getCrostiniSharedPathsDisplayText(paths)
        .then(text => {
          let sharedPaths = [];
          for (let i = 0; i < paths.length; i++) {
            sharedPaths.push({path: paths[i], pathDisplayText: text[i]});
          }
          this.sharedPaths_ = sharedPaths;
        });
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveSharedPathTap_: function(event) {
    settings.CrostiniBrowserProxyImpl.getInstance().removeCrostiniSharedPath(
        event.model.item.path);
  },
});
