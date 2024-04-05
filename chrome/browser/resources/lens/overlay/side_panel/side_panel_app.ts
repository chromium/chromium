// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import '//resources/cr_components/searchbox/realbox.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from '//resources/js/assert.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './side_panel_app.html.js';
import {SidePanelBrowserProxyImpl} from './side_panel_browser_proxy.js';
import type {SidePanelBrowserProxy} from './side_panel_browser_proxy.js';

export interface LensSidePanelAppElement {
  $: {results: HTMLIFrameElement};
}

export class LensSidePanelAppElement extends PolymerElement {
  static get is() {
    return 'lens-side-panel-app';
  }

  static get template() {
    return getTemplate();
  }

  private browserProxy_: SidePanelBrowserProxy =
      SidePanelBrowserProxyImpl.getInstance();
  private listenerIds_: number[];

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

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
    // The src needs to be reset explicitly every time this function is called
    // to force a reload. We cannot get the currently displayed URL from the
    // frame because of cross-origin restrictions.
    this.$.results.src = resultsUrl.url;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-side-panel-app': LensSidePanelAppElement;
  }
}

customElements.define(LensSidePanelAppElement.is, LensSidePanelAppElement);
