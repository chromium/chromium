// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../strings.m.js';

import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadAnythingApiProxy} from './read_anything_api_proxy.js';

const ReadAnythingElementBase = WebUIListenerMixin(PolymerElement);

export class ReadAnythingElement extends ReadAnythingElementBase {
  static get is() {
    return 'read-anything-app';
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

  private apiProxy_: ReadAnythingApiProxy = ReadAnythingApiProxy.getInstance();
  private listenerIds_: number[];
  private paragraphs_: string[];

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_ = [callbackRouter.onEssentialContent.addListener(
        (essentialContent: string[]) =>
            this.showEssentialContent_(essentialContent))];

    this.apiProxy_.showUI();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
  }

  showEssentialContent_(essentialContent: string[]) {
    this.paragraphs_ = essentialContent;
  }
}
customElements.define(ReadAnythingElement.is, ReadAnythingElement);
