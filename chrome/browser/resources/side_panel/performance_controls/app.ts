// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//performance-side-panel.top-chrome/shared/sp_shared_style.css.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {PerformanceApiProxy, PerformanceApiProxyImpl} from './performance_api_proxy.js';

export interface PerformanceAppElement {
  $: {};
}

export class PerformanceAppElement extends PolymerElement {
  static get is() {
    return 'performance-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private performanceApi_: PerformanceApiProxy =
      PerformanceApiProxyImpl.getInstance();

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();

    // Inform the handler that listeners are registered.
    setTimeout(() => this.performanceApi_.showUi(), 0);
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'performance-app': PerformanceAppElement;
  }
}
customElements.define(PerformanceAppElement.is, PerformanceAppElement);
