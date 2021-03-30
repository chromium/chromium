// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DocumentMetadata} from '../constants.js';

export class ViewerPropertiesDialogElement extends PolymerElement {
  static get is() {
    return 'viewer-properties-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!DocumentMetadata} */
      documentMetadata: Object,

      fileName: String,

      pageCount: Number,
    };
  }

  /**
   * @return {!CrDialogElement}
   * @private
   */
  getDialog_() {
    return /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('cr-dialog'));
  }

  /**
   * @param {string} yesLabel
   * @param {string} noLabel
   * @param {boolean} linearized
   * @return {string}
   * @private
   */
  getFastWebViewValue_(yesLabel, noLabel, linearized) {
    return linearized ? yesLabel : noLabel;
  }

  /**
   * @param {string} value
   * @return {string}
   * @private
   */
  getOrPlaceholder_(value) {
    return value || '-';
  }

  /** @private */
  onClickClose_() {
    this.getDialog_().close();
  }
}

customElements.define(
    ViewerPropertiesDialogElement.is, ViewerPropertiesDialogElement);
