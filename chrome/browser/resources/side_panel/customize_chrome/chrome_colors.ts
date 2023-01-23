// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './color.js';

import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './chrome_colors.html.js';
import {Color, DARK_DEFAULT_COLOR, LIGHT_DEFAULT_COLOR} from './color_utils.js';
import {ChromeColor, Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

export interface ChromeColorsElement {
  $: {
    backButton: HTMLElement,
    colorPicker: HTMLInputElement,
    colorPickerIcon: HTMLElement,
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
      defaultColor_: {
        type: Object,
        computed: 'computeDefaultColor_(theme_)',
      },
      colors_: Array,
      theme_: Object,
      customColor_: {
        type: Object,
        value: {
          background: {value: 0xffffffff},
          foreground: {value: 0xfff1f3f4},
        },
      },
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
    FocusOutlineManager.forDocument(document);
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
