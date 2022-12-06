// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './strings.m.js';
import './shared_style.css.js';
import './shared_vars.css.js';
import './site_permissions_edit_permissions_dialog.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './site_permissions_site_group.html.js';
import {SiteSettingsDelegate} from './site_settings_mixin.js';
import {getFaviconUrl} from './url_util.js';

export interface SitePermissionsSiteGroupElement {
  $: {
    etldOrSite: HTMLElement,
    etldOrSiteSubtext: HTMLElement,
  };
}

export class SitePermissionsSiteGroupElement extends PolymerElement {
  static get is() {
    return 'site-permissions-site-group';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      data: Object,
      delegate: Object,
      extensions: Array,

      listIndex: {
        type: Number,
        value: -1,
      },

      expanded_: {
        type: Boolean,
        value: false,
      },

      isExpandable_: {
        type: Boolean,
        computed: 'computeIsExpandable_(data.sites)',
      },

      showEditSitePermissionsDialog_: {
        type: Boolean,
        value: false,
      },

      siteToEdit_: {
        type: Object,
        value: null,
      },
    };
  }

  data: chrome.developerPrivate.SiteGroup;
  delegate: SiteSettingsDelegate;
  extensions: chrome.developerPrivate.ExtensionInfo[];
  listIndex: number;
  private expanded_: boolean;
  private isExpandable_: boolean;
  private showEditSitePermissionsDialog_: boolean;
  private siteToEdit_: chrome.developerPrivate.SiteInfo|null;

  private getEtldOrSiteFaviconUrl_(): string {
    return getFaviconUrl(this.getDisplayUrl_());
  }

  private getFaviconUrl_(url: string): string {
    return getFaviconUrl(url);
  }

  private computeIsExpandable_(): boolean {
    return this.data.sites.length > 1;
  }

  private getClassForIndex_(): string {
    return this.listIndex > 0 ? 'hr' : '';
  }

  private getDisplayUrl_(): string {
    return this.data.sites.length === 1 ? this.data.sites[0].site :
                                          this.data.etldPlusOne;
  }

  private getEtldOrSiteSubText_(): string {
    // TODO(crbug.com/1253673): Revisit what to show for this eTLD+1 group's
    // subtext. For now, default to showing no text if there is any mix of sites
    // under the group (i.e. user permitted/restricted/specified by extensions).
    const siteSet = this.data.sites[0].siteSet;
    const isSiteSetConsistent =
        this.data.sites.every(site => site.siteSet === siteSet);
    if (!isSiteSetConsistent) {
      return '';
    }

    if (siteSet === chrome.developerPrivate.SiteSet.USER_PERMITTED) {
      return loadTimeData.getString('permittedSites');
    }

    return siteSet === chrome.developerPrivate.SiteSet.USER_RESTRICTED ?
        loadTimeData.getString('restrictedSites') :
        loadTimeData.getStringF(
            'sitePermissionsAllSitesExtensionCount', this.data.numExtensions);
  }

  private getSiteSubtext_(siteInfo: chrome.developerPrivate.SiteInfo): string {
    if (siteInfo.numExtensions > 0) {
      return loadTimeData.getStringF(
          'sitePermissionsAllSitesExtensionCount', siteInfo.numExtensions);
    }

    return loadTimeData.getString(
        siteInfo.siteSet === chrome.developerPrivate.SiteSet.USER_PERMITTED ?
            'permittedSites' :
            'restrictedSites');
  }

  private onEditSiteClick_() {
    this.siteToEdit_ = this.data.sites[0];
    this.showEditSitePermissionsDialog_ = true;
  }

  private onEditSiteInListClick_(
      e: DomRepeatEvent<chrome.developerPrivate.SiteInfo>) {
    this.siteToEdit_ = e.model.item;
    this.showEditSitePermissionsDialog_ = true;
  }

  private onEditSitePermissionsDialogClose_() {
    this.showEditSitePermissionsDialog_ = false;
    assert(this.siteToEdit_, 'Site To Edit');
    this.siteToEdit_ = null;
  }

  private isUserSpecifiedSite_(siteSet: chrome.developerPrivate.SiteSet):
      boolean {
    return siteSet === chrome.developerPrivate.SiteSet.USER_PERMITTED ||
        siteSet === chrome.developerPrivate.SiteSet.USER_RESTRICTED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-permissions-site-group': SitePermissionsSiteGroupElement;
  }
}

customElements.define(
    SitePermissionsSiteGroupElement.is, SitePermissionsSiteGroupElement);
