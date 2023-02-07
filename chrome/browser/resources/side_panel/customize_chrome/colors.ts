// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import './color.js';
import 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';
import './strings.m.js'; // Required by <managed-dialog>.

import {hexColorToSkColor, skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ColorElement} from './color.js';
import {Color, ColorType, DARK_DEFAULT_COLOR, LIGHT_DEFAULT_COLOR, SelectedColor} from './color_utils.js';
import {getTemplate} from './colors.html.js';
import {ChromeColor, Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

export interface ColorsElement {
  $: {
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
      mainColor_: {
        type: Object,
        computed: 'computeMainColor_(theme_)',
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
      isMainColorSelected_: {
        type: Object,
        computed: 'computeIsMainColorSelected_(selectedColor_)',
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
      showManagedDialog_: Boolean,
    };
  }

  static get observers() {
    return [
      'updateCustomColor_(colors_, theme_, isCustomColorSelected_)',
    ];
  }

  private colors_: ChromeColor[];
  private theme_: Theme;
  private selectedColor_: SelectedColor;
  private isCustomColorSelected_: boolean;
  private customColor_: Color;
  private setThemeListenerId_: number|null = null;
  private showManagedDialog_: boolean;

  constructor() {
    super();
    CustomizeChromeApiProxy.getInstance()
        .handler.getOverviewChromeColors()
        .then(({colors}) => {
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

  private computeMainColor_(): SkColor|undefined {
    return this.theme_ && this.theme_.backgroundImage &&
        this.theme_.backgroundImage.mainColor;
  }

  private computeSelectedColor_(): SelectedColor {
    if (!this.colors_ || !this.theme_) {
      return {type: ColorType.NONE};
    }
    if (!this.theme_.foregroundColor) {
      return {type: ColorType.DEFAULT};
    }
    if (this.theme_.backgroundImage && this.theme_.backgroundImage.mainColor &&
        this.theme_.backgroundImage.mainColor!.value ===
            this.theme_.seedColor.value) {
      return {type: ColorType.MAIN};
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

  private computeIsDefaultColorSelected_(): boolean {
    return this.selectedColor_.type === ColorType.DEFAULT;
  }

  private computeIsMainColorSelected_(): boolean {
    return this.selectedColor_.type === ColorType.MAIN;
  }

  private computeIsCustomColorSelected_(): boolean {
    return this.selectedColor_.type === ColorType.CUSTOM;
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

  private tabIndex_(selected: boolean): string {
    return selected ? '0' : '-1';
  }

  private chromeColorTabIndex_(color: SkColor): string {
    return this.selectedColor_.type === ColorType.CHROME &&
            this.selectedColor_.chromeColor!.value === color.value ?
        '0' :
        '-1';
  }

  private themeHasBackgroundImage_(): boolean {
    return !!this.theme_ && !!this.theme_.backgroundImage;
  }

  private themeHasMainColor_(): boolean {
    return !!this.theme_ && !!this.theme_.backgroundImage &&
        !!this.theme_.backgroundImage.mainColor;
  }

  private onDefaultColorClick_() {
    if (this.handleClickForManagedColors_()) {
      return;
    }
    CustomizeChromeApiProxy.getInstance().handler.setDefaultColor();
  }

  private onMainColorClick_() {
    if (this.handleClickForManagedColors_()) {
      return;
    }
    CustomizeChromeApiProxy.getInstance().handler.setSeedColor(
        this.theme_!.backgroundImage!.mainColor!);
  }

  private onChromeColorClick_(e: Event) {
    if (this.handleClickForManagedColors_()) {
      return;
    }
    CustomizeChromeApiProxy.getInstance().handler.setSeedColor(
        this.$.chromeColors.itemForElement(e.target as HTMLElement).seed);
  }

  private onCustomColorClick_() {
    if (this.handleClickForManagedColors_()) {
      return;
    }
    this.$.colorPicker.focus();
    this.$.colorPicker.click();
  }

  private onCustomColorChange_(e: Event) {
    CustomizeChromeApiProxy.getInstance().handler.setSeedColor(
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

  private onManagedDialogClosed_() {
    this.showManagedDialog_ = false;
  }

  private handleClickForManagedColors_(): boolean {
    if (!this.theme_ || !this.theme_.colorsManagedByPolicy) {
      return false;
    }
    this.showManagedDialog_ = true;
    return true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-colors': ColorsElement;
  }
}

customElements.define(ColorsElement.is, ColorsElement);
