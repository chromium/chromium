// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './strings.m.js';
import './shared_style.js';
import './shared_vars.js';
import './site_permissions_list.js';

import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigation, Page} from './navigation_helper.js';
import {Service} from './service.js';

export interface SitePermissionsDelegate {
  getUserSiteSettings(): Promise<chrome.developerPrivate.UserSiteSettings>;
  addUserSpecifiedSite(
      siteSet: chrome.developerPrivate.UserSiteSet,
      host: string): Promise<void>;
  removeUserSpecifiedSite(
      siteSet: chrome.developerPrivate.UserSiteSet,
      host: string): Promise<void>;
}

export interface ExtensionsSitePermissionsElement {
  $: {
    allSitesLink: CrLinkRowElement,
  };
}

export class ExtensionsSitePermissionsElement extends PolymerElement {
  static get is() {
    return 'extensions-site-permissions';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      delegate: Object,

      permittedSites_: Array,

      restrictedSites_: Array,

      userSiteSetEnum_: {
        type: Object,
        value: chrome.developerPrivate.UserSiteSet,
      },
    };
  }

  delegate: SitePermissionsDelegate;
  private permittedSites_: string[];
  private restrictedSites_: string[];

  ready() {
    super.ready();
    const service = Service.getInstance();
    service.getUserSiteSettingsChangedTarget().addListener(
        this.onUserSiteSettingsChanged_.bind(this));
  }

  connectedCallback() {
    super.connectedCallback();
    this.delegate.getUserSiteSettings().then(
        ({permittedSites, restrictedSites}) => {
          this.permittedSites_ = permittedSites;
          this.restrictedSites_ = restrictedSites;
        });
  }

  private onUserSiteSettingsChanged_({permittedSites, restrictedSites}: chrome
                                         .developerPrivate.UserSiteSettings) {
    this.permittedSites_ = permittedSites;
    this.restrictedSites_ = restrictedSites;
  }

  private onAllSitesLinkClick_() {
    navigation.navigateTo({page: Page.SITE_PERMISSIONS_ALL_SITES});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-site-permissions': ExtensionsSitePermissionsElement;
  }
}

customElements.define(
    ExtensionsSitePermissionsElement.is, ExtensionsSitePermissionsElement);
