// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './toggle_row.js';
import './shared_style.js';
import './strings.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemDelegate} from './item.js';

Polymer({
  is: 'extensions-host-permissions-toggle-list',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * The underlying permissions data.
     * @type {chrome.developerPrivate.RuntimeHostPermissions}
     */
    permissions: Object,

    /** @private */
    itemId: String,

    /** @type {!ItemDelegate} */
    delegate: Object,
  },

  /**
   * @return {boolean} Whether the item is allowed to execute on all of its
   *     requested sites.
   * @private
   */
  allowedOnAllHosts_: function() {
    return this.permissions.hostAccess ==
        chrome.developerPrivate.HostAccess.ON_ALL_SITES;
  },

  /**
   * Returns a lexicographically-sorted list of the hosts associated with this
   * item.
   * @return {!Array<!chrome.developerPrivate.SiteControl>}
   * @private
   */
  getSortedHosts_: function() {
    return this.permissions.hosts.sort((a, b) => {
      if (a.host < b.host) {
        return -1;
      }
      if (a.host > b.host) {
        return 1;
      }
      return 0;
    });
  },

  /** @private */
  onAllHostsToggleChanged_: function() {
    // TODO(devlin): In the case of going from all sites to specific sites,
    // we'll withhold all sites (i.e., all specific site toggles will move to
    // unchecked, and the user can check them individually). This is slightly
    // different than the sync page, where disabling the "sync everything"
    // switch leaves everything synced, and user can uncheck them
    // individually. It could be nice to align on behavior, but probably not
    // super high priority.
    this.delegate.setItemHostAccess(
        this.itemId,
        this.$.allHostsToggle.checked ?
            chrome.developerPrivate.HostAccess.ON_ALL_SITES :
            chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES);
  },

  /** @private */
  onHostAccessChanged_: function(e) {
    const host = e.target.host;
    const checked = e.target.checked;

    if (checked) {
      this.delegate.addRuntimeHostPermission(this.itemId, host);
    } else {
      this.delegate.removeRuntimeHostPermission(this.itemId, host);
    }
  },
});
