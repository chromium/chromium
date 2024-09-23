// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './multidevice_setup_shared.css.js';
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_button/cr_button.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './button_bar.html.js';

/**
 * DOM Element containing (page-dependent) navigation buttons for the
 * MultiDevice Setup WebUI.
 */
Polymer({
  _template: getTemplate(),
  is: 'button-bar',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** Whether a shadow should appear over the button bar. */
    shouldShowShadow: {
      type: Boolean,
      value: false,
    },

    /**
     * ID of loadTimeData string to be shown on the backward navigation button.
     * @type {string|undefined}
     */
    backwardButtonTextId: {
      type: String,
      value: '',
    },

    /**
     * ID of loadTimeData string to be shown on the cancel button.
     * @type {string|undefined}
     */
    cancelButtonTextId: {
      type: String,
      value: '',
    },

    /**
     * ID of loadTimeData string to be shown on the forward navigation button.
     * @type {string|undefined}
     */
    forwardButtonTextId: {
      type: String,
      value: '',
    },

    /**
     * Whether the forward button should be disabled. I.e., when on the password
     * page and there is an empty password or if the password entered failed and
     * is not yet changed/updated.
     * @type {boolean}
     */
    forwardButtonDisabled: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onForwardButtonClicked_() {
    this.fire('forward-navigation-requested');
  },

  /** @private */
  onCancelButtonClicked_() {
    this.fire('cancel-requested');
  },

  /** @private */
  onBackwardButtonClicked_() {
    this.fire('backward-navigation-requested');
  },

  /**
   * @return {string} The i18n text for any button obtained from loadTimeData
   *     text ID.
   * @private
   */
  getButtonTextFromId_(locale, textId) {
    if (!textId) {
      return '';
    }
    return this.i18nDynamic(locale, textId);
  },
});
