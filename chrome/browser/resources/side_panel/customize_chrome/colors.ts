// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import './color.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './colors.html.js';
import {ChromeColor} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

export class ColorsElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-colors';
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
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-colors': ColorsElement;
  }
}

customElements.define(ColorsElement.is, ColorsElement);
