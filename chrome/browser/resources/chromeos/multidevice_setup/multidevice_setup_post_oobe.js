// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './strings.m.js';
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {PageName} from 'chrome://resources/ash/common/multidevice_setup/multidevice_setup.js';
import {MultiDeviceSetupDelegate} from 'chrome://resources/ash/common/multidevice_setup/multidevice_setup_delegate.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PostOobeDelegate} from './post_oobe_delegate.js';

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * @enum {number}
 */
const PageNameValue = {
  UNKNOWN: 0,
  START: 1,
  PASSWORD: 2,
  SUCCESS: 3,
  MAX_VALUE: 4,
};

/**
 * MultiDevice setup flow which is shown after OOBE has completed.
 */
Polymer({
  is: 'multidevice-setup-post-oobe',

  _template: html`{__html_template__}`,

  properties: {
    /** @private {!MultiDeviceSetupDelegate} */
    delegate_: Object,

    /**
     * ID of loadTimeData string to be shown on the forward navigation button.
     * @private {string|undefined}
     */
    forwardButtonTextId_: {
      type: String,
    },

    /**
     * Whether the forward button should be disabled.
     * @private
     */
    forwardButtonDisabled_: {
      type: Boolean,
      value: false,
    },

    /**
     * ID of loadTimeData string to be shown on the cancel navigation button.
     * @private {string|undefined}
     */
    cancelButtonTextId_: {
      type: String,
    },

    /**
     * ID of loadTimeData string to be shown on the backward navigation button.
     * @private {string|undefined}
     */
    backwardButtonTextId_: {
      type: String,
    },
  },

  behaviors: [I18nBehavior],

  /** @override */
  ready() {
    this.onWindowSizeUpdated_();

    document.body.classList.add('jelly-enabled');

    // Start listening for color changes in 'chrome://theme/colors.css'.
    /** @suppress {checkTypes} */
    (function() {
      ColorChangeUpdater.forDocument().start();
    })();
  },

  /** @override */
  attached() {
    this.delegate_ = new PostOobeDelegate();
    this.$$('multidevice-setup').initializeSetupFlow();
    window.addEventListener('orientationchange', this.onWindowSizeUpdated_);
    window.addEventListener('resize', this.onWindowSizeUpdated_);
  },

  /** @override */
  detached() {
    window.removeEventListener('orientationchange', this.onWindowSizeUpdated_);
    window.removeEventListener('resize', this.onWindowSizeUpdated_);
  },

  /** @private */
  onExitRequested_() {
    chrome.send('dialogClose');
  },

  /** @private */
  onForwardButtonFocusRequested_() {
    this.$$('#forward-button').focus();
  },

  /**
   * @param {!CustomEvent<!{value: PageName}>} event
   * @private
   */
  onVisiblePageNameChanged_(event) {
    let pageNameValue;
    switch (event.detail.value) {
      case PageName.START:
        pageNameValue = PageNameValue.START;
        break;
      case PageName.PASSWORD:
        pageNameValue = PageNameValue.PASSWORD;
        break;
      case PageName.SUCCESS:
        pageNameValue = PageNameValue.SUCCESS;
        break;
      default:
        console.warn('Unexpected PageName.');
        pageNameValue = PageNameValue.UNKNOWN;
        break;
    }

    chrome.send('metricsHandler:recordInHistogram', [
      'MultiDevice.PostOOBESetupFlow.PageShown',
      pageNameValue,
      PageNameValue.MAX_VALUE,
    ]);
  },

  /**
   * Called during initialization, when the window is resized, or the window's
   * orientation is updated.
   */
  onWindowSizeUpdated_() {
    // Below code is also used to set the dialog size for display manager and
    // in-session assistant onboarding flow. Please make sure code changes are
    // applied to all places.
    document.documentElement.style.setProperty(
        '--oobe-oobe-dialog-height-base', window.innerHeight + 'px');
    document.documentElement.style.setProperty(
        '--oobe-oobe-dialog-width-base', window.innerWidth + 'px');
    if (window.innerWidth > window.innerHeight) {
      document.documentElement.setAttribute('orientation', 'horizontal');
    } else {
      document.documentElement.setAttribute('orientation', 'vertical');
    }
  },

  /**
   * Wraps i18n to return early if text is not yet defined. This prevents
   * console errors since some of the strings are initially undefined. Variables
   * like |cancelButtonTextId_| are initially undefined because they get piped
   * by a 2-way data binding from the embedded multidevice-setup component. This
   * does not affect the ui since these variables get defined shortly after the
   * page is initialized. We purposely don't set some of these properties if the
   * button is not expected to be shown in which case they will remain
   * undefined.
   * @param {string|undefined} text
   * @return {string}
   */
  getButtonText_(text) {
    if (!text) {
      return '';
    }

    return this.i18n(text);
  },
});
