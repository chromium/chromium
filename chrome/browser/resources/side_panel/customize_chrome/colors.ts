// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import './color.js';

import {hexColorToSkColor, skColorToRgba} from 'chrome://resources/js/color_utils.js';
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

function isChromeColorSelected(
    theme: Theme|undefined, colors: ChromeColor[]|undefined, color: SkColor) {
  return !!theme && !!colors && !!theme.foregroundColor &&
      theme.foregroundColor.value === color.value;
}

export interface ColorsElement {
  $: {
    defaultColor: ColorElement,
    chromeColors: DomRepeat,
    customColorContainer: HTMLElement,
    customColor: ColorElement,
    colorPicker: HTMLInputElement,
    colorPickerIcon: HTMLElement,
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
      isDefaultColorSelected_: {
        type: Object,
        computed: 'computeIsDefaultColorSelected_(theme_)',
      },
      isCustomColorSelected_: {
        type: Object,
        computed: 'computeIsCustomColorSelected_(theme_, color_)',
      },
      customColor_: {
        type: Object,
        value: {
          background: {value: 0xffffffff},
          foreground: {value: 0xfff1f3f4},
        },
      },
    };
  }

  static get observers() {
    return [
      'updateCustomColor_(colors_, theme_, isCustomColorSelected_)',
    ];
  }

  private colors_: ChromeColor[];
  private theme_: Theme;
  private isCustomColorSelected_: boolean;
  private customColor_: Color;
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

  private computeIsDefaultColorSelected_(): boolean {
    return this.theme_ && !this.theme_.foregroundColor;
  }

  private computeIsCustomColorSelected_(): boolean {
    return !!this.colors_ && !!this.theme_ && !!this.theme_.foregroundColor &&
        !this.colors_.find(
            (color: ChromeColor) =>
                color.foreground.value === this.theme_.foregroundColor!.value);
  }

  private isChromeColorSelected_(color: SkColor): boolean {
    return isChromeColorSelected(this.theme_, this.colors_, color);
  }

  private tabIndex_(selected: boolean): string {
    return selected ? '0' : '-1';
  }

  private chromeColorTabIndex_(color: SkColor): string {
    return isChromeColorSelected(this.theme_, this.colors_, color) ? '0' : '-1';
  }

  private themeHasBackgroundImage_(): boolean {
    return !!this.theme_ && !!this.theme_.backgroundImage;
  }

  private onDefaultColorClick_() {
    CustomizeChromeApiProxy.getInstance().handler.setDefaultColor();
  }

  private onChromeColorClick_(e: Event) {
    CustomizeChromeApiProxy.getInstance().handler.setForegroundColor(
        this.$.chromeColors.itemForElement(e.target as HTMLElement).foreground);
  }

  private onCustomColorClick_() {
    this.$.colorPicker.focus();
    this.$.colorPicker.click();
  }

  private onCustomColorChange_(e: Event) {
    CustomizeChromeApiProxy.getInstance().handler.setForegroundColor(
        hexColorToSkColor((e.target as HTMLInputElement).value));
  }

  private updateCustomColor_() {
    // We only change the custom color when theme updates to a new custom color
    // so that the picked color persists while clicking on other color circles.
    if (!this.isCustomColorSelected_) {
      return;
    }
    this.customColor_ = {
      background: this.theme_.backgroundColor,
      foreground: this.theme_.foregroundColor!,
    };
    this.$.colorPickerIcon.style.setProperty(
        'background-color', skColorToRgba(this.theme_.colorPickerIconColor));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-colors': ColorsElement;
  }
}

customElements.define(ColorsElement.is, ColorsElement);
