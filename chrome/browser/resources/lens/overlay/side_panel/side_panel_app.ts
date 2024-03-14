// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './side_panel_app.html.js';
import {SidePanelBrowserProxyImpl} from './side_panel_browser_proxy.js';
import type {SidePanelBrowserProxy} from './side_panel_browser_proxy.js';

export class LensSidePanelAppElement extends PolymerElement {
  static get is() {
    return 'lens-side-panel-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      resultsUrl_: {
        type: String,
      },
    };
  }

  private browserProxy_: SidePanelBrowserProxy =
      SidePanelBrowserProxyImpl.getInstance();
  private listenerIds_: number[];
  private resultsUrl_: string;

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_ = [
      this.browserProxy_.callbackRouter.loadResultsInFrame.addListener(
          this.loadResultsInFrame_.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => assert(this.browserProxy_.callbackRouter.removeListener(id)));
    this.listenerIds_ = [];
  }

  private loadResultsInFrame_(resultsUrl: Url) {
    this.resultsUrl_ = resultsUrl.url;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-side-panel-app': LensSidePanelAppElement;
  }
}

customElements.define(LensSidePanelAppElement.is, LensSidePanelAppElement);
