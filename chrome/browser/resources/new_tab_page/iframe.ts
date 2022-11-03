// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './iframe.html.js';
import {strictQuery} from './utils.js';
import {WindowProxy} from './window_proxy.js';

/**
 * @fileoverview Wrapper around <iframe> element that lets us mock out loading
 * and postMessaging in tests.
 */

export class IframeElement extends PolymerElement {
  static get is() {
    return 'ntp-iframe';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      allow: {
        reflectToAttribute: true,
        type: String,
      },

      src: {
        reflectToAttribute: true,
        type: String,
      },

      src_: {
        type: String,
        computed: 'computeSrc_(src)',
      },
    };
  }

  allow: string;
  src: string;
  private src_: string;

  // Sends message to iframe.
  postMessage(message: any) {
    assert(this.shadowRoot);
    WindowProxy.getInstance().postMessage(
        strictQuery(this.shadowRoot, '#iframe', HTMLIFrameElement), message,
        new URL(this.src).origin);
  }

  private computeSrc_(): string {
    return WindowProxy.getInstance().createIframeSrc(this.src);
  }
}

customElements.define(IframeElement.is, IframeElement);
