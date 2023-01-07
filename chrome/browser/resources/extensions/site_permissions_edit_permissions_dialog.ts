// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import './strings.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './site_permissions_edit_permissions_dialog.html.js';
import {SiteSettingsDelegate} from './site_settings_mixin.js';

interface ExtensionSiteAccessInfo {
  id: string;
  name: string;
  iconUrl: string;
  siteAccess: string;
}

export interface SitePermissionsEditPermissionsDialogElement {
  $: {
    dialog: CrDialogElement,
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
    I18nMixin(PolymerElement);

export class SitePermissionsEditPermissionsDialogElement extends
    SitePermissionsEditPermissionsDialogElementBase {
  static get is() {
    return 'site-permissions-edit-permissions-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      delegate: Object,
      extensions: {
        type: Array,
        value: () => [],
        observer: 'onExtensionsUpdated_',
      },

      /**
       * The current siteSet for `site`, as stored in the backend. Specifies
       * whether `site` is a user specified permitted or restricted site, or is
       * a pattern specified by an extension's host permissions..
       */
      originalSiteSet: String,

      /**
       * The url of the site whose permissions are currently being edited.
       */
      site: String,

      /**
       * The temporary siteSet for `site` as displayed in the dialog. Will be
       * saved to the backend when the dialog is submitted.
       */
      siteSet_: {
        type: String,
        observer: 'onSiteSetUpdated_',
      },

      siteSetEnum_: {
        type: Object,
        value: chrome.developerPrivate.SiteSet,
      },

      extensionSiteAccessData_: {
        type: Array,
        value: () => [],
      },

      /**
       * Proxying the enum to be used easily by the html template.
       */
      hostAccessEnum_: {
        type: Object,
        value: chrome.developerPrivate.HostAccess,
      },
    };
  }

  delegate: SiteSettingsDelegate;
  extensions: chrome.developerPrivate.ExtensionInfo[];
  originalSiteSet: chrome.developerPrivate.SiteSet;
  site: string;
  private siteSet_: chrome.developerPrivate.SiteSet;
  private extensionsIdToInfo_:
      Map<string, chrome.developerPrivate.ExtensionInfo>;
  private extensionSiteAccessData_: ExtensionSiteAccessInfo[];

  override ready() {
    super.ready();
    this.siteSet_ = this.originalSiteSet;

    // If `this.site` matches subdomains, then it should not be a user specified
    // site.
    assert(
        !this.matchesSubdomains_() ||
        this.originalSiteSet === EXTENSION_SPECIFIED);

    this.updateExtensionSiteAccessData_(this.siteSet_);
  }

  private onExtensionsUpdated_(extensions:
                                   chrome.developerPrivate.ExtensionInfo[]) {
    this.extensionsIdToInfo_ = new Map();
    for (const extension of extensions) {
      this.extensionsIdToInfo_.set(extension.id, extension);
    }
    this.updateExtensionSiteAccessData_(this.siteSet_);
  }

  private onSiteSetUpdated_(siteSet: chrome.developerPrivate.SiteSet) {
    this.updateExtensionSiteAccessData_(siteSet);
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
        this.isSiteHostOnly_() ? `*://${this.site}/` : this.site;
    const matchingExtensionsInfo =
        await this.delegate.getMatchingExtensionsForSite(siteToCheck);
    const extensionSiteAccessData: ExtensionSiteAccessInfo[] = [];
    matchingExtensionsInfo.forEach(({id, siteAccess}) => {
      assert(this.extensionsIdToInfo_.has(id));
      const {name, iconUrl} = this.extensionsIdToInfo_.get(id)!;
      extensionSiteAccessData.push({id, name, iconUrl, siteAccess});
    });

    this.extensionSiteAccessData_ = extensionSiteAccessData;
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private onSubmitClick_() {
    if (this.siteSet_ === this.originalSiteSet) {
      this.$.dialog.close();
      return;
    }

    // If `this.site` has a scheme (and can be considered a full url), use it
    // as is. Otherwise if `this.site` is just a host, append the http and https
    // schemes to it.
    const sitesToChange = this.isSiteHostOnly_() ?
        [`http://${this.site}`, `https://${this.site}`] :
        [this.site];
    if (this.siteSet_ === EXTENSION_SPECIFIED) {
      this.delegate
          .removeUserSpecifiedSites(this.originalSiteSet, sitesToChange)
          .then(() => {
            this.$.dialog.close();
          });
    } else {
      this.delegate.addUserSpecifiedSites(this.siteSet_, sitesToChange)
          .then(() => {
            this.$.dialog.close();
          });
    }
  }

  private computeDialogTitle_(): string {
    return this.i18n('sitePermissionsEditPermissionsDialogTitle', this.site);
  }

  private getPermittedSiteLabel_(): string {
    return this.i18n('editSitePermissionsAllowAllExtensions', this.site);
  }

  private getRestrictedSiteLabel_(): string {
    return this.i18n('editSitePermissionsRestrictExtensions', this.site);
  }

  private matchesSubdomains_(): boolean {
    // Sites that match all subdomains for a given host will specify "*.<host>".
    // Given how sites are specified as origins for user specified sites and how
    // extension host permissions are specified, it should be safe to assume
    // that "*." will only be used to match subdomains.
    return this.site.includes('*.');
  }

  private showExtensionSiteAccessData_(): boolean {
    return this.siteSet_ === EXTENSION_SPECIFIED;
  }

  private getDialogBodyContainerClass_(): string {
    return this.matchesSubdomains_() ? 'site-access-list' :
                                       'indented-site-access-list';
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
