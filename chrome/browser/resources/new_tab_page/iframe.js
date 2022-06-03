// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {strictQuery} from './utils.js';
import {WindowProxy} from './window_proxy.js';

/**
 * @fileoverview Wrapper around <iframe> element that lets us mock out loading
 * and postMessaging in tests.
 */

class IframeElement extends PolymerElement {
  static get is() {
    return 'ntp-iframe';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {string} */
      src: {
        reflectToAttribute: true,
        type: String,
      },

      /** @private */
      src_: {
        type: String,
        computed: 'computeSrc_(src)',
      },
    };
  }

  /**
   * Sends message to iframe.
   * @param {*} message
   */
  postMessage(message) {
    assert(this.shadowRoot);
    WindowProxy.getInstance().postMessage(
        strictQuery(this.shadowRoot, '#iframe', HTMLIFrameElement), message,
        new URL(this.src).origin);
  }

  /**
   * @return {string}
   * @private
   */
  computeSrc_() {
    return WindowProxy.getInstance().createIframeSrc(this.src);
  }
}

customElements.define(IframeElement.is, IframeElement);
