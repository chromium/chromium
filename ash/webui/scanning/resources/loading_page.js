// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppState} from './scanning_app_types.js';

/**
 * @fileoverview
 * 'loading-page' is shown while searching for available scanners.
 */
Polymer({
  is: 'loading-page',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!AppState} */
    appState: {
      type: Number,
      observer: 'onAppStateChange_',
    },

    /** @protected {boolean} */
    isDarkModeEnabled_: {
      type: Boolean,
    },

    /** @private {boolean} */
    noScannersAvailable_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Determines correct SVG for "no scanners" based on dark mode.
   * @protected
   * @return {string}
   */
  getNoScannersSvgSrc_() {
    return this.isDarkModeEnabled_ ? 'svg/no_scanners_dark.svg' :
                                     'svg/no_scanners.svg';
  },

  /**
   * Determines correct SVG for "scanners loading" based on dark mode.
   * @protected
   * @return {string}
   */
  getScannersLoadingSvgSrc_() {
    return this.isDarkModeEnabled_ ? 'svg/scanners_loading_dark.svg' :
                                     'svg/scanners_loading.svg';
  },

  /** @private */
  onAppStateChange_() {
    this.noScannersAvailable_ = this.appState === AppState.NO_SCANNERS;
  },

  /** @private */
  onRetryClick_() {
    this.fire('retry-click');
  },

  /** @private */
  onLearnMoreClick_() {
    this.fire('learn-more-click');
  },
});
