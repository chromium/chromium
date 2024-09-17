// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './strings.m.js';
import './shared_vars.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './runtime_hosts_dialog.html.js';
import {sitePermissionsPatternRegExp} from './site_permissions/site_permissions_edit_url_dialog.js';
import {SiteSettingsMixin} from './site_permissions/site_settings_mixin.js';

// A RegExp to roughly match acceptable patterns entered by the user.
// exec'ing() this RegExp will match the following groups:
// 0: Full matched string.
// 1: Scheme + scheme separator (e.g., 'https://').
// 2: Scheme only (e.g., 'https').
// 3: Match subdomains ('*.').
// 4: Hostname (e.g., 'example.com').
// 5: Port, including ':' separator (e.g., ':80').
// 6: Path, include '/' separator (e.g., '/*').
const runtimeHostsPatternRegExp = new RegExp(
    '^' +
    // Scheme; optional.
    '((http|https|\\*)://)?' +
    // Include subdomains specifier; optional.
    '(\\*\\.)?' +
    // Hostname or localhost, required.
    '([a-z0-9\\.-]+\\.[a-z0-9]+|localhost)' +
    // Port, optional.
    '(:[0-9]+)?' +
    // Path, optional but if present must be '/' or '/*'.
    '(\\/\\*|\\/)?' +
    '$');

export function getPatternFromSite(site: string): string {
  const res = runtimeHostsPatternRegExp.exec(site)!;
  assert(res);
  const scheme = res[1] || '*://';
  const host = (res[3] || '') + res[4];
  const port = res[5] || '';
  const path = '/*';
  return scheme + host + port + path;
}

// Returns the sublist of `userSites` which match the pattern specified by
// `host`.
export function getMatchingUserSpecifiedSites(
    userSites: string[], host: string): string[] {
  if (!runtimeHostsPatternRegExp.test(host)) {
    return [];
  }

  const newHostRes = runtimeHostsPatternRegExp.exec(host);
  assert(newHostRes);

  const matchAllSchemes = !newHostRes[1] || newHostRes[1] === '*://';
  const matchAllSubdomains = newHostRes[3] === '*.';

  // For each restricted site, break it down into
  // `sitePermissionsPatternRegExp` components and check against components
  // from `newHostRes`.
  return userSites.filter((userSite: string) => {
    const siteRes = sitePermissionsPatternRegExp.exec(userSite);
    assert(siteRes);

    // Check if schemes match, unless `newHostRes` has a wildcard scheme.
    if (!matchAllSchemes && newHostRes[1] !== siteRes[1]) {
      return false;
    }

    // Check if host names match. If `matchAllSubdomains` is specified, check
    // that `newHostRes[4]` is a suffix of `siteRes[3]`
    if (matchAllSubdomains && !siteRes[3].endsWith(newHostRes[4])) {
      return false;
    }
    if (!matchAllSubdomains && siteRes[3] !== newHostRes[4]) {
      return false;
    }

    // Ports match if:
    //  - both are unspecified
    //  - both are specified and are an exact match
    //  - specified for `restrictedSite` but not `this,site_`
    return !newHostRes[5] || newHostRes[5] === siteRes[4];
  });
}

export interface ExtensionsRuntimeHostsDialogElement {
  $: {
    dialog: CrDialogElement,
    submit: CrButtonElement,
  };
}

const ExtensionsRuntimeHostsDialogElementBase =
    I18nMixin(SiteSettingsMixin(PolymerElement));

