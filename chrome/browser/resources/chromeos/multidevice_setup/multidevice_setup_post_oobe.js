// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('multidevice_setup_post_oobe', function() {
  /**
   * This enum is tied directly to a UMA enum defined in
   * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
   * change one without changing the other).
   * @enum {number}
   */
  PageNameValue = {
    UNKNOWN: 0,
    START: 1,
    PASSWORD: 2,
    SUCCESS: 3,
    MAX_VALUE: 4,
  };

  return {
    PageNameValue: PageNameValue,
  };
});

/**
 * MultiDevice setup flow which is shown after OOBE has completed.
 */
Polymer({
  is: 'multidevice-setup-post-oobe',

  properties: {
    /** @private {!multidevice_setup.MultiDeviceSetupDelegate} */
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
  attached() {
    this.delegate_ = new multidevice_setup.PostOobeDelegate();
    this.$$('multidevice-setup').initializeSetupFlow();
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
   * @param {!CustomEvent<!{value: multidevice_setup.PageName}>} event
   * @private
   */
  onVisiblePageNameChanged_(event) {
    let pageNameValue;
    switch (event.detail.value) {
      case multidevice_setup.PageName.START:
        pageNameValue = multidevice_setup_post_oobe.PageNameValue.START;
        break;
      case multidevice_setup.PageName.PASSWORD:
        pageNameValue = multidevice_setup_post_oobe.PageNameValue.PASSWORD;
        break;
      case multidevice_setup.PageName.SUCCESS:
        pageNameValue = multidevice_setup_post_oobe.PageNameValue.SUCCESS;
        break;
      default:
        console.warn('Unexpected PageName.');
        pageNameValue = multidevice_setup_post_oobe.PageNameValue.UNKNOWN;
        break;
    }

    chrome.send('metricsHandler:recordInHistogram', [
      'MultiDevice.PostOOBESetupFlow.PageShown', pageNameValue,
      multidevice_setup_post_oobe.PageNameValue.MAX_VALUE
    ]);
  }
});
