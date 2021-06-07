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

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemDelegate} from './item.js';
import {UserAction} from './item_util.js';
import {ExtensionsToggleRowElement} from './toggle_row.js';

class ExtensionsHostPermissionsToggleListElement extends PolymerElement {
  static get is() {
    return 'extensions-host-permissions-toggle-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The underlying permissions data.
       */
      permissions: Object,

      itemId: String,

      delegate: Object,
    };
  }

  permissions: chrome.developerPrivate.RuntimeHostPermissions;
  private itemId: string;
  delegate: ItemDelegate;

  /**
   * @return Whether the item is allowed to execute on all of its requested
   *     sites.
   */
  private allowedOnAllHosts_(): boolean {
    return this.permissions.hostAccess ===
        chrome.developerPrivate.HostAccess.ON_ALL_SITES;
  }

  /**
   * @return A lexicographically-sorted list of the hosts associated with this
   *     item.
   */
  private getSortedHosts_(): Array<chrome.developerPrivate.SiteControl> {
    return this.permissions.hosts.sort((a, b) => {
      if (a.host < b.host) {
        return -1;
      }
      if (a.host > b.host) {
        return 1;
      }
      return 0;
    });
  }

  private onAllHostsToggleChanged_(e: CustomEvent<boolean>) {
    // TODO(devlin): In the case of going from all sites to specific sites,
    // we'll withhold all sites (i.e., all specific site toggles will move to
    // unchecked, and the user can check them individually). This is slightly
    // different than the sync page, where disabling the "sync everything"
    // switch leaves everything synced, and user can uncheck them
    // individually. It could be nice to align on behavior, but probably not
    // super high priority.
    const checked = e.detail;

    if (checked) {
      this.delegate.setItemHostAccess(
          this.itemId, chrome.developerPrivate.HostAccess.ON_ALL_SITES);
      this.delegate.recordUserAction(UserAction.ALL_TOGGLED_ON);
    } else {
      this.delegate.setItemHostAccess(
          this.itemId, chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES);
      this.delegate.recordUserAction(UserAction.ALL_TOGGLED_OFF);
    }
  }

  private onHostAccessChanged_(e: CustomEvent<boolean>) {
    const host = (e.target as unknown as {host: string}).host;
    const checked = (e.target as ExtensionsToggleRowElement).checked;

    if (checked) {
      this.delegate.addRuntimeHostPermission(this.itemId, host);
      this.delegate.recordUserAction(UserAction.SPECIFIC_TOGGLED_ON);
    } else {
      this.delegate.removeRuntimeHostPermission(this.itemId, host);
      this.delegate.recordUserAction(UserAction.SPECIFIC_TOGGLED_OFF);
    }
  }

  private onLearnMoreClick_() {
    this.delegate.recordUserAction(UserAction.LEARN_MORE);
  }
}

customElements.define(
    ExtensionsHostPermissionsToggleListElement.is,
    ExtensionsHostPermissionsToggleListElement);
