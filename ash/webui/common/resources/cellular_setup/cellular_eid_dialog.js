// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying cellular EID and QR code
 */
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {flush, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EuiccProperties, QRCode} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

import {getTemplate} from './cellular_eid_dialog.html.js';

// The size of each tile in pixels.
const QR_CODE_TILE_SIZE = 5;
// Styling for filled tiles in the QR code.
const QR_CODE_FILL_STYLE = '#000000';

Polymer({
  _template: getTemplate(),
  is: 'cellular-eid-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * The euicc object whose EID and QRCode should be shown in the dialog.
     */
    euicc: Object,

    /** @private */
    canvasSize_: Number,

    /** @private */
    eid_: String,
  },

  /**
   * @private {?CanvasRenderingContext2D}
   */
  canvasContext_: null,

  /** @override */
  attached() {
    if (!this.euicc) {
      return;
    }
    this.euicc.getEidQRCode().then(this.updateQRCode_.bind(this));
    this.euicc.getProperties().then(this.updateEid_.bind(this));
    requestAnimationFrame(() => {
      this.$.done.focus();
    });
  },

  /**@private */
  onDonePressed_() {
    this.$.eidDialog.close();
  },

  /**
   * @private
   * @param {{qrCode: QRCode} | null} response
   */
  updateQRCode_(response) {
    if (!response || !response.qrCode) {
      return;
    }
    this.canvasSize_ = response.qrCode.size * QR_CODE_TILE_SIZE;
    flush();
    const context = this.getCanvasContext_();
    context.clearRect(0, 0, this.canvasSize_, this.canvasSize_);
    context.fillStyle = QR_CODE_FILL_STYLE;
    let index = 0;
    for (let x = 0; x < response.qrCode.size; x++) {
      for (let y = 0; y < response.qrCode.size; y++) {
        if (response.qrCode.data[index]) {
          context.fillRect(
              x * QR_CODE_TILE_SIZE, y * QR_CODE_TILE_SIZE, QR_CODE_TILE_SIZE,
              QR_CODE_TILE_SIZE);
        }
        index++;
      }
    }
  },

  /**
   * @private
   * @param {{properties: EuiccProperties}} response
   */
  updateEid_(response) {
    if (!response || !response.properties) {
      return;
    }
    this.eid_ = response.properties.eid;
  },

  /**
   * @private
   * @return {CanvasRenderingContext2D}
   */
  getCanvasContext_() {
    if (this.canvasContext_) {
      return this.canvasContext_;
    }
    return this.$.qrCodeCanvas.getContext('2d');
  },

  /**
   * @param {CanvasRenderingContext2D} canvasContext
   */
  setCanvasContextForTest(canvasContext) {
    this.canvasContext_ = canvasContext;
  },

  /**
   * @param {string} eid
   * @return {string}
   * @private
   */
  getA11yLabel_(eid) {
    return this.i18n('eidPopupA11yLabel', eid);
  },
});
