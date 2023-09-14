// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './scanning_fonts_css.js';
import './strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './multi_page_scan.html.js';
import {AppState} from './scanning_app_types.js';
import {ScanningBrowserProxyImpl, SelectedPath} from './scanning_browser_proxy.js';

/**
 * @fileoverview
 * 'multi-page-scan' shows the available actions for a multi-page scan.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const MultiPageScanElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class MultiPageScanElement extends MultiPageScanElementBase {
  static get is() {
    return 'multi-page-scan';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {!AppState} */
      appState: {
        type: Number,
        observer: 'onAppStateChange_',
      },

      /** @type {number} */
      pageNumber: {
        type: Number,
        observer: 'onPageNumberChange_',
      },

      /** @private {string} */
      scanButtonText_: String,

      /** @private {boolean} */
      showCancelButton_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      cancelButtonDisabled_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      showCancelingText_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @private */
  onAppStateChange_() {
    this.showCancelButton_ = this.appState === AppState.MULTI_PAGE_SCANNING ||
        this.appState === AppState.MULTI_PAGE_CANCELING;
    this.cancelButtonDisabled_ =
        this.appState === AppState.MULTI_PAGE_CANCELING;
    this.showCancelingText_ = this.appState === AppState.MULTI_PAGE_CANCELING;
  }

  /** @private */
  onPageNumberChange_() {
    ScanningBrowserProxyImpl.getInstance()
        .getPluralString('scanButtonText', this.pageNumber + 1)
        .then(
            /* @type {string} */ (pluralString) => {
              this.scanButtonText_ = pluralString;
            });
  }

  /** @private */
  onScanClick_() {
    this.dispatchEvent(
        new CustomEvent('scan-next-page', {bubbles: true, composed: true}));
  }

  /** @private */
  onSaveClick_() {
    this.dispatchEvent(new CustomEvent(
        'complete-multi-page-scan', {bubbles: true, composed: true}));
  }

  /** @private */
  onCancelClick_() {
    this.dispatchEvent(
        new CustomEvent('cancel-click', {bubbles: true, composed: true}));
  }

  /**
   * @return {string}
   * @private
   */
  getProgressText_() {
    return this.i18n('multiPageScanProgressText', this.pageNumber);
  }
}

customElements.define(MultiPageScanElement.is, MultiPageScanElement);
