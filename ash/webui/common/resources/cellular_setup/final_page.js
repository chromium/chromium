// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Final page in Cellular Setup flow, which either displays a success or error
 * message depending on the outcome of the flow. This element contains an image
 * asset and description that indicates that the setup flow has completed.
 */
import './base_page.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CellularSetupDelegate} from './cellular_setup_delegate.js';
import {getTemplate} from './final_page.html.js';

Polymer({
  _template: getTemplate(),
  is: 'final-page',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!CellularSetupDelegate} */
    delegate: Object,

    /**
     * Whether error state should be shown.
     * @type {boolean}
     */
    showError: Boolean,

    /** @type {string} */
    message: String,

    /** @type {string} */
    errorMessage: String,
  },

  /**
   * @param {boolean} showError
   * @return {?string}
   * @private
   */
  getTitle_(showError) {
    if (this.delegate.shouldShowPageTitle()) {
      return showError ? this.i18n('finalPageErrorTitle') :
                         this.i18n('finalPageTitle');
    }
    return null;
  },

  /**
   * @param {boolean} showError
   * @return {string}
   * @private
   */
  getMessage_(showError) {
    return showError ? this.errorMessage : this.message;
  },

  /**
   * @param {boolean} showError
   * @return {string}
   * @private
   */
  getPageBodyClass_(showError) {
    return showError ? 'error' : '';
  },

  /**
   * @param {boolean} showError
   * @return {string}
   * @private
   */
  getJellyIllustrationName_(showError) {
    return showError ? 'cellular-setup-illo:error' :
                       'cellular-setup-illo:final-page-success';
  },
});
