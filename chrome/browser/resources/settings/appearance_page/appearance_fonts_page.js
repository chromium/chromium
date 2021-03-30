// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_slider.js';
import '../settings_shared_css.js';

import {SliderTick} from 'chrome://resources/cr_elements/cr_slider/cr_slider.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import {loadTimeData} from '../i18n_setup.js';

import {FontsBrowserProxy, FontsBrowserProxyImpl, FontsData} from './fonts_browser_proxy.js';


/** @type {!Array<number>} */
const FONT_SIZE_RANGE = [
  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
  26, 28, 30, 32, 34, 36, 40, 44, 48, 56, 64, 72,
];

/** @type {!Array<number>} */
const MINIMUM_FONT_SIZE_RANGE =
    [0, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24];

/**
 * @param {!Array<number>} ticks
 * @return {!Array<!SliderTick>}
 */
function ticksWithLabels(ticks) {
  return ticks.map(x => ({label: `${x}`, value: x}));
}

/**
 * 'settings-appearance-fonts-page' is the settings page containing appearance
 * settings.
 */
Polymer({
  is: 'settings-appearance-fonts-page',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /** @private {!DropdownMenuOptionList} */
    fontOptions_: Object,

    /**
     * Common font sizes.
     * @private {!Array<!SliderTick>}
     */
    fontSizeRange_: {
      readOnly: true,
      type: Array,
      value: ticksWithLabels(FONT_SIZE_RANGE),
    },

    /**
     * Reasonable, minimum font sizes.
     * @private {!Array<!SliderTick>}
     */
    minimumFontSizeRange_: {
      readOnly: true,
      type: Array,
      value: ticksWithLabels(MINIMUM_FONT_SIZE_RANGE),
    },

    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  observers: [
    'onMinimumSizeChange_(prefs.webkit.webprefs.minimum_font_size.value)',
  ],

  /** @private {?FontsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = FontsBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.browserProxy_.fetchFontsData().then(this.setFontsData_.bind(this));
  },

  /**
   * @param {!FontsData} response A list of fonts.
   * @private
   */
  setFontsData_(response) {
    const fontMenuOptions = [];
    for (const fontData of response.fontList) {
      fontMenuOptions.push({value: fontData[0], name: fontData[1]});
    }
    this.fontOptions_ = fontMenuOptions;
  },

  /**
   * Get the minimum font size, accounting for unset prefs.
   * @return {number}
   * @private
   */
  computeMinimumFontSize_() {
    const prefValue = this.get('prefs.webkit.webprefs.minimum_font_size.value');
    return /** @type {number} */ (prefValue) || MINIMUM_FONT_SIZE_RANGE[0];
  },


  /** @private */
  onMinimumSizeChange_() {
    this.$.minimumSizeFontPreview.hidden = this.computeMinimumFontSize_() <= 0;
  },
});
