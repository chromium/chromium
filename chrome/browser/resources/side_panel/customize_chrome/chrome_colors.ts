// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './color.js';

import {SpHeading} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {hexColorToSkColor, skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './chrome_colors.html.js';
import {ColorElement} from './color.js';
import {Color, ColorType, DARK_DEFAULT_COLOR, LIGHT_DEFAULT_COLOR, SelectedColor} from './color_utils.js';
import {ChromeColor, CustomizeChromePageHandlerInterface, Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

export interface ChromeColorsElement {
  $: {
    colorPicker: HTMLInputElement,
    colorPickerIcon: HTMLElement,
    defaultColor: ColorElement,
    customColor: ColorElement,
    customColorContainer: HTMLElement,
    heading: SpHeading,
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
      selectedColor_: {
        type: Object,
        computed: 'computeSelectedColor_(theme_, colors_)',
      },
      isDefaultColorSelected_: {
        type: Object,
        computed: 'computeIsDefaultColorSelected_(selectedColor_)',
      },
      isCustomColorSelected_: {
        type: Object,
        computed: 'computeIsCustomColorSelected_(selectedColor_)',
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
  private setThemeListenerId_: number|null = null;
  private isCustomColorSelected_: boolean;
  private customColor_: Color;
  private selectedColor_: SelectedColor;

  private pageHandler_: CustomizeChromePageHandlerInterface;

  constructor() {
    super();
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.pageHandler_.getChromeColors().then(({colors}) => {
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
    this.pageHandler_.updateTheme();
    FocusOutlineManager.forDocument(document);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    CustomizeChromeApiProxy.getInstance().callbackRouter.removeListener(
        this.setThemeListenerId_!);
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  private computeIsDefaultColorSelected_(): boolean {
    return this.selectedColor_.type === ColorType.DEFAULT;
  }

  private computeIsCustomColorSelected_(): boolean {
    return this.selectedColor_.type === ColorType.CUSTOM;
  }

  private computeSelectedColor_(): SelectedColor {
    // None will be considered selected if it isn't classic chrome.
    if (!this.colors_ || !this.theme_ || this.theme_.backgroundImage ||
        this.theme_.thirdPartyThemeInfo) {
      return {type: ColorType.NONE};
    }
    if (!this.theme_.foregroundColor) {
      return {type: ColorType.DEFAULT};
    }
    if (this.colors_.find(
            (color: ChromeColor) =>
                color.seed.value === this.theme_.seedColor.value)) {
      return {
        type: ColorType.CHROME,
        chromeColor: this.theme_.seedColor,
      };
    }
    return {type: ColorType.CUSTOM};
  }

  private computeDefaultColor_(): Color {
    return this.theme_.systemDarkMode ? DARK_DEFAULT_COLOR :
                                        LIGHT_DEFAULT_COLOR;
  }

  private isChromeColorSelected_(color: SkColor): boolean {
    return this.selectedColor_.type === ColorType.CHROME &&
        this.selectedColor_.chromeColor!.value === color.value;
  }

  private boolToString_(value: boolean): string {
    return value ? 'true' : 'false';
  }

  private getChromeColorCheckedStatus_(color: SkColor): string {
    return this.boolToString_(this.isChromeColorSelected_(color));
  }

  private onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  private onDefaultColorClick_() {
    this.pageHandler_.setDefaultColor();
    this.pageHandler_.removeBackgroundImage();
  }

  private onChromeColorClick_(e: DomRepeatEvent<ChromeColor>) {
    this.pageHandler_.setSeedColor(e.model.item.seed);
    this.pageHandler_.removeBackgroundImage();
  }

  private onCustomColorClick_() {
    this.$.colorPicker.focus();
    this.$.colorPicker.click();
  }

  private onCustomColorChange_(e: Event) {
    this.pageHandler_.setSeedColor(
        hexColorToSkColor((e.target as HTMLInputElement).value));
    this.pageHandler_.removeBackgroundImage();
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
    'customize-chrome-chrome-colors': ChromeColorsElement;
  }
}

customElements.define(ChromeColorsElement.is, ChromeColorsElement);
