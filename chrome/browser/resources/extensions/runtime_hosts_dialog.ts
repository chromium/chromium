// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './strings.m.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemDelegate} from './item.js';

// A RegExp to roughly match acceptable patterns entered by the user.
// exec'ing() this RegExp will match the following groups:
// 0: Full matched string.
// 1: Scheme + scheme separator (e.g., 'https://').
// 2: Scheme only (e.g., 'https').
// 3: Match subdomains ('*.').
// 4: Hostname (e.g., 'example.com').
// 5: Port, including ':' separator (e.g., ':80').
// 6: Path, include '/' separator (e.g., '/*').
const patternRegExp = new RegExp(
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
  const res = patternRegExp.exec(site)!;
  assert(res);
  const scheme = res[1] || '*://';
  const host = (res[3] || '') + res[4];
  const port = res[5] || '';
  const path = '/*';
  return scheme + host + port + path;
}

interface ExtensionsRuntimeHostsDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

class ExtensionsRuntimeHostsDialogElement extends PolymerElement {
  static get is() {
    return 'extensions-runtime-hosts-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      delegate: Object,

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
    };
  }

  delegate: ItemDelegate;
  itemId: string;
  currentSite: string|null;
  updateHostAccess: boolean;
  private site_: string;
  private inputInvalid_: boolean;

  connectedCallback() {
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

    const valid = patternRegExp.test(this.site_);
    this.inputInvalid_ = !valid;
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

  private onCancelTap_() {
    this.$.dialog.cancel();
  }

  /**
   * The tap handler for the submit button (adds the pattern and closes
   * the dialog).
   */
  private onSubmitTap_() {
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
    this.delegate.removeRuntimeHostPermission(this.itemId, this.currentSite!)
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
    this.delegate.addRuntimeHostPermission(this.itemId, pattern)
        .then(
            () => {
              this.$.dialog.close();
            },
            () => {
              this.inputInvalid_ = true;
            });
  }
}

customElements.define(
    ExtensionsRuntimeHostsDialogElement.is,
    ExtensionsRuntimeHostsDialogElement);
