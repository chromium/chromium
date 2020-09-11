// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview <ntp-img> is a specialized <img> that facilitates embedding
 * images into WebUIs via its auto-src attribute. <ntp-img> automatically
 * determines if the image is local (e.g. data: or chrome://) or external (e.g.
 * https://), and embeds the image directly or via the chrome://image data
 * source accordingly. Usage:
 *
 *   1. In C++ register |SanitizedImageSource| for your WebUI.
 *
 *   2. In HTML instantiate
 *
 *      <img is="ntp-img" auto-src="https://foo.com/bar.png"></img>
 *
 * NOTE: Since <ntp-img> may use the chrome://image data source some images may
 * be transcoded to PNG.
 */

/** @type {string} */
const AUTO_SRC = 'auto-src';

export class ImgElement extends HTMLImageElement {
  static get observedAttributes() {
    return [AUTO_SRC];
  }

  /** @override */
  attributeChangedCallback(name, oldValue, newValue) {
    if (name !== AUTO_SRC) {
      return;
    }

    let url = null;
    try {
      url = new URL(newValue || '');
    } catch (_) {
    }

    if (!url || url.protocol === 'chrome-untrusted:') {
      // Loading chrome-untrusted:// directly kills the renderer process.
      // Loading chrome-untrusted:// via the chrome://image data source
      // results in a broken image.
      this.removeAttribute('src');
    } else if (url.protocol === 'data:' || url.protocol === 'chrome:') {
      this.src = url.href;
    } else {
      this.src = 'chrome://image?' + url.href;
    }
  }

  /** @param {string} src */
  set autoSrc(src) {
    this.setAttribute(AUTO_SRC, src);
  }

  /** @return {string} */
  get autoSrc() {
    return this.getAttribute(AUTO_SRC);
  }
}

customElements.define('ntp-img', ImgElement, {extends: 'img'});
