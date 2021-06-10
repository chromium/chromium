// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import './os_feedback_shared_css.js';

/**
 * @fileoverview
 * 'confirmation-page' is the last step of the feedback tool.
 */
export class ConfirmationPageElement extends PolymerElement {
  static get is() {
    return 'confirmation-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  ready() {
    super.ready();
    /**TODO(xiangdongkong): remove  */
    this.$.header.textContent = 'Thank you for your feedback';
  }

  close_() {
    window.close();
  }
};

customElements.define(ConfirmationPageElement.is, ConfirmationPageElement);
