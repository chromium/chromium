// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './color.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './chrome_colors.html.js';
import {ChromeColor} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

export interface ChromeColorsElement {
  $: {
    backButton: HTMLElement,
  };
}

export class ChromeColorsElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-chrome-colors';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      colors_: Array,
    };
  }

  private colors_: ChromeColor[];

  constructor() {
    super();
    CustomizeChromeApiProxy.getInstance().handler.getChromeColors().then(
        ({colors}) => {
          this.colors_ = colors;
        });
  }

  private onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-chrome-colors': ChromeColorsElement;
  }
}

customElements.define(ChromeColorsElement.is, ChromeColorsElement);
