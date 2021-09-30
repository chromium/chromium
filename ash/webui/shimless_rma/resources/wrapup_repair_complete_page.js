// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'wrapup-repair-complete-page' is the main landing page for the shimless rma
 * process.
 */
export class WrapupRepairCompletePage extends PolymerElement {
  static get is() {
    return 'wrapup-repair-complete-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  ready() {
    super.ready();
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: false},
        ));
  }

  /** @protected */
  onDiagnosticsButtonClick_() {}

  /** @protected */
  onShutdownClick_() {}

  /** @protected */
  onRmaLogButtonClick_() {
    const dialog = /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#logsDialog'));
    if (!dialog.open) {
      dialog.showModal();
    }
  }

  /** @protected */
  onBatteryCutButtonClick_() {
    const dialog = /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#batteryCutDialog'));
    if (!dialog.open) {
      dialog.showModal();
    }
  }

  /** @protected */
  onCancelClick_() {
    const dialogs = /** @type {!NodeList<!CrDialogElement>} */ (
        this.shadowRoot.querySelectorAll('cr-dialog'));
    Array.from(dialogs).map((dialog) => {
      dialog.close();
    });
  }
};

customElements.define(WrapupRepairCompletePage.is, WrapupRepairCompletePage);
