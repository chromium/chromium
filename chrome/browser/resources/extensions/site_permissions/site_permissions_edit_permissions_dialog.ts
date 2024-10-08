// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '../strings.m.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getItemSource, SourceType} from '../item_util.js';
import {matchesSubdomains, SUBDOMAIN_SPECIFIER} from '../url_util.js';

import {getCss} from './site_permissions_edit_permissions_dialog.css.js';
import {getHtml} from './site_permissions_edit_permissions_dialog.html.js';
import type {SiteSettingsDelegate} from './site_settings_mixin.js';
import {DummySiteSettingsDelegate} from './site_settings_mixin.js';

interface ExtensionSiteAccessInfo {
  id: string;
  name: string;
  iconUrl: string;
  siteAccess: chrome.developerPrivate.HostAccess;
  addedByPolicy: boolean;
  canRequestAllSites: boolean;
}

export interface SitePermissionsEditPermissionsDialogElement {
  $: {
    dialog: CrDialogElement,
    includesSubdomains: HTMLElement,
    site: HTMLElement,
    submit: CrButtonElement,
  };
}

const EXTENSION_SPECIFIED = chrome.developerPrivate.SiteSet.EXTENSION_SPECIFIED;

// A list of possible schemes that can be specified by extension host
// permissions. This is derived from URLPattern::SchemeMasks.
const VALID_SCHEMES = [
  '*',
  'http',
  'https',
  'file',
  'ftp',
  'chrome',
  'chrome-extension',
  'filesystem',
  'ftp',
  'ws',
  'wss',
  'data',
  'uuid-in-package',
];

const SitePermissionsEditPermissionsDialogElementBase =
    I18nMixinLit(CrLitElement);