export class ExtensionsRuntimeHostsDialogElement extends
    ExtensionsRuntimeHostsDialogElementBase {
  static get is() {
    return 'extensions-runtime-hosts-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      itemId: String,

      /**
       * The site that this entry is currently managing. Only non-empty if this
       * is for editing an existing entry.
       */
      currentSite: {
        type: String,
        value: null,
      },

      /**
       * Whether the dialog should update the host access to be "on specific
       * sites" before adding a new host permission.
       */
      updateHostAccess: {
        type: Boolean,
        value: false,
      },

      /** The site to add an exception for. */
      site_: String,

      /** Whether the currently-entered input is valid. */
      inputInvalid_: {
        type: Boolean,
        value: false,
      },

      /**
       * the list of user specified restricted sites that match with `site_` if
       * `site_` is valid.
       */
      matchingRestrictedSites_: {
        type: Array,
        computed: 'computeMatchingRestrictedSites_(site_, restrictedSites)',
      },
    };
  }

  itemId: string;
  currentSite: string|null;
  updateHostAccess: boolean;
  private site_: string;
  private inputInvalid_: boolean;
  private matchingRestrictedSites_: string[];

  override connectedCallback() {
    super.connectedCallback();

    if (this.currentSite !== null && this.currentSite !== undefined) {
      this.site_ = this.currentSite;
      this.validate_();
    }
    this.$.dialog.showModal();
  }

  isOpen(): boolean {
    return this.$.dialog.open;
  }

  /**
   * Validates that the pattern entered is valid.
   */
  private validate_() {
    // If input is empty, disable the action button, but don't show the red
    // invalid message.
    if (this.site_.trim().length === 0) {
      this.inputInvalid_ = false;
      return;
    }

    this.inputInvalid_ = !runtimeHostsPatternRegExp.test(this.site_);
  }

  private computeDialogTitle_(): string {
    const stringId = this.currentSite === null ? 'runtimeHostsDialogTitle' :
                                                 'hostPermissionsEdit';
    return loadTimeData.getString(stringId);
  }

  private computeSubmitButtonDisabled_(): boolean {
    return this.inputInvalid_ || this.site_ === undefined ||
        this.site_.trim().length === 0;
  }

  private computeSubmitButtonLabel_(): string {
    const stringId = this.currentSite === null ? 'add' : 'save';
    return loadTimeData.getString(stringId);
  }

  private computeMatchingRestrictedSites_(): string[] {
    return getMatchingUserSpecifiedSites(this.restrictedSites, this.site_);
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  /**
   * The tap handler for the submit button (adds the pattern and closes
   * the dialog).
   */
  private onSubmitClick_() {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.AddHostDialogSubmitted');
    if (this.currentSite !== null) {
      this.handleEdit_();
    } else {
      this.handleAdd_();
    }
  }

  /**
   * Handles adding a new site entry.
   */
  private handleAdd_() {
    assert(!this.currentSite);

    if (this.updateHostAccess) {
      this.delegate.setItemHostAccess(
          this.itemId, chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES);
    }

    this.addPermission_();
  }

  /**
   * Handles editing an existing site entry.
   */
  private handleEdit_() {
    assert(this.currentSite);
    assert(
        !this.updateHostAccess,
        'Editing host permissions should only be possible if the host ' +
            'access is already set to specific sites.');

    if (this.currentSite === this.site_) {
      // No change in values, so no need to update anything.
      this.$.dialog.close();
      return;
    }

    // Editing an existing entry is done by removing the current site entry,
    // and then adding the new one.
    this.delegate.removeRuntimeHostPermission(this.itemId, this.currentSite)
        .then(() => {
          this.addPermission_();
        });
  }

  /**
   * Adds the runtime host permission through the delegate. If successful,
   * closes the dialog; otherwise displays the invalid input message.
   */
  private addPermission_() {
    const pattern = getPatternFromSite(this.site_);
    const restrictedSites = this.matchingRestrictedSites_;
    this.delegate.addRuntimeHostPermission(this.itemId, pattern)
        .then(
            () => {
              if (restrictedSites.length) {
                this.delegate.removeUserSpecifiedSites(
                    chrome.developerPrivate.SiteSet.USER_RESTRICTED,
                    restrictedSites);
              }
              this.$.dialog.close();
            },
            () => {
              this.inputInvalid_ = true;
            });
  }

  /**
   * Returns a warning message containing the first restricted site that
   * overlaps with `this.site_`, or an empty string if there are no matching
   * restricted sites.
   */
  private computeMatchingRestrictedSitesWarning_(): string {
    return this.matchingRestrictedSites_.length ?
        this.i18n(
            'matchingRestrictedSitesWarning',
            this.matchingRestrictedSites_[0]) :
        '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-runtime-hosts-dialog': ExtensionsRuntimeHostsDialogElement;
  }
}

customElements.define(
    ExtensionsRuntimeHostsDialogElement.is,
    ExtensionsRuntimeHostsDialogElement);
