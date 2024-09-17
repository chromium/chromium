// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './restricted_sites_dialog.js';
import './toggle_row.js';
import './shared_style.css.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './host_permissions_toggle_list.html.js';
import {UserAction} from './item_util.js';
import type {ExtensionsRestrictedSitesDialogElement} from './restricted_sites_dialog.js';
import {getMatchingUserSpecifiedSites} from './runtime_hosts_dialog.js';
import {SiteSettingsMixin} from './site_permissions/site_settings_mixin.js';
import type {ExtensionsToggleRowElement} from './toggle_row.js';
import {getFaviconUrl} from './url_util.js';


export interface ExtensionsHostPermissionsToggleListElement {
  $: {
    allHostsToggle: ExtensionsToggleRowElement,
    linkIconButton: HTMLAnchorElement,
  };
}

const ExtensionsHostPermissionsToggleListElementBase =
    SiteSettingsMixin(PolymerElement);

export class ExtensionsHostPermissionsToggleListElement extends
    ExtensionsHostPermissionsToggleListElementBase {
  static get is() {
    return 'extensions-host-permissions-toggle-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The underlying permissions data.
       */
      permissions: Object,

      itemId: String,

      /**
       * This is set as the host the user is trying to toggle on/grant host
       * permissions for, if the host matches one or more user specified
       * restricted sites.
       */
      selectedHost_: {
        type: String,
        value: '',
      },

      // The list of restricted sites that match a host the user is toggling on.
      matchingRestrictedSites_: Array,

      showMatchingRestrictedSitesDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  permissions: chrome.developerPrivate.RuntimeHostPermissions;
  itemId: string;
  private selectedHost_: string;
  private matchingRestrictedSites_: string[];
  private showMatchingRestrictedSitesDialog_: boolean;

  getRestrictedSitesDialog(): ExtensionsRestrictedSitesDialogElement|null {
    return this.shadowRoot!
        .querySelector<ExtensionsRestrictedSitesDialogElement>(
            'extensions-restricted-sites-dialog');
  }

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
  private getSortedHosts_(): chrome.developerPrivate.SiteControl[] {
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

    if (!checked) {
      this.delegate.removeRuntimeHostPermission(this.itemId, host);
      this.delegate.recordUserAction(UserAction.SPECIFIC_TOGGLED_OFF);
      return;
    }

    // If the user is about to toggle on `host`, show a dialog if there are
    // matching user specified restricted sites instead of granting `host`
    // right away.
    this.delegate.recordUserAction(UserAction.SPECIFIC_TOGGLED_ON);
    const matchingRestrictedSites =
        getMatchingUserSpecifiedSites(this.restrictedSites, host);
    if (matchingRestrictedSites.length) {
      this.selectedHost_ = host;
      this.matchingRestrictedSites_ = matchingRestrictedSites;
      this.showMatchingRestrictedSitesDialog_ = true;
      // Flow continues in onRestrictedSitesDialogClose_.
      return;
    }

    this.delegate.addRuntimeHostPermission(this.itemId, host);
  }

  private isItemChecked_(item: chrome.developerPrivate.SiteControl): boolean {
    return item.granted || this.selectedHost_ === item.host;
  }

  private getAllHostsToggleLabelClass_(): string {
    return this.enableEnhancedSiteControls ? 'new-all-hosts-toggle-label' : '';
  }

  private onLearnMoreClick_() {
    this.delegate.recordUserAction(UserAction.LEARN_MORE);
  }

  private getFaviconUrl_(url: string): string {
    return getFaviconUrl(url);
  }

  private unselectHost_() {
    this.showMatchingRestrictedSitesDialog_ = false;
    this.selectedHost_ = '';
    this.matchingRestrictedSites_ = [];
  }

  private onMatchingRestrictedSitesDialogClose_() {
    const dialog = this.getRestrictedSitesDialog();
    assert(dialog);
    if (dialog.wasConfirmed()) {
      assert(this.matchingRestrictedSites_.length);
      this.delegate.addRuntimeHostPermission(this.itemId, this.selectedHost_)
          .then(() => {
            this.delegate.removeUserSpecifiedSites(
                chrome.developerPrivate.SiteSet.USER_RESTRICTED,
                this.matchingRestrictedSites_);
          })
          .finally(() => {
            this.unselectHost_();
          });
    } else {
      this.unselectHost_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-host-permissions-toggle-list':
        ExtensionsHostPermissionsToggleListElement;
  }
}

customElements.define(
    ExtensionsHostPermissionsToggleListElement.is,
    ExtensionsHostPermissionsToggleListElement);
