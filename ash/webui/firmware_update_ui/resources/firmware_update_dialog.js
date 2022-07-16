// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './firmware_shared_css.js';
import './firmware_shared_fonts.js';

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FirmwareUpdate} from './firmware_update_types.js';

/** @enum {number} */
export const DialogState = {
  CLOSED: 0,
  DEVICE_PREP: 1,
  UPDATING: 2,
};

/**
 * @fileoverview
 * 'firmware-update-dialog' displays information related to a firmware update.
 */
export class FirmwareUpdateDialogElement extends PolymerElement {
  static get is() {
    return 'firmware-update-dialog';
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

      /** @type {!DialogState} */
      dialogState: {
        type: Number,
        value: DialogState.CLOSED,
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /**
     * Event callback for 'open-device-prep-dialog'.
     * @param {!Event} e
     * @private
     */
    this.openDevicePrepDialog_ = (e) => {
      this.update = e.detail.update;
      this.dialogState = DialogState.DEVICE_PREP;
    };

    /**
     * Event callback for 'open-update-dialog'.
     * @param {!Event} e
     * @private
     */
    this.openUpdateDialog_ = (e) => {
      this.update = e.detail.update;
      this.dialogState = DialogState.UPDATING;
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    window.addEventListener(
        'open-device-prep-dialog', (e) => this.openDevicePrepDialog_(e));

    window.addEventListener(
        'open-update-dialog', (e) => this.openUpdateDialog_(e));
  }

  /**
   * @protected
   * @return {boolean}
   */
  shouldShowDevicePrepDialog_() {
    return this.dialogState === DialogState.DEVICE_PREP;
  }

  /** @protected */
  closeDialog_() {
    this.dialogState = DialogState.CLOSED;
  }

  /** @protected */
  startUpdate_() {
    // TODO(michaelcheco): Start update.
    this.dialogState = DialogState.UPDATING;
  }

  /**
   * @protected
   * @return {boolean}
   */
  shouldShowUpdateDialog_() {
    // TODO(michaelchheco): Update when 'UPDATE_DONE' is added to the
    // |DialogState| enum.
    return this.isUpdateInProgress_();
  }

  /**
   * @protected
   * @return {number}
   */
  computePercentageValue_() {
    // TODO(michaelcheco): Dynamically update this when 'onProgressChanged'
    // observer is implemented.
    return 0;
  }

  /**
   * @protected
   * @return {boolean}
   */
  isUpdateInProgress_() {
    return this.dialogState === DialogState.UPDATING;
  }

  /**
   * @protected
   * @return {string}
   */
  computeUpdateDialogTitle_() {
    if (this.isUpdateInProgress_()) {
      return `Updating ${this.update.deviceName}`;
    }
    return '';
  }

  /**
   * @protected
   * @return {string}
   */
  computeProgressText_() {
    // TODO(michaelcheco): Dynamically update this when 'onProgressChanged'
    // observer is implemented.
    return 'Installing (1%)';
  }
}

customElements.define(
    FirmwareUpdateDialogElement.is, FirmwareUpdateDialogElement);
