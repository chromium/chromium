// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview <ntp-img> is specialized <img> that lets you embed external
 * images via its external-src attribute. Usage:
 *
 *   1. In C++ register |SanitizedImageSource| for your WebUI.
 *
 *   2. In HTML instantiate
 *
 *      <img is="ntp-img" external-src="https://foo.com/bar.png"></img>
 *
 * NOTE: Internally, <ntp-img> uses the chrome://image data source. This means
 * the external image will be transcoded to PNG.
 */

/** @type {string} */
const EXTERNAL_SRC = 'external-src';

export class ImgElement extends HTMLImageElement {
  static get observedAttributes() {
    return [EXTERNAL_SRC];
  }

  /** @override */
  attributeChangedCallback(name, oldValue, newValue) {
    if (name === EXTERNAL_SRC) {
      this.src = newValue ? 'chrome://image?' + newValue : '';
    }
  }

  /** @param {string} src */
  set externalSrc(src) {
    this.setAttribute(EXTERNAL_SRC, src);
  }

  /** @return {string} */
  get externalSrc() {
    return this.getAttribute(EXTERNAL_SRC);
  }
}

customElements.define('ntp-img', ImgElement, {extends: 'img'});
