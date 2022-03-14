// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../strings.m.js';

import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReaderModeApiProxy} from './reader_mode_api_proxy.js';

const ReaderModeElementBase = WebUIListenerMixin(PolymerElement);

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
  private listenerIds_: number[];
  private paragraphs_: string[];

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_ = [callbackRouter.onEssentialContent.addListener(
        (essential_content: string[]) =>
            this.showEssentialContent_(essential_content))];

    this.apiProxy_.showUI();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
  }

  showEssentialContent_(essential_content: string[]) {
    this.paragraphs_ = essential_content;
  }
}
customElements.define(ReaderModeElement.is, ReaderModeElement);
