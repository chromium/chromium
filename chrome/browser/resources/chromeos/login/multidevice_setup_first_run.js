// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiDevice setup flow Polymer element to be used in the first
 *     run (i.e., after OOBE or during the user's first login on this
 *     Chromebook).
 */

cr.define('multidevice_setup', function() {
  /** @implements {multidevice_setup.MultiDeviceSetupDelegate} */
  class MultiDeviceSetupFirstRunDelegate {
    constructor() {
      /**
       * @private {?chromeos.multideviceSetup.mojom.
       *               PrivilegedHostDeviceSetterRemote}
       */
      this.remote_ = null;
    }

    /** @override */
    isPasswordRequiredToSetHost() {
      return false;
    }

    /** @override */
    setHostDevice(hostDeviceId, opt_authToken) {
      // An authentication token is not expected since a password is not
      // required.
      assert(!opt_authToken);

      if (!this.remote_) {
        this.remote_ = chromeos.multideviceSetup.mojom
                           .PrivilegedHostDeviceSetter.getRemote();
      }

      return /** @type {!Promise<{success: boolean}>} */ (
          this.remote_.setHostDevice(hostDeviceId));
    }

    /** @override */
    shouldExitSetupFlowAfterSettingHost() {
      return true;
    }

    /** @override */
    getStartSetupCancelButtonTextId() {
      return 'noThanks';
    }
  }

  const MultiDeviceSetupFirstRun = Polymer({
    is: 'multidevice-setup-first-run',

    behaviors: [I18nBehavior, WebUIListenerBehavior],

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
       * @private {boolean}
       */
      forwardButtonDisabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * ID of loadTimeData string to be shown on the cancel button.
       * @private {string|undefined}
       */
      cancelButtonTextId_: {
        type: String,
      },

      /** Whether the webview overlay should be hidden. */
      webviewOverlayHidden_: {
        type: Boolean,
        value: true,
      },

      /** Whether the webview is currently loading. */
      isWebviewLoading_: {
        type: Boolean,
        value: false,
      },

      /**
       * URL for the webview to display.
       * @private {string|undefined}
       */
      webviewSrc_: {
        type: String,
        value: '',
      },
    },

    listeners: {
      'open-learn-more-webview-requested': 'onOpenLearnMoreWebviewRequested_',
    },

    /** @override */
    attached: function() {
      this.delegate_ = new MultiDeviceSetupFirstRunDelegate();
      this.$.multideviceHelpOverlayWebview.addEventListener(
          'contentload', () => {
            this.isWebviewLoading_ = false;
          });
    },

    /** @override */
    ready: function() {
      this.updateLocalizedContent();
    },

    updateLocalizedContent: function() {
      this.i18nUpdateLocale();
      this.$.multideviceSetup.updateLocalizedContent();
    },

    onForwardButtonFocusRequested_: function() {
      this.$.nextButton.focus();
    },

    /**
     * @param {!CustomEvent<!{didUserCompleteSetup: boolean}>} event
     * @private
     */
    onExitRequested_: function(event) {
      if (event.detail.didUserCompleteSetup) {
        chrome.send(
            'login.MultiDeviceSetupScreen.userActed', ['setup-accepted']);
      } else {
        chrome.send(
            'login.MultiDeviceSetupScreen.userActed', ['setup-declined']);
      }
    },

    /** @private */
    hideWebviewOverlay_: function() {
      this.webviewOverlayHidden_ = true;
    },

    /**
     * @param {!CustomEvent<string>} event
     * @private
     */
    onOpenLearnMoreWebviewRequested_: function(event) {
      this.isWebviewLoading_ = true;
      this.webviewSrc_ = event.detail;
      this.webviewOverlayHidden_ = false;
    },
  });

  return {
    MultiDeviceSetupFirstRun: MultiDeviceSetupFirstRun,
  };
});
