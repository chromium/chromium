// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview UI element of a download item.
 */

import {CustomElement} from 'chrome://resources/js/custom_element.js';

export class DownloadItemElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {Object} */
    this.item_;
  }

  set item(value) {
    this.item_ = value;
  }

  get item() {
    return this.item_;
  }

  connectedCallback() {
    this.$('#filename').innerText = this.item_.filename;
  }
}

customElements.define('download-item', DownloadItemElement);
