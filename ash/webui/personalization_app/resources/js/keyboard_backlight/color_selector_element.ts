// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * The color selector provides the selection of wallpaper or preset colors in
 * keyboard backlight and zone customization section.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import './color_icon_element.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {BacklightColor, CurrentBacklightState} from '../../personalization_app.mojom-webui.js';
import {isMultiZoneRgbKeyboardSupported} from '../load_time_booleans.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {ColorInfo, getPresetColors, isSelectionEvent, RAINBOW, WALLPAPER, WHITE} from '../utils.js';

import {getTemplate} from './color_selector_element.html.js';
import {getShouldShowNudge, handleNudgeShown} from './keyboard_backlight_controller.js';
import {getKeyboardBacklightProvider} from './keyboard_backlight_interface_provider.js';

export type PresetColorSelectedEvent = CustomEvent<{colorId: string}>;
export type RainbowColorSelectedEvent = CustomEvent<null>;
export type WallpaperColorSelectedEvent = CustomEvent<null>;

const presetColorSelectedEventName = 'preset-color-selected';
const rainbowColorSelectedEventName = 'rainbow-color-selected';
const wallpaperColorSelectedEventName = 'wallpaper-color-selected';

declare global {
  interface HTMLElementEventMap {
    [presetColorSelectedEventName]: PresetColorSelectedEvent;
    [rainbowColorSelectedEventName]: RainbowColorSelectedEvent;
    [wallpaperColorSelectedEventName]: WallpaperColorSelectedEvent;
  }
}

export interface ColorSelectorElement {
  $: {
    keys: IronA11yKeysElement,
    selector: IronSelectorElement,
  };
}

