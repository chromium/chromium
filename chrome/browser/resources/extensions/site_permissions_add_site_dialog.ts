// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './strings.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SitePermissionsDelegate} from './site_permissions.js';

// A RegExp to roughly match acceptable patterns entered by the user.
// exec'ing() this RegExp will match the following groups:
// 0: Full matched string.
// 1: Scheme + scheme separator (e.g., 'https://').
// 2: Scheme only (e.g., 'https').
// 3: Hostname (e.g., 'example.com').
// 4: Port, including ':' separator (e.g., ':80').
const patternRegExp = new RegExp(
    '^' +
    // Scheme; optional.
    '((http|https|\\*)://)?' +
    // Hostname or localhost, required.
    '([a-z0-9\\.-]+\\.[a-z0-9]+|localhost)' +
    // Port, optional.
    '(:[0-9]+)?' +
    '$');

export function getSitePermissionsPatternFromSite(site: string): string {
  const res = patternRegExp.exec(site)!;
  assert(res);
  const scheme = res[1] || 'https://';
  const host = res[3];
  const port = res[4] || '';
  return scheme + host + port;
}

export interface SitePermissionsAddSiteDialogElement {
  $: {
    dialog: CrDialogElement,
    submit: CrButtonElement,
  };
}

export class SitePermissionsAddSiteDialogElement extends PolymerElement {
  static get is() {
    return 'site-permissions-add-site-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      delegate: Object,
      siteSet: String,

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

  delegate: SitePermissionsDelegate;
  siteSet: chrome.developerPrivate.UserSiteSet;
  private site_: string;
  private inputValid_: boolean;

  /**
   * Validates that the pattern entered is valid by testing it against the
   * regex. An empty patterh is considered "valid" as the invalid message will
   * not be shown, but the input cannot be submitted as the action button will
   * be disabled.
   */
  private validate_() {
    this.inputValid_ =
        this.site_.trim().length === 0 || patternRegExp.test(this.site_);
  }

  private computeSubmitButtonDisabled_(): boolean {
    // If input is empty, disable the action button.
    return !this.inputValid_ || this.site_.trim().length === 0;
  }

  private onCancel_() {
    this.$.dialog.cancel();
  }

  private onSubmit_() {
    const pattern = getSitePermissionsPatternFromSite(this.site_);
    this.delegate.addUserSpecifiedSite(this.siteSet, pattern)
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
    'site-permissions-add-site-dialog': SitePermissionsAddSiteDialogElement;
  }
}

customElements.define(
    SitePermissionsAddSiteDialogElement.is,
    SitePermissionsAddSiteDialogElement);