export class SitePermissionsEditPermissionsDialogElement extends
    SitePermissionsEditPermissionsDialogElementBase {
  static get is() {
    return 'site-permissions-edit-permissions-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      delegate: {type: Object},
      extensions: {type: Array},

      /**
       * The current siteSet for `site`, as stored in the backend. Specifies
       * whether `site` is a user specified permitted or restricted site, or is
       * a pattern specified by an extension's host permissions..
       */
      originalSiteSet: {type: String},

      /**
       * The url of the site whose permissions are currently being edited.
       */
      site: {type: String},

      /**
       * The temporary siteSet for `site` as displayed in the dialog. Will be
       * saved to the backend when the dialog is submitted.
       */
      siteSet_: {type: String},

      extensionSiteAccessData_: {type: Array},
      showPermittedOption_: {type: Boolean},
    };
  }

  delegate: SiteSettingsDelegate = new DummySiteSettingsDelegate();
  extensions: chrome.developerPrivate.ExtensionInfo[] = [];
  originalSiteSet: chrome.developerPrivate.SiteSet =
      chrome.developerPrivate.SiteSet.USER_PERMITTED;
  site: string = '';
  protected siteSet_: chrome.developerPrivate.SiteSet =
      chrome.developerPrivate.SiteSet.USER_PERMITTED;
  private extensionsIdToInfo_:
      Map<string, chrome.developerPrivate.ExtensionInfo> = new Map();
  protected extensionSiteAccessData_: ExtensionSiteAccessInfo[] = [];
  protected showPermittedOption_: boolean =
      loadTimeData.getBoolean('enableUserPermittedSites');

  // Tracks any unsaved changes to HostAccess for each extension made by
  // changing the value in the ".extension-host-access" <select> element. Any
  // values in here should be different than the HostAccess for the extension
  // inside `extensionSiteAccessData_`.
  private unsavedExtensionsIdToHostAccess_:
      Map<string, chrome.developerPrivate.HostAccess> = new Map();

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    this.siteSet_ = this.originalSiteSet;
    this.updateExtensionSiteAccessData_(this.siteSet_);

    // If `this.site` matches subdomains, then it should not be a user specified
    // site.
    assert(
        !this.matchesSubdomains_() ||
        this.originalSiteSet === EXTENSION_SPECIFIED);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('extensions')) {
      this.onExtensionsUpdated_();
    }
  }

  private onExtensionsUpdated_() {
    this.extensionsIdToInfo_ = new Map();
    for (const extension of this.extensions) {
      this.extensionsIdToInfo_.set(extension.id, extension);
    }
    this.updateExtensionSiteAccessData_(this.siteSet_);
  }

  protected onSiteSetChanged_(
      e: CustomEvent<{value: chrome.developerPrivate.SiteSet}>) {
    this.siteSet_ = e.detail.value;
    this.updateExtensionSiteAccessData_(this.siteSet_);
  }

  // Returns true if this.site is a just a host by checking whether or not it
  // starts with a valid scheme. If not, assume the site is a full URL.
  // Different components that use this dialog may supply either a URL or just a
  // host.
  private isSiteHostOnly_(): boolean {
    return !VALID_SCHEMES.some(scheme => this.site.startsWith(`${scheme}://`));
  }

  // Fetches all extensions that have requested access to `this.site` along with
  // their access status. This information is joined with some fields in
  // `this.extensions` to update `this.extensionSiteAccessData_`.
  private async updateExtensionSiteAccessData_(
      siteSet: chrome.developerPrivate.SiteSet) {
    // Avoid fetching the list of matching extensions if they will not be
    // displayed.
    if (siteSet !== EXTENSION_SPECIFIED) {
      return;
    }

    const siteToCheck =
        this.isSiteHostOnly_() ? `*://${this.site}/` : `${this.site}/`;

    const matchingExtensionsInfo =
        await this.delegate.getMatchingExtensionsForSite(siteToCheck);

    const extensionSiteAccessData: ExtensionSiteAccessInfo[] = [];
    matchingExtensionsInfo.forEach(({id, siteAccess, canRequestAllSites}) => {
      assert(this.extensionsIdToInfo_.has(id));
      const {name, iconUrl} = this.extensionsIdToInfo_.get(id)!;
      const addedByPolicy = getItemSource(this.extensionsIdToInfo_.get(id)!) ===
          SourceType.POLICY;
      extensionSiteAccessData.push(
          {id, name, iconUrl, siteAccess, addedByPolicy, canRequestAllSites});

      // Remove the unsaved HostAccess from `unsavedExtensionsIdToHostAccess_`
      // if it is now the same as `siteAccess`.
      if (this.unsavedExtensionsIdToHostAccess_.get(id) === siteAccess) {
        this.unsavedExtensionsIdToHostAccess_.delete(id);
      }
    });

    // Remove any HostAccess from `unsavedExtensionsIdToHostAccess_` for
    // extensions that are no longer in `extensionSiteAccessData`.
    for (const extensionId of this.unsavedExtensionsIdToHostAccess_.keys()) {
      if (!this.extensionsIdToInfo_.has(extensionId)) {
        this.unsavedExtensionsIdToHostAccess_.delete(extensionId);
      }
    }

    this.extensionSiteAccessData_ = extensionSiteAccessData;
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }

  protected async onSubmitClick_() {
    if (this.siteSet_ !== this.originalSiteSet) {
      // If `this.site` has a scheme (and can be considered a full url), use it
      // as is. Otherwise if `this.site` is just a host, append the http and
      // https schemes to it.
      const sitesToChange = this.isSiteHostOnly_() ?
          [`http://${this.site}`, `https://${this.site}`] :
          [this.site];
      if (this.siteSet_ === EXTENSION_SPECIFIED) {
        await this.delegate.removeUserSpecifiedSites(
            this.originalSiteSet, sitesToChange);
      } else {
        await this.delegate.addUserSpecifiedSites(this.siteSet_, sitesToChange);
      }
    }

    if (this.siteSet_ === EXTENSION_SPECIFIED &&
        this.unsavedExtensionsIdToHostAccess_.size) {
      const updates: chrome.developerPrivate.ExtensionSiteAccessUpdate[] = [];
      this.unsavedExtensionsIdToHostAccess_.forEach((val, key) => {
        updates.push({id: key, siteAccess: val});
      });

      // For changing extensions' site access, first. the wildcard path "/*" is
      // added to the end. Then, if the site does not specify a scheme, use the
      // wildcard scheme.
      const siteToUpdate =
          this.isSiteHostOnly_() ? `*://${this.site}/` : `${this.site}/`;
      await this.delegate.updateSiteAccess(siteToUpdate, updates);
    }

    this.$.dialog.close();
  }

  protected getSiteWithoutSubdomainSpecifier_(): string {
    return this.site.replace(SUBDOMAIN_SPECIFIER, '');
  }

  protected getPermittedSiteLabel_(): string {
    return this.i18n('editSitePermissionsAllowAllExtensions', this.site);
  }

  protected getRestrictedSiteLabel_(): string {
    return this.i18n('editSitePermissionsRestrictExtensions', this.site);
  }

  protected matchesSubdomains_(): boolean {
    return matchesSubdomains(this.site);
  }

  protected showExtensionSiteAccessData_(): boolean {
    return this.siteSet_ === EXTENSION_SPECIFIED;
  }

  protected getDialogBodyContainerClass_(): string {
    return this.matchesSubdomains_() ? 'site-access-list' :
                                       'indented-site-access-list';
  }

  // Returns whether a <select> <option> is selected for the
  // extension's host access. This shows the unsaved HostAccess value that was
  // changed by the user. Otherwise, show the preexisting HostAccess value.
  protected isSelected_(
      extensionId: string,
      originalSiteAccess: chrome.developerPrivate.HostAccess,
      option: chrome.developerPrivate.HostAccess): boolean {
    const selectedValue =
        this.unsavedExtensionsIdToHostAccess_.get(extensionId) ||
        originalSiteAccess;
    return selectedValue === option;
  }

  protected onHostAccessChange_(e: Event) {
    const selectMenu = e.target as HTMLSelectElement;
    assert(selectMenu);

    const index = Number(selectMenu.dataset['index']);
    const item = this.extensionSiteAccessData_[index]!;
    const originalSiteAccess = item.siteAccess;
    const newSiteAccess =
        selectMenu.value as chrome.developerPrivate.HostAccess;

    // Sanity check that extensions that don't request all sites access cannot
    // request all sites access from the dialog.
    assert(
        item.canRequestAllSites ||
        newSiteAccess !== chrome.developerPrivate.HostAccess.ON_ALL_SITES);

    if (originalSiteAccess === newSiteAccess) {
      this.unsavedExtensionsIdToHostAccess_.delete(item.id);
    } else {
      this.unsavedExtensionsIdToHostAccess_.set(item.id, newSiteAccess);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-permissions-edit-permissions-dialog':
        SitePermissionsEditPermissionsDialogElement;
  }
}

customElements.define(
    SitePermissionsEditPermissionsDialogElement.is,
    SitePermissionsEditPermissionsDialogElement);
