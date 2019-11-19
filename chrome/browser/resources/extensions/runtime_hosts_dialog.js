// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemDelegate} from './item.js';

// A RegExp to roughly match acceptable patterns entered by the user.
// exec'ing() this RegExp will match the following groups:
// 0: Full matched string.
// 1: Scheme + scheme separator (e.g., 'https://').
// 2: Scheme only (e.g., 'https').
// 3: Match subdomains ('*.').
// 4: Hostname (e.g., 'example.com').
// 5: Port, including ':' separator (e.g., ':80').
// 6: Path, include '/' separator (e.g., '/*').
const patternRegExp = new RegExp(
    '^' +
    // Scheme; optional.
    '((http|https|\\*)://)?' +
    // Include subdomains specifier; optional.
    '(\\*\\.)?' +
    // Hostname, required.
    '([a-z0-9\\.-]+\\.[a-z0-9]+)' +
    // Port, optional.
    '(:[0-9]+)?' +
    // Path, optional but if present must be '/' or '/*'.
    '(\\/\\*|\\/)?' +
    '$');

export function getPatternFromSite(site) {
  const res = patternRegExp.exec(site);
  assert(res);
  const scheme = res[1] || '*://';
  const host = (res[3] || '') + res[4];
  const port = res[5] || '';
  const path = '/*';
  return scheme + host + port + path;
}

Polymer({
  is: 'extensions-runtime-hosts-dialog',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!ItemDelegate} */
    delegate: Object,

    /** @type {string} */
    itemId: String,

    /**
     * The site that this entry is currently managing. Only non-empty if this
     * is for editing an existing entry.
     * @type {?string}
     */
    currentSite: {
      type: String,
      value: null,
    },

    /**
     * Whether the dialog should update the host access to be "on specific
     * sites" before adding a new host permission.
     */
    updateHostAccess: {
      type: Boolean,
      value: false,
    },

    /**
     * The site to add an exception for.
     * @private
     */
    site_: String,

    /**
     * Whether the currently-entered input is valid.
     * @private
     */
    inputInvalid_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  attached: function() {
    if (this.currentSite !== null && this.currentSite !== undefined) {
      this.site_ = this.currentSite;
      this.validate_();
    }
    this.$.dialog.showModal();
  },

  /** @return {boolean} */
  isOpen: function() {
    return this.$.dialog.open;
  },

  /**
   * Validates that the pattern entered is valid.
   * @private
   */
  validate_: function() {
    // If input is empty, disable the action button, but don't show the red
    // invalid message.
    if (this.site_.trim().length == 0) {
      this.inputInvalid_ = false;
      return;
    }

    const valid = patternRegExp.test(this.site_);
    this.inputInvalid_ = !valid;
  },

  /**
   * @return {string}
   * @private
   */
  computeDialogTitle_: function() {
    const stringId = this.currentSite === null ? 'runtimeHostsDialogTitle' :
                                                 'hostPermissionsEdit';
    return loadTimeData.getString(stringId);
  },

  /**
   * @return {boolean}
   * @private
   */
  computeSubmitButtonDisabled_: function() {
    return this.inputInvalid_ || this.site_ === undefined ||
        this.site_.trim().length == 0;
  },

  /**
   * @return {string}
   * @private
   */
  computeSubmitButtonLabel_: function() {
    const stringId = this.currentSite === null ? 'add' : 'save';
    return loadTimeData.getString(stringId);
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.cancel();
  },

  /**
   * The tap handler for the submit button (adds the pattern and closes
   * the dialog).
   * @private
   */
  onSubmitTap_: function() {
    if (this.currentSite !== null) {
      this.handleEdit_();
    } else {
      this.handleAdd_();
    }
  },

  /**
   * Handles adding a new site entry.
   * @private
   */
  handleAdd_: function() {
    assert(!this.currentSite);

    if (this.updateHostAccess) {
      this.delegate.setItemHostAccess(
          this.itemId, chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES);
    }

    this.addPermission_();
  },

  /**
   * Handles editing an existing site entry.
   * @private
   */
  handleEdit_: function() {
    assert(this.currentSite);
    assert(
        !this.updateHostAccess,
        'Editing host permissions should only be possible if the host ' +
            'access is already set to specific sites.');

    if (this.currentSite == this.site_) {
      // No change in values, so no need to update anything.
      this.$.dialog.close();
      return;
    }

    // Editing an existing entry is done by removing the current site entry,
    // and then adding the new one.
    this.delegate.removeRuntimeHostPermission(this.itemId, this.currentSite)
        .then(() => {
          this.addPermission_();
        });
  },

  /**
   * Adds the runtime host permission through the delegate. If successful,
   * closes the dialog; otherwise displays the invalid input message.
   * @private
   */
  addPermission_: function() {
    const pattern = getPatternFromSite(this.site_);
    this.delegate.addRuntimeHostPermission(this.itemId, pattern)
        .then(
            () => {
              this.$.dialog.close();
            },
            () => {
              this.inputInvalid_ = true;
            });
  },
});
