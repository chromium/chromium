// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './strings.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './site_permissions_edit_url_dialog.html.js';
import {SiteSettingsDelegate} from './site_settings_mixin.js';

// A RegExp to roughly match acceptable patterns entered by the user.
// exec'ing() this RegExp will match the following groups:
// 0: Full matched string.
// 1: Scheme + scheme separator (e.g., 'https://').
// 2: Scheme only (e.g., 'https').
// 3: Hostname (e.g., 'example.com').
// 4: Port, including ':' separator (e.g., ':80').
export const sitePermissionsPatternRegExp = new RegExp(
    '^' +
    // Scheme; optional.
    '((http|https)://)?' +
    // Hostname or localhost, required.
    '([a-z0-9\\.-]+\\.[a-z0-9]+|localhost)' +
    // Port, optional.
    '(:[0-9]+)?' +
    '$');

export function getSitePermissionsPatternFromSite(site: string): string {
  const res = sitePermissionsPatternRegExp.exec(site)!;
  assert(res);
  const scheme = res[1] || 'https://';
  const host = res[3];
  const port = res[4] || '';
  return scheme + host + port;
}

export interface SitePermissionsEditUrlDialogElement {
  $: {
    dialog: CrDialogElement,
    submit: CrButtonElement,
  };
}

export class SitePermissionsEditUrlDialogElement extends PolymerElement {
  static get is() {
    return 'site-permissions-edit-url-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      delegate: Object,
      siteSet: String,

      /**
       * The site that this entry is currently managing. Only non-empty if this
       * is for editing an existing entry.
       */
      siteToEdit: {
        type: String,
        value: null,
      },

      site_: {
        type: String,
        value: '',
      },

      /** Whether the currently-entered input is valid. */
      inputValid_: {
        type: Boolean,
        value: true,
      },
    };
  }

  delegate: SiteSettingsDelegate;
  siteSet: chrome.developerPrivate.SiteSet;
  siteToEdit: string|null;
  private site_: string;
  private inputValid_: boolean;

  override connectedCallback() {
    super.connectedCallback();

    if (this.siteToEdit !== null) {
      this.site_ = this.siteToEdit;
      this.validate_();
    }
  }

  /**
   * Validates that the pattern entered is valid by testing it against the
   * regex. An empty patterh is considered "valid" as the invalid message will
   * not be shown, but the input cannot be submitted as the action button will
   * be disabled.
   */
  private validate_() {
    this.inputValid_ = this.site_.trim().length === 0 ||
        sitePermissionsPatternRegExp.test(this.site_);
  }

  private computeDialogTitle_(): string {
    return loadTimeData.getString(
        this.siteToEdit === null ? 'sitePermissionsAddSiteDialogTitle' :
                                   'sitePermissionsEditSiteDialogTitle');
  }

  private computeSubmitButtonDisabled_(): boolean {
    // If input is empty, disable the action button.
    return !this.inputValid_ || this.site_.trim().length === 0;
  }

  private computeSubmitButtonLabel_(): string {
    return loadTimeData.getString(this.siteToEdit === null ? 'add' : 'save');
  }

  private onCancel_() {
    this.$.dialog.cancel();
  }

  private onSubmit_() {
    const pattern = getSitePermissionsPatternFromSite(this.site_);
    if (this.siteToEdit !== null) {
      this.handleEdit_(pattern);
    } else {
      this.handleAdd_(pattern);
    }
  }

  private handleEdit_(pattern: string) {
    assert(this.siteToEdit);
    if (pattern === this.siteToEdit) {
      this.$.dialog.close();
      return;
    }

    this.delegate.removeUserSpecifiedSites(this.siteSet, [this.siteToEdit])
        .then(() => {
          this.addUserSpecifiedSite_(pattern);
        });
  }

  private handleAdd_(pattern: string) {
    assert(!this.siteToEdit);
    this.addUserSpecifiedSite_(pattern);
  }

  private addUserSpecifiedSite_(pattern: string) {
    this.delegate.addUserSpecifiedSites(this.siteSet, [pattern])
        .then(
            () => {
              this.$.dialog.close();
            },
            () => {
              this.inputValid_ = false;
            });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-permissions-edit-url-dialog': SitePermissionsEditUrlDialogElement;
  }
}

customElements.define(
    SitePermissionsEditUrlDialogElement.is,
    SitePermissionsEditUrlDialogElement);
