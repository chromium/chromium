// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Loading subpage in Cellular Setup flow that shows an in progress operation or
 * an error. This element contains error image asset and loading animation.
 */
import './base_page.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/cr_elements/cr_lottie/cr_lottie.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './setup_loading_page.html.js';

Polymer({
  _template: getTemplate(),
  is: 'setup-loading-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Message displayed with spinner when in LOADING state.
     */
    loadingMessage: {
      type: String,
      value: '',
    },

    /**
     * Title for page if needed.
     * @type {?string}
     */
    loadingTitle: {
      type: Object,
      value: '',
    },

    /**
     * Displays a sim detect error graphic if true.
     */
    isSimDetectError: {
      type: Boolean,
      value: false,
    },

    /**
     * @type {boolean}
     * @private
     */
    isDarkModeActive_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * @return {string}
   * @private
   */
  getAnimationUrl_() {
    return this.isDarkModeActive_ ?
        'chrome://resources/ash/common/cellular_setup/spinner_dark.json' :
        'chrome://resources/ash/common/cellular_setup/spinner.json';
  },
});
