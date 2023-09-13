// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppState} from './scanning_app_types.js';

/**
 * @fileoverview
 * 'loading-page' is shown while searching for available scanners.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const LoadingPageElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class LoadingPageElement extends LoadingPageElementBase {
  static get is() {
    return 'loading-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!AppState} */
      appState: {
        type: Number,
        observer: 'onAppStateChange_',
      },

      /** @protected {boolean} */
      isDarkModeEnabled_: {
        type: Boolean,
      },

      /** @protected {boolean} */
      isJellyEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('isJellyEnabledForScanningApp');
        },
      },

      /** @private {boolean} */
      noScannersAvailable_: {
        type: Boolean,
        value: false,
      },
    };
  }



  /**
   * Determines correct SVG for "no scanners" based on dark mode.
   * @protected
   * @return {string}
   */
  getNoScannersSvgSrc_() {
    return this.isDarkModeEnabled_ ? 'svg/no_scanners_dark.svg' :
                                     'svg/no_scanners.svg';
  }

  /**
   * Determines correct SVG for "scanners loading" based on dark mode.
   * @protected
   * @return {string}
   */
  getScannersLoadingSvgSrc_() {
    return this.isDarkModeEnabled_ ? 'svg/scanners_loading_dark.svg' :
                                     'svg/scanners_loading.svg';
  }

  /** @private */
  onAppStateChange_() {
    this.noScannersAvailable_ = this.appState === AppState.NO_SCANNERS;
  }

  /** @private */
  onRetryClick_() {
    this.dispatchEvent(
        new CustomEvent('retry-click', {bubbles: true, composed: true}));
  }

  /** @private */
  onLearnMoreClick_() {
    this.dispatchEvent(
        new CustomEvent('learn-more-click', {bubbles: true, composed: true}));
  }

  /** @param {boolean} enabled */
  setIsJellyEnabledForTesting(enabled) {
    this.isJellyEnabled_ = enabled;
  }
}

customElements.define(LoadingPageElement.is, LoadingPageElement);
