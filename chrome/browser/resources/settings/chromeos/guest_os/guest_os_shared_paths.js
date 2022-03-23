// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'guest-os-shared-paths' is the settings shared paths subpage for guest OSes.
 */

import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '../../settings_shared_css.js';

import {assert} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

import {GuestOsBrowserProxy, GuestOsBrowserProxyImpl} from './guest_os_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-guest-os-shared-paths',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The type of Guest OS to share with. Should be 'crostini' or 'pluginVm'.
     */
    guestOsType: String,

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
     * to remove a path. Null indicates that removal succeeded. When non-null,
     * the failure dialog is shown.
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
      if (vms.includes(this.vmName_())) {
        vmPaths.push(path);
      }
    }
    GuestOsBrowserProxyImpl.getInstance()
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
    GuestOsBrowserProxyImpl.getInstance()
        .removeGuestOsSharedPath(this.vmName_(), path)
        .then(success => {
          if (!success) {
            this.sharedPathWhichFailedRemoval_ = path;
          }
        });
    recordSettingChange();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveSharedPathClick_(event) {
    this.removeSharedPath_(event.model.item.path);
  },

  /** @private */
  onRemoveFailedRetryClick_() {
    this.removeSharedPath_(assert(this.sharedPathWhichFailedRemoval_));
  },

  /** @private */
  onRemoveFailedDismissClick_() {
    this.sharedPathWhichFailedRemoval_ = null;
  },

  /**
   * @return {string} The name of the VM to share devices with.
   * @private
   */
  vmName_() {
    return {crostini: 'termina', pluginVm: 'PvmDefault'}[this.guestOsType];
  },

  /**
   * @return {string} Description for the page.
   * @private
   */
  getDescriptionText_() {
    return this.i18n(this.guestOsType + 'SharedPathsInstructionsLocate') +
        '\n' + this.i18n(this.guestOsType + 'SharedPathsInstructionsAdd');
  },

  /**
   * @return {string} Message to display when removing a shared path fails.
   * @private
   */
  getRemoveFailureMessage_() {
    return this.i18n(
        this.guestOsType + 'SharedPathsRemoveFailureDialogMessage');
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
