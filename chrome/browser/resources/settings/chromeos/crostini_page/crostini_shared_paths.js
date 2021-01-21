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
     * @private {Array<!{path: string, pathDisplayText: string}>}
     */
    sharedPaths_: Array,

    /**
     * The shared path which failed to be removed in the most recent attempt
     * to remove a path. Null indicates that removal succeeded.
     * @private {?string}
     */
    sharedPathWhichFailedRemoval_: {
      type: String,
      value: null,
    },
  },

  observers: [
    'onGuestOsSharedPathsChanged_(prefs.guest_os.paths_shared_to_vms.value)'
  ],

  /**
   * @param {!Object<!Array<string>>} paths
   * @private
   */
  onGuestOsSharedPathsChanged_(paths) {
    const vmPaths = [];
    for (const path in paths) {
      const vms = paths[path];
      if (vms.includes(DEFAULT_CROSTINI_VM)) {
        vmPaths.push(path);
      }
    }
    settings.GuestOsBrowserProxyImpl.getInstance()
        .getGuestOsSharedPathsDisplayText(vmPaths)
        .then(text => {
          this.sharedPaths_ = vmPaths.map(
              (path, i) => ({path: path, pathDisplayText: text[i]}));
        });
  },

  /**
   * @param {string} path
   * @private
   */
  removeSharedPath_(path) {
    this.sharedPathWhichFailedRemoval_ = null;
    settings.GuestOsBrowserProxyImpl.getInstance()
        .removeGuestOsSharedPath(DEFAULT_CROSTINI_VM, path)
        .then(result => {
          if (!result) {
            this.sharedPathWhichFailedRemoval_ = path;
            // Flush to make sure that the retry dialog is attached.
            Polymer.dom.flush();
            this.$$('#removeSharedPathFailedDialog').showModal();
          }
        });
    settings.recordSettingChange();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveSharedPathTap_(event) {
    this.removeSharedPath_(event.model.item.path);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveFailedRetryTap_(event) {
    this.removeSharedPath_(assert(this.sharedPathWhichFailedRemoval_));
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveFailedDismissTap_(event) {
    this.sharedPathWhichFailedRemoval_ = null;
  },

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  generatePathDisplayTextId_(index) {
    return 'path-display-text-' + index;
  },
});
})();
