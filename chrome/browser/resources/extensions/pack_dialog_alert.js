// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class ExtensionsPackDialogAlertElement extends PolymerElement {
  static get is() {
    return 'extensions-pack-dialog-alert';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {chrome.developerPrivate.PackDirectoryResponse} */
      model: Object,

      /** @private */
      title_: String,

      /** @private */
      message_: String,

      /** @private {?string} */
      cancelLabel_: String,

      /**
       * This needs to be initialized to trigger data-binding.
       * @private {?string}
       */
      confirmLabel_: {
        type: String,
        value: '',
      }
    };
  }

  /** @return {string} */
  get returnValue() {
    return /** @type {!CrDialogElement} */ (this.$.dialog)
        .getNative()
        .returnValue;
  }

  /** @override */
  ready() {
    super.ready();

    // Initialize button label values for initial html binding.
    this.cancelLabel_ = null;
    this.confirmLabel_ = null;

    switch (this.model.status) {
      case chrome.developerPrivate.PackStatus.WARNING:
        this.title_ = loadTimeData.getString('packDialogWarningTitle');
        this.cancelLabel_ = loadTimeData.getString('cancel');
        this.confirmLabel_ = loadTimeData.getString('packDialogProceedAnyway');
        break;
      case chrome.developerPrivate.PackStatus.ERROR:
        this.title_ = loadTimeData.getString('packDialogErrorTitle');
        this.cancelLabel_ = loadTimeData.getString('ok');
        break;
      case chrome.developerPrivate.PackStatus.SUCCESS:
        this.title_ = loadTimeData.getString('packDialogTitle');
        this.cancelLabel_ = loadTimeData.getString('ok');
        break;
      default:
        assertNotReached();
        return;
    }
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  /**
   * @return {string}
   * @private
   */
  getCancelButtonClass_() {
    return this.confirmLabel_ ? 'cancel-button' : 'action-button';
  }

  /** @private */
  onCancelTap_() {
    this.$.dialog.cancel();
  }

  /** @private */
  onConfirmTap_() {
    // The confirm button should only be available in WARNING state.
    assert(this.model.status === chrome.developerPrivate.PackStatus.WARNING);
    this.$.dialog.close();
  }
}

customElements.define(
    ExtensionsPackDialogAlertElement.is, ExtensionsPackDialogAlertElement);
