// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * The color icon indicates wallpaper or preset colors in keyboard backlight and
 * zone customization section.
 */

import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {WithPersonalizationStore} from '../personalization_store.js';
import {convertToRgbHexStr, getPresetColors, GREEN, INDIGO, RAINBOW, RED, WALLPAPER, YELLOW} from '../utils.js';

import {getTemplate} from './color_icon_element.html.js';

/**
  Based on this algorithm suggested by the W3:
  https://www.w3.org/TR/AERT/#color-contrast
*/
function calculateColorBrightness(hexVal: number): number {
  const r = (hexVal >> 16) & 0xff;  // extract red
  const g = (hexVal >> 8) & 0xff;   // extract green
  const b = (hexVal >> 0) & 0xff;   // extract blue
  return (r * 299 + g * 587 + b * 114) / 1000;
}

export class ColorIconElement extends WithPersonalizationStore {
  static get is() {
    return 'color-icon';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The color id indicates the color of the icon. */
      colorId: {
        type: String,
        value: null,
        reflectToAttribute: true,
      },

      /** The current wallpaper extracted color. */
      wallpaperColor_: Object,
    };
  }

  colorId: string|null;
  private wallpaperColor_: SkColor|null;

  override connectedCallback() {
    super.connectedCallback();
    this.watch(
        'wallpaperColor_', state => state.keyboardBacklight.wallpaperColor);
    this.updateFromStore();
  }

  private isWallpaperColorId_(colorId: string|null): boolean {
    return colorId === WALLPAPER;
  }

  private getColorInnerContainerStyle_(colorId: string|null): string {
    if (!colorId) {
      return '';
    }
    const colors = getPresetColors();
    const outlineStyle = `outline: 2px solid var(--cros-separator-color);
                    outline-offset: -2px;`;
    switch (colorId) {
      case RAINBOW:
        return `background-image: linear-gradient(90deg,
            ${colors[RED].hexVal},
            ${colors[YELLOW].hexVal},
            ${colors[GREEN].hexVal},
            ${colors[INDIGO].hexVal});
            ${outlineStyle}`;
      default:
        return `background-color: ${colors[colorId].hexVal};
                                    ${outlineStyle};`;
    }
  }

  private getWallpaperColorInnerContainerStyle_(wallpaperColor: SkColor|
                                                null): string {
    const hexStr = !wallpaperColor ?
        '#FFFFFF' :
        convertToRgbHexStr(wallpaperColor.value & 0xFFFFFF);
    return `background-color: ${hexStr};
        outline: 2px solid var(--cros-separator-color);
        outline-offset: -2px;`;
  }

  private getWallpaperIconColorClass_(wallpaperColor: SkColor): string {
    if (!wallpaperColor || (wallpaperColor.value & 0xFFFFFF) === 0xFFFFFF) {
      return `light-icon`;
    }
    const brightness =
        calculateColorBrightness(wallpaperColor.value & 0xFFFFFF);
    if (brightness < 125) {
      return `dark-icon`;
    }
    return `light-icon`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'color-icon': ColorIconElement;
  }
}

customElements.define(ColorIconElement.is, ColorIconElement);
