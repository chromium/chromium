// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {PageName} from 'chrome://resources/cr_components/chromeos/multidevice_setup/multidevice_setup.m.js';
import {MultiDeviceSetupDelegate} from 'chrome://resources/cr_components/chromeos/multidevice_setup/multidevice_setup_delegate.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
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
    if (loadTimeData.valueExists('newLayoutEnabled') &&
        loadTimeData.getBoolean('newLayoutEnabled')) {
      document.documentElement.setAttribute('new-layout', '');
    } else {
      document.documentElement.removeAttribute('new-layout');
    }
    this.onWindowSizeUpdated_();
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
      'MultiDevice.PostOOBESetupFlow.PageShown', pageNameValue,
      PageNameValue.MAX_VALUE
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
    if (loadTimeData.valueExists('newLayoutEnabled') &&
        loadTimeData.getBoolean('newLayoutEnabled')) {
      if (window.innerWidth > window.innerHeight) {
        document.documentElement.setAttribute('orientation', 'horizontal');
      } else {
        document.documentElement.setAttribute('orientation', 'vertical');
      }
    }
  },
});
