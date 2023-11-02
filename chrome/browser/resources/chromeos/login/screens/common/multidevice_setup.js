// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiDevice setup screen for login/OOBE.
 */

/* #js_imports_placeholder */
import {PrivilegedHostDeviceSetter, PrivilegedHostDeviceSetterRemote} from 'chrome://resources/mojo/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom-webui.js';

/** @implements {MultiDeviceSetupDelegate} */
class MultiDeviceSetupScreenDelegate {

  constructor() {
    /**
     * @private {?PrivilegedHostDeviceSetterRemote}
     */
    this.remote_ = null;
  }

  /** @override */
  isPasswordRequiredToSetHost() {
    return false;
  }

  /** @override */
  setHostDevice(hostInstanceIdOrLegacyDeviceId, opt_authToken) {
    // An authentication token is not expected since a password is not
    // required.
    assert(!opt_authToken);

    if (!this.remote_) {
      this.remote_ = PrivilegedHostDeviceSetter.getRemote();
    }

    return /** @type {!Promise<{success: boolean}>} */ (
        this.remote_.setHostDevice(hostInstanceIdOrLegacyDeviceId));
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

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
 const MultiDeviceSetupScreenBase = Polymer.mixinBehaviors(
  [OobeI18nBehavior, LoginScreenBehavior, WebUIListenerBehavior],
  Polymer.Element);

/**
 * @polymer
 */
class MultiDeviceSetupScreen extends MultiDeviceSetupScreenBase {
  static get is() {
    return 'multidevice-setup-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
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
    };
  }

  constructor() {
    super();
    this.delegate_ = new MultiDeviceSetupScreenDelegate();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.$.multideviceHelpOverlayWebview.addEventListener(
        'contentload', () => {
          this.isWebviewLoading_ = false;
        });
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('MultiDeviceSetupScreen');
    this.updateLocalizedContent();
  }

  updateLocalizedContent() {
    this.i18nUpdateLocale();
    this.$.multideviceSetup.updateLocalizedContent();
  }

  onForwardButtonFocusRequested_() {
    this.$.nextButton.focus();
  }

  /**
   * @param {!CustomEvent<!{didUserCompleteSetup: boolean}>} event
   * @private
   */
  onExitRequested_(event) {
    if (event.detail.didUserCompleteSetup) {
      chrome.send(
          'login.MultiDeviceSetupScreen.userActed', ['setup-accepted']);
    } else {
      chrome.send(
          'login.MultiDeviceSetupScreen.userActed', ['setup-declined']);
    }
  }

  /** @private */
  hideWebviewOverlay_() {
    this.webviewOverlayHidden_ = true;
  }

  /**
   * @param {!CustomEvent<string>} event
   * @private
   */
  onOpenLearnMoreWebviewRequested_(event) {
    this.isWebviewLoading_ = true;
    this.webviewSrc_ = event.detail;
    this.webviewOverlayHidden_ = false;
  }
}

customElements.define(MultiDeviceSetupScreen.is, MultiDeviceSetupScreen);
