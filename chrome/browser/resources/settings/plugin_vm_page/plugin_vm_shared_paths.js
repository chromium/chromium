// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'plugin-vm-shared-paths' is the settings shared paths subpage for Plugin VM.
 */

(function() {

/**
 * The Plugin VM is named 'PvmDefault'.
 * https://cs.chromium.org/chromium/src/chrome/browser/chromeos/plugin_vm/plugin_vm_util.h?q=kPluginVmName
 * @type {string}
 */
const PLUGIN_VM = 'PvmDefault';

Polymer({
  is: 'settings-plugin-vm-shared-paths',

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
  },

  observers: [
    'onPluginVmSharedPathsChanged_(prefs.guest_os.paths_shared_to_vms.value)'
  ],

  /**
   * @param {!Object<!Array<string>>} paths
   * @private
   */
  onPluginVmSharedPathsChanged_: function(paths) {
    const vmPaths = [];
    for (const path in paths) {
      const vms = paths[path];
      if (vms.includes(PLUGIN_VM)) {
        vmPaths.push(path);
      }
    }
    settings.PluginVmBrowserProxyImpl.getInstance()
        .getPluginVmSharedPathsDisplayText(vmPaths)
        .then(text => {
          this.sharedPaths_ = vmPaths.map(
              (path, i) => ({path: path, pathDisplayText: text[i]}));
        });
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveSharedPathClick_: function(event) {
    settings.PluginVmBrowserProxyImpl.getInstance().removePluginVmSharedPath(
        PLUGIN_VM, event.model.item.path);
  },
});
})();
