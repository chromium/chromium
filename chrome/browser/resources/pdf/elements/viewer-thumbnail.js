// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared-css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// The maximum widths of thumbnails for each layout (px).
// These constants should be kept in sync with `kMaxWidthPortraitPx` and
// `kMaxWidthLandscapePx` in pdf/thumbnail.cc.
/** @type {number} */
const PORTRAIT_WIDTH = 108;
/** @type {number} */
const LANDSCAPE_WIDTH = 140;

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

  /** @param {!ImageData} imageData */
  set image(imageData) {
    const canvas = this.shadowRoot.querySelector('canvas');
    canvas.width = imageData.width;
    canvas.height = imageData.height;

    // Resize the canvas CSS size to maintain the resolution of the thumbnail.
    const isPortrait = canvas.width < canvas.height;
    const cssWidth = Math.min(
        isPortrait ? PORTRAIT_WIDTH : LANDSCAPE_WIDTH,
        parseInt(canvas.width / window.devicePixelRatio, 10));
    const scale = cssWidth / canvas.width;
    const cssHeight = parseInt(canvas.height * scale, 10);
    canvas.style.width = `${cssWidth}px`;
    canvas.style.height = `${cssHeight}px`;

    const ctx = canvas.getContext('2d');
    ctx.putImageData(imageData, 0, 0);
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
