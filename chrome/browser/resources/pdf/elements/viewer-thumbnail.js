// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared-css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ViewerThumbnailElement extends PolymerElement {
  static get is() {
    return 'viewer-thumbnail';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      isActive: {
        type: Boolean,
        observer: 'isActiveChanged_',
        reflectToAttribute: true,
      },

      pageNumber: Number,
    };
  }

  /** @private */
  isActiveChanged_() {
    if (this.isActive) {
      this.scrollIntoView({block: 'nearest'});
    }
  }

  /** @private */
  onClick_() {
    this.dispatchEvent(new CustomEvent('change-page', {
      detail: {page: this.pageNumber - 1, origin: 'thumbnail'},
      bubbles: true,
      composed: true
    }));
  }
}

customElements.define(ViewerThumbnailElement.is, ViewerThumbnailElement);
