// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import './color.js';
import './check_mark_wrapper.js';

import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ColorElement} from './color.js';
import {getTemplate} from './colors.html.js';
import {ChromeColor, Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

export interface Color {
  background: SkColor;
  foreground: SkColor;
}

export const LIGHT_DEFAULT_COLOR: Color = {
  background: {value: 0xffffffff},
  foreground: {value: 0xffdee1e6},
};

export const DARK_DEFAULT_COLOR: Color = {
  background: {value: 0xff323639},
  foreground: {value: 0xff202124},
};

export interface ColorsElement {
  $: {
    defaultColor: ColorElement,
    chromeColors: DomRepeat,
  };
}

export class ColorsElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-colors';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      defaultColor_: {
        type: Object,
        computed: 'computeDefaultColor_(theme_)',
      },
      colors_: Array,
      theme_: Object,
    };
  }

  private colors_: ChromeColor[];
  private theme_: Theme;
  private setThemeListenerId_: number|null = null;

  constructor() {
    super();
    CustomizeChromeApiProxy.getInstance().handler.getChromeColors().then(
        ({colors}) => {
          this.colors_ = colors;
        });
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        CustomizeChromeApiProxy.getInstance()
            .callbackRouter.setTheme.addListener((theme: Theme) => {
              this.theme_ = theme;
            });
    CustomizeChromeApiProxy.getInstance().handler.updateTheme();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    CustomizeChromeApiProxy.getInstance().callbackRouter.removeListener(
        this.setThemeListenerId_!);
  }

  private computeDefaultColor_(): Color {
    return this.theme_.systemDarkMode ? DARK_DEFAULT_COLOR :
                                        LIGHT_DEFAULT_COLOR;
  }

  private onDefaultColorClick_() {
    CustomizeChromeApiProxy.getInstance().handler.setDefaultColor();
  }

  private onChromeColorClick_(e: Event) {
    CustomizeChromeApiProxy.getInstance().handler.setForegroundColor(
        this.$.chromeColors.itemForElement(e.target as HTMLElement).foreground);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-colors': ColorsElement;
  }
}

customElements.define(ColorsElement.is, ColorsElement);
