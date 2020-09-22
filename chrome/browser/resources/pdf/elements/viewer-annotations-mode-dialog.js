// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ViewerAnnotationsModeDialogElement extends PolymerElement {
  static get is() {
    return 'viewer-annotations-mode-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      rotated: Boolean,
      twoUpViewEnabled: Boolean,
    };
  }

  /** @return {boolean} Whether the dialog is open. */
  isOpen() {
    return this.getDialog_().hasAttribute('open');
  }

  /** @return {boolean} Whether the dialog was confirmed */
  wasConfirmed() {
    return this.getDialog_().getNative().returnValue === 'success';
  }

  /**
   * @return {string}
   * @private
   */
  getBodyMessage_() {
    if (this.rotated && this.twoUpViewEnabled) {
      return loadTimeData.getString('annotationResetRotateAndTwoPageView');
    }
    if (this.rotated) {
      return loadTimeData.getString('annotationResetRotate');
    }
    return loadTimeData.getString('annotationResetTwoPageView');
  }

  /**
   * @return {!CrDialogElement}
   * @private
   */
  getDialog_() {
    return /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#dialog'));
  }

  /** @private */
  onEditClick_() {
    this.getDialog_().close();
  }

  /** @private */
  onCancelClick_() {
    this.getDialog_().cancel();
  }
}

customElements.define(
    ViewerAnnotationsModeDialogElement.is, ViewerAnnotationsModeDialogElement);
