// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
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

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {SelectBehaviorInterface}
 */
const ColorModeSelectElementBase =
    mixinBehaviors([I18nBehavior, SelectBehavior], PolymerElement);

/** @polymer */
class ColorModeSelectElement extends ColorModeSelectElementBase {
  static get is() {
    return 'color-mode-select';
  }

  static get template() {
    return getTemplate();
  }

  /**
   * @param {number} index
   * @return {string}
   */
  getOptionAtIndex(index) {
    assert(index < this.options.length);

    return this.options[index].toString();
  }

  /**
   * @param {!ColorMode} mojoColorMode
   * @return {string}
   * @private
   */
  getColorModeString_(mojoColorMode) {
    return getColorModeString(mojoColorMode);
  }

  sortOptions() {
    this.options.sort((a, b) => {
      return alphabeticalCompare(getColorModeString(a), getColorModeString(b));
    });
  }

  /**
   * @param {!ColorMode} option
   * @return {boolean}
   */
  isDefaultOption(option) {
    return option === DEFAULT_COLOR_MODE;
  }
}

customElements.define(ColorModeSelectElement.is, ColorModeSelectElement);
