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

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './site_permissions_site_group.html.js';
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

      expanded_: {
        type: Boolean,
        value: false,
      },

      isExpandable_: {
        type: Boolean,
        computed: 'computeIsExpandable_(data.sites)',
      },
    };
  }

  data: chrome.developerPrivate.SiteGroup;
  private expanded_: boolean;
  private isExpandable_: boolean;

  private getEtldOrSiteFaviconUrl_(): string {
    return getFaviconUrl(this.getDisplayUrl_());
  }

  private getFaviconUrl_(url: string): string {
    return getFaviconUrl(url);
  }

  private computeIsExpandable_(): boolean {
    return this.data.sites.length > 1;
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
}

declare global {
  interface HTMLElementTagNameMap {
    'site-permissions-site-group': SitePermissionsSiteGroupElement;
  }
}

customElements.define(
    SitePermissionsSiteGroupElement.is, SitePermissionsSiteGroupElement);
