// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './color_mode_select.html.js';
import {ColorMode} from './scanning.mojom-webui.js';
import {alphabeticalCompare, getColorModeString} from './scanning_app_util.js';
import {SelectBehavior, SelectBehaviorInterface} from './select_behavior.js';

/** @type {ColorMode} */
const DEFAULT_COLOR_MODE = ColorMode.kColor;

/**
 * @fileoverview
 * 'color-mode-select' displays the available scanner color modes in a dropdown.
 */

// TODO(b/300484132): Replace mixinBehavior with mixin implementation once a
// Mixin version of SelectBehavior is available.
const ColorModeSelectElementBase =
    mixinBehaviors([I18nBehavior, SelectBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior & SelectBehaviorInterface};

class ColorModeSelectElement extends ColorModeSelectElementBase {
  static get is() {
    return 'color-mode-select' as const;
  }

  static get template() {
    return getTemplate();
  }

  getOptionAtIndex(index: number): string {
    assert(index < this.options.length);

    return this.options[index].toString();
  }

  getColorModeAsString(mojoColorMode: ColorMode): string {
    return getColorModeString(mojoColorMode);
  }

  sortOptions() {
    this.options.sort((a, b) => {
      return alphabeticalCompare(getColorModeString(a), getColorModeString(b));
    });
  }

  isDefaultOption(option: ColorMode): boolean {
    return option === DEFAULT_COLOR_MODE;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ColorModeSelectElement.is]: ColorModeSelectElement;
  }
}

customElements.define(ColorModeSelectElement.is, ColorModeSelectElement);
