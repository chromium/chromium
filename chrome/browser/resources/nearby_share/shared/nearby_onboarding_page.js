// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/cr_icons_css.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './nearby_page_template.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NearbyShareOnboardingFinalState, processOnboardingCancelledMetrics, processOnboardingInitiatedMetrics} from './nearby_metrics_logger.js';
import {getNearbyShareSettings} from './nearby_share_settings.js';
import {NearbySettings} from './nearby_share_settings_behavior.js';

/**
 * @fileoverview The 'nearby-onboarding-page' component handles the Nearby Share
 * onboarding flow. It is embedded in chrome://os-settings, chrome://settings
 * and as a standalone dialog via chrome://nearby.
 */

/**
 * @type {string}
 */
const ONBOARDING_SPLASH_LIGHT_ICON =
    'nearby-images:nearby-onboarding-splash-light';

/**
 * @type {string}
 */
const ONBOARDING_SPLASH_DARK_ICON =
    'nearby-images:nearby-onboarding-splash-dark';

Polymer({
  _template: html`{__html_template__}`,
  is: 'nearby-onboarding-page',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?NearbySettings} */
    settings: {
      type: Object,
    },

    /** @type {string} */
    errorMessage: {
      type: String,
      value: '',
    },

    /**
     * Whether the onboarding page is being rendered in dark mode.
     * @private {boolean}
     */
    isDarkModeActive_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'next': 'onNext_',
    'close': 'onClose_',
    'keydown': 'onKeydown_',
    'view-enter-start': 'onViewEnterStart_',
  },


  /** @private */
  onNext_() {
    this.submitDeviceNameInput_();
  },

  /** @private */
  onClose_() {
    processOnboardingCancelledMetrics(
        NearbyShareOnboardingFinalState.DEVICE_NAME_PAGE);
    this.fire('onboarding-cancelled');
  },

  /**
   * @param {!KeyboardEvent} e Event containing the key
   * @private
   */
  onKeydown_(e) {
    e.stopPropagation();
    if (e.key === 'Enter') {
      this.submitDeviceNameInput_();
      e.preventDefault();
    }
  },

  /** @private */
  onViewEnterStart_() {
    this.$$('#deviceName').focus();
    processOnboardingInitiatedMetrics(new URL(document.URL));
  },


  /** @private */
  onDeviceNameInput_() {
    getNearbyShareSettings()
        .validateDeviceName(this.$.deviceName.value)
        .then((result) => {
          this.updateErrorMessage_(result.result);
        });
  },

  /** @private */
  submitDeviceNameInput_() {
    getNearbyShareSettings()
        .setDeviceName(this.$.deviceName.value)
        .then((result) => {
          this.updateErrorMessage_(result.result);
          if (result.result ===
              nearbyShare.mojom.DeviceNameValidationResult.kValid) {
            this.fire('change-page', {page: 'visibility'});
          }
        });
  },

  /**
   * @private
   *
   * @param {!nearbyShare.mojom.DeviceNameValidationResult} validationResult The
   *     error status from validating the provided device name.
   */
  updateErrorMessage_(validationResult) {
    switch (validationResult) {
      case nearbyShare.mojom.DeviceNameValidationResult.kErrorEmpty:
        this.errorMessage = this.i18n('nearbyShareDeviceNameEmptyError');
        break;
      case nearbyShare.mojom.DeviceNameValidationResult.kErrorTooLong:
        this.errorMessage = this.i18n('nearbyShareDeviceNameTooLongError');
        break;
      case nearbyShare.mojom.DeviceNameValidationResult.kErrorNotValidUtf8:
        this.errorMessage =
            this.i18n('nearbyShareDeviceNameInvalidCharactersError');
        break;
      default:
        this.errorMessage = '';
        break;
    }
  },

  /**
   * @private
   *
   * @param {!string} errorMessage The error message.
   * @return {boolean} Whether or not the error message exists.
   */
  hasErrorMessage_(errorMessage) {
    return errorMessage !== '';
  },

  /**
   * Returns the icon based on Light/Dark mode.
   * @return {string}
   */
  getOnboardingSplashIcon_() {
    return this.isDarkModeActive_ ? ONBOARDING_SPLASH_DARK_ICON :
                                    ONBOARDING_SPLASH_LIGHT_ICON;
  },
});
