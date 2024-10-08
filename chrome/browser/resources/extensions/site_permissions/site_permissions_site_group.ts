// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '../strings.m.js';
import './site_permissions_edit_permissions_dialog.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getFaviconUrl, matchesSubdomains, SUBDOMAIN_SPECIFIER} from '../url_util.js';

import {getCss} from './site_permissions_site_group.css.js';
import {getHtml} from './site_permissions_site_group.html.js';
import type {SiteSettingsDelegate} from './site_settings_mixin.js';
import {DummySiteSettingsDelegate} from './site_settings_mixin.js';

export interface SitePermissionsSiteGroupElement {
  $: {
    etldOrSite: HTMLElement,
    etldOrSiteIncludesSubdomains: HTMLElement,
    etldOrSiteSubtext: HTMLElement,
  };
}

export class SitePermissionsSiteGroupElement extends CrLitElement {
  static get is() {
    return 'site-permissions-site-group';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data: {type: Object},
      delegate: {type: Object},
      extensions: {type: Array},
      listIndex: {type: Number},
      expanded_: {type: Boolean},
      showEditSitePermissionsDialog_: {type: Boolean},
      siteToEdit_: {type: Object},
    };
  }

  data: chrome.developerPrivate.SiteGroup = {
    etldPlusOne: '',
    numExtensions: 0,
    sites: [],
  };
  delegate: SiteSettingsDelegate = new DummySiteSettingsDelegate();
  extensions: chrome.developerPrivate.ExtensionInfo[] = [];
  listIndex: number = -1;
  protected expanded_: boolean = false;
  protected showEditSitePermissionsDialog_: boolean = false;
  protected siteToEdit_: chrome.developerPrivate.SiteInfo|null = null;

  protected getEtldOrSiteFaviconUrl_(): string {
    return getFaviconUrl(this.getDisplayUrl_());
  }

  protected getFaviconUrl_(url: string): string {
    return getFaviconUrl(url);
  }

  protected isExpandable_(): boolean {
    return this.data.sites.length > 1;
  }

  protected getClassForIndex_(): string {
    return this.listIndex > 0 ? 'hr' : '';
  }

  protected getDisplayUrl_(): string {
    return this.data.sites.length === 1 ?
        this.getSiteWithoutSubdomainSpecifier_(this.data.sites[0].site) :
        this.data.etldPlusOne;
  }

  protected getEtldOrSiteSubText_(): string {
    // TODO(crbug.com/40199251): Revisit what to show for this eTLD+1 group's
    // subtext. For now, default to showing no text if there is any mix of sites
    // under the group (i.e. user permitted/restricted/specified by extensions).
    if (this.data.sites.length === 0) {
      return '';
    }
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
        this.getExtensionCountText_(this.data.numExtensions);
  }

  protected getSiteWithoutSubdomainSpecifier_(site: string): string {
    return site.replace(SUBDOMAIN_SPECIFIER, '');
  }

  protected etldOrFirstSiteMatchesSubdomains_(): boolean {
    const site = this.data.sites.length === 1 ? this.data.sites[0].site :
                                                this.data.etldPlusOne;
    return matchesSubdomains(site);
  }

  protected matchesSubdomains_(site: string): boolean {
    return matchesSubdomains(site);
  }

  protected getSiteSubtext_(siteInfo: chrome.developerPrivate.SiteInfo):
      string {
    if (siteInfo.numExtensions > 0) {
      return this.getExtensionCountText_(siteInfo.numExtensions);
    }

    return loadTimeData.getString(
        siteInfo.siteSet === chrome.developerPrivate.SiteSet.USER_PERMITTED ?
            'permittedSites' :
            'restrictedSites');
  }

  // TODO(crbug.com/40251278): Use PluralStringProxyImpl to retrieve the
  // extension count text. However, this is non-trivial in this component as
  // some of the strings are nestled inside dom-repeats and plural strings are
  // currently retrieved asynchronously, and would need to be set directly on a
  // property when retrieved.
  private getExtensionCountText_(numExtensions: number): string {
    return numExtensions === 1 ?
        loadTimeData.getString('sitePermissionsAllSitesOneExtension') :
        loadTimeData.getStringF(
            'sitePermissionsAllSitesExtensionCount', numExtensions);
  }

  protected onEditSiteClick_() {
    this.siteToEdit_ = this.data.sites[0];
    this.showEditSitePermissionsDialog_ = true;
  }

  protected onEditSiteInListClick_(e: Event) {
    const index = Number((e.target as HTMLElement).dataset['index']);
    this.siteToEdit_ = this.data.sites[index] || null;
    this.showEditSitePermissionsDialog_ = true;
  }

  protected onEditSitePermissionsDialogClose_() {
    this.showEditSitePermissionsDialog_ = false;
    assert(this.siteToEdit_, 'Site To Edit');
    this.siteToEdit_ = null;
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-permissions-site-group': SitePermissionsSiteGroupElement;
  }
}

customElements.define(
    SitePermissionsSiteGroupElement.is, SitePermissionsSiteGroupElement);
