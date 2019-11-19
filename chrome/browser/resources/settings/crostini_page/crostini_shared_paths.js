// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-shared-paths' is the settings shared paths subpage for Crostini.
 */

(function() {

/**
 * The default crostini VM is named 'termina'.
 * https://cs.chromium.org/chromium/src/chrome/browser/chromeos/crostini/crostini_util.h?q=kCrostiniDefaultVmName&dr=CSs
 * @type {string}
 */
const DEFAULT_CROSTINI_VM = 'termina';

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

  observers: [
    'onCrostiniSharedPathsChanged_(prefs.guest_os.paths_shared_to_vms.value)'
  ],

  /**
   * @param {!Object<!Array<string>>} paths
   * @private
   */
  onCrostiniSharedPathsChanged_: function(paths) {
    const vmPaths = [];
    for (const path in paths) {
      const vms = paths[path];
      if (vms.includes(DEFAULT_CROSTINI_VM)) {
        vmPaths.push(path);
      }
    }
    settings.CrostiniBrowserProxyImpl.getInstance()
        .getCrostiniSharedPathsDisplayText(vmPaths)
        .then(text => {
          this.sharedPaths_ = vmPaths.map(
              (path, i) => ({path: path, pathDisplayText: text[i]}));
        });
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveSharedPathTap_: function(event) {
    settings.CrostiniBrowserProxyImpl.getInstance().removeCrostiniSharedPath(
        DEFAULT_CROSTINI_VM, event.model.item.path);
  },
});
})();
