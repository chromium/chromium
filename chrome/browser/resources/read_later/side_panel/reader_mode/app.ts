// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReaderModeApiProxy} from './reader_mode_api_proxy.js';

const ReaderModeElementBase =
    mixinBehaviors([WebUIListenerBehavior], PolymerElement) as
    {new (): PolymerElement & WebUIListenerBehavior};

export class ReaderModeElement extends ReaderModeElementBase {
  static get is() {
    return 'reader-mode-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      paragraphs_: {
        type: Array,
        value: () => [],
      },
    };
  }

  private apiProxy_: ReaderModeApiProxy = ReaderModeApiProxy.getInstance();
  private paragraphs_: string[];

  connectedCallback() {
    super.connectedCallback();
    this.showReaderMode_();
  }

  async showReaderMode_() {
    const {result} = await this.apiProxy_.showReaderMode();
    this.paragraphs_ = result;
  }
}
customElements.define(ReaderModeElement.is, ReaderModeElement);
