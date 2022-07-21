// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './strings.m.js';
import './shared_style.css.js';
import './shared_vars.css.js';
import './site_permissions_edit_permissions_dialog.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
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
    if (this.data.sites.length === 1) {
      return this.getSiteSubtext_(this.data.sites[0].siteList);
    }

    const areAllPermitted = this.data.sites.every(
        site =>
            site.siteList === chrome.developerPrivate.UserSiteSet.PERMITTED);
    if (areAllPermitted) {
      return loadTimeData.getString('permittedSites');
    }

    const areAllRestricted = this.data.sites.every(
        site =>
            site.siteList === chrome.developerPrivate.UserSiteSet.RESTRICTED);
    return areAllRestricted ? loadTimeData.getString('restrictedSites') : '';
  }

  private getSiteSubtext_(siteList: chrome.developerPrivate.UserSiteSet):
      string {
    return loadTimeData.getString(
        siteList === chrome.developerPrivate.UserSiteSet.PERMITTED ?
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
}

declare global {
  interface HTMLElementTagNameMap {
    'site-permissions-site-group': SitePermissionsSiteGroupElement;
  }
}

customElements.define(
    SitePermissionsSiteGroupElement.is, SitePermissionsSiteGroupElement);
