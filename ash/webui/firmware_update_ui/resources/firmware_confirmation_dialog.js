// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import './firmware_shared_css.js';
import './firmware_shared_fonts.js';
import './strings.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FirmwareUpdate} from './firmware_update_types.js';
import {mojoString16ToString} from './mojo_utils.js';

/**
 * @fileoverview
 * 'firmware-confirmation-dialog' provides information about the update and
 *  allows users to either cancel or begin the installation.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const FirmwareConfirmationDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class FirmwareConfirmationDialogElement extends
    FirmwareConfirmationDialogElementBase {
  static get is() {
    return 'firmware-confirmation-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!FirmwareUpdate} */
      update: {
        type: Object,
      },

      /** @type {boolean} */
      open: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @override */
  constructor() {
    super();
    /**
     * Event callback for 'open-confirmation-dialog'.
     * @param {!Event} e
     * @private
     */
    this.openConfirmationDialog_ = (e) => {
      this.update = e.detail.update;
      this.open = true;
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    window.addEventListener(
        'open-confirmation-dialog', (e) => this.openConfirmationDialog_(e));
  }

  /** @protected */
  openUpdateDialog_() {
    this.closeDialog_();
    this.dispatchEvent(new CustomEvent('open-update-dialog', {
      bubbles: true,
      composed: true,
      detail: {update: this.update, inflight: false},
    }));
  }

  /** @protected */
  closeDialog_() {
    this.open = false;
  }

  /**
   * @protected
   * @return {string}
   */
  computeTitle_() {
    return this.i18n(
        'confirmationTitle', mojoString16ToString(this.update.deviceName));
  }
}

customElements.define(
    FirmwareConfirmationDialogElement.is, FirmwareConfirmationDialogElement);