export class ColorSelectorElement extends WithPersonalizationStore {
  static get is() {
    return 'color-selector';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isCustomizedDialog: {
        type: Boolean,
        value: false,
      },

      ironSelectedColor_: Object,

      isMultiZoneRgbKeyboardSupported_: {
        type: Boolean,
        value() {
          return isMultiZoneRgbKeyboardSupported();
        },
      },

      presetColors_: {
        type: Object,
        value() {
          return getPresetColors();
        },
      },

      presetColorIds_: {
        type: Array,
        computed: 'computePresetColorIds_(presetColors_)',
      },

      rainbowColorId_: {
        type: String,
        value: RAINBOW,
      },

      wallpaperColorId_: {
        type: String,
        value: WALLPAPER,
      },

      currentBacklightState_: Object,

      selectedColor: BacklightColor,

      /** The current wallpaper extracted color. */
      wallpaperColor_: Object,

      shouldShowNudge_: {
        type: Boolean,
        value: false,
        observer: 'onShouldShowNudgeChanged_',
      },
    };
  }

  private isCustomizedDialog: string;
  private ironSelectedColor_: HTMLElement;
  private presetColors_: Record<string, ColorInfo>;
  private presetColorIds_: string[];
  private rainbowColorId_: string;
  private wallpaperColorId_: string;
  private currentBacklightState_: CurrentBacklightState|null;
  private selectedColor: BacklightColor;
  private wallpaperColor_: SkColor|null;
  private shouldShowNudge_: boolean;

  override ready() {
    super.ready();
    this.$.keys.target = this.$.selector;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<ColorSelectorElement['currentBacklightState_']>(
        'currentBacklightState_',
        state => state.keyboardBacklight.currentBacklightState);
    this.watch<ColorSelectorElement['shouldShowNudge_']>(
        'shouldShowNudge_', state => state.keyboardBacklight.shouldShowNudge);
    this.watch<ColorSelectorElement['wallpaperColor_']>(
        'wallpaperColor_', state => state.keyboardBacklight.wallpaperColor);
    this.updateFromStore();

    getShouldShowNudge(getKeyboardBacklightProvider(), this.getStore());
  }

  /** Handle keyboard navigation. */
  private onKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    const selector = this.$.selector;
    const prevButton = this.ironSelectedColor_;
    switch (e.detail.key) {
      case 'left':
        selector.selectPrevious();
        break;
      case 'right':
        selector.selectNext();
        break;
      case 'enter':
        switch (this.ironSelectedColor_.id) {
          case this.rainbowColorId_:
            this.dispatchEvent(new CustomEvent(
                rainbowColorSelectedEventName,
                {bubbles: true, composed: true, detail: null}));
            break;
          case this.wallpaperColorId_:
            this.dispatchEvent(new CustomEvent(
                wallpaperColorSelectedEventName,
                {bubbles: true, composed: true, detail: null}));
            break;
          default:
            this.dispatchEvent(new CustomEvent(presetColorSelectedEventName, {
              bubbles: true,
              composed: true,
              detail: {colorId: this.ironSelectedColor_.id},
            }));
            break;
        }
        break;
      default:
        return;
    }
    // Remove focus state of color icon in previous button.
    if (prevButton) {
      const colorIconElem = this.getColorIconElement_(prevButton);
      if (colorIconElem) {
        colorIconElem.removeAttribute('tabindex');
      }
    }
    // Add focus state for the color icon in new button.
    if (this.ironSelectedColor_) {
      const colorIconElem = this.getColorIconElement_(this.ironSelectedColor_);
      if (colorIconElem) {
        colorIconElem.setAttribute('tabindex', '0');
        colorIconElem.focus();
      }
    }
    e.detail.keyboardEvent.preventDefault();
  }

  private shouldShowRainbowColorItem_(): boolean {
    return !this.isCustomizedDialog;
  }

  private computePresetColorIds_(presetColors: Record<string, string>):
      string[] {
    // ES2020 maintains ordering of Object.keys.
    return Object.keys(presetColors);
  }

  /** Invoked when the wallpaper color is selected. */
  private onWallpaperColorSelected_(e: Event) {
    if (!isSelectionEvent(e)) {
      return;
    }
    const eventTarget = e.target as HTMLElement;
    if (eventTarget.id === 'wallpaperColorIcon') {
      // Only dispatch the event if the icon is clicked.
      this.dispatchEvent(new CustomEvent(
          wallpaperColorSelectedEventName,
          {bubbles: true, composed: true, detail: null}));
    }
  }

  /** Invoked when a preset color is selected. */
  private onPresetColorSelected_(e: Event) {
    if (!isSelectionEvent(e)) {
      return;
    }
    const htmlElement = e.currentTarget as HTMLElement;
    const colorId = htmlElement.id;
    assert(colorId !== undefined, 'colorId not found');
    this.dispatchEvent(new CustomEvent(
        presetColorSelectedEventName,
        {bubbles: true, composed: true, detail: {colorId: colorId}}));
  }

  /** Invoked when the rainbow color is selected. */
  private onRainbowColorSelected_(e: Event) {
    if (!isSelectionEvent(e)) {
      return;
    }
    this.dispatchEvent(new CustomEvent(
        rainbowColorSelectedEventName,
        {bubbles: true, composed: true, detail: null}));
  }

  private onShouldShowNudgeChanged_(shouldShowNudge: boolean) {
    if (shouldShowNudge) {
      setTimeout(() => {
        handleNudgeShown(getKeyboardBacklightProvider(), this.getStore());
      }, 3000);
    }
  }

  private getColorIconElement_(button: HTMLElement): HTMLElement {
    return this.shadowRoot!.getElementById(button.id)!.querySelector(
        'color-icon')!;
  }

  private getColorSelectorAriaLabel_(): string {
    return loadTimeData.getString('keyboardBacklightTitle');
  }

  private getPresetColorTabIndex_(
      isMultiZoneRgbKeyboardSupported: boolean, presetColorId: string): string {
    return isMultiZoneRgbKeyboardSupported && presetColorId === WHITE ? '0' :
                                                                        '-1';
  }

  private getPresetColorAriaLabel_(presetColorId: string): string {
    return this.i18n(presetColorId);
  }

  private getWallpaperColorContainerClass_(selectedColor: BacklightColor):
      string {
    return this.getColorContainerClass_(
        this.getWallpaperColorAriaChecked_(selectedColor));
  }

  private getPresetColorContainerClass_(
      colorId: string, colors: Record<string, ColorInfo>,
      selectedColor: BacklightColor) {
    return this.getColorContainerClass_(
        this.getPresetColorAriaChecked_(colorId, colors, selectedColor));
  }

  private getRainbowColorContainerClass_(selectedColor: BacklightColor) {
    return this.getColorContainerClass_(
        this.getRainbowColorAriaChecked_(selectedColor));
  }

  private getColorContainerClass_(isSelected: string) {
    const defaultClassName = 'selectable';
    return isSelected === 'true' ? `${defaultClassName} tast-selected-color` :
                                   defaultClassName;
  }

  private getWallpaperColorAriaChecked_(selectedColor: BacklightColor) {
    return (selectedColor === BacklightColor.kWallpaper).toString();
  }

  private getPresetColorAriaChecked_(
      colorId: string, colors: Record<string, ColorInfo>,
      selectedColor: BacklightColor) {
    if (!colorId || !colors[colorId]) {
      return 'false';
    }
    return (colors[colorId].enumVal === selectedColor).toString();
  }

  private getRainbowColorAriaChecked_(selectedColor: BacklightColor) {
    return (selectedColor === BacklightColor.kRainbow).toString();
  }

  private getWallpaperColorTitle_() {
    return this.i18n('wallpaperColorTooltipText');
  }
}

customElements.define(ColorSelectorElement.is, ColorSelectorElement);
