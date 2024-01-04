// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './color_mode_select.html.js';
import {ColorMode} from './scanning.mojom-webui.js';
import {alphabeticalCompare, getColorModeString} from './scanning_app_util.js';
import {AbstractConstructor, SelectMixin, SelectMixinInterface} from './select_mixin.js';

/** @type {ColorMode} */
const DEFAULT_COLOR_MODE = ColorMode.kColor;

/**
 * @fileoverview
 * 'color-mode-select' displays the available scanner color modes in a dropdown.
 */

const ColorModeSelectElementBase = SelectMixin(I18nMixin(PolymerElement)) as
        AbstractConstructor<SelectMixinInterface<ColorMode>>&
    {new (): PolymerElement & I18nMixinInterface};

export class ColorModeSelectElement extends ColorModeSelectElementBase {
  static get is() {
    return 'color-mode-select' as const;
  }

  static get template() {
    return getTemplate();
  }

  override getOptionAtIndex(index: number): string {
    assert(index < this.options.length);

    return this.options[index].toString();
  }

  getColorModeAsString(mojoColorMode: ColorMode): string {
    return getColorModeString(mojoColorMode);
  }

  override sortOptions(): void {
    this.options.sort((a, b) => {
      return alphabeticalCompare(getColorModeString(a), getColorModeString(b));
    });
  }

  override isDefaultOption(option: ColorMode): boolean {
    return option === DEFAULT_COLOR_MODE;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ColorModeSelectElement.is]: ColorModeSelectElement;
  }
}

customElements.define(ColorModeSelectElement.is, ColorModeSelectElement);
