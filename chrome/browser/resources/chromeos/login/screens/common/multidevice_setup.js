// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiDevice setup screen for login/OOBE.
 */
import '//resources/ash/common/multidevice_setup/mojo_api.js';
import '//resources/ash/common/multidevice_setup/multidevice_setup_shared.css.js';
import '//resources/ash/common/multidevice_setup/multidevice_setup.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/throbber_notice.js';

import {MultiDeviceSetupDelegate} from '//resources/ash/common/multidevice_setup/multidevice_setup_delegate.js';
import {WebUIListenerBehavior} from '//resources/ash/common/web_ui_listener_behavior.js';
import {assert} from '//resources/ash/common/assert.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrivilegedHostDeviceSetter, PrivilegedHostDeviceSetterRemote} from 'chrome://resources/mojo/chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom-webui.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';

// OOBE screen that wraps MultiDevice setup flow when displayed during the
// user's onboarding on this Chromebook. Note that this flow is slightly
// different from the post-OOBE flow ( `c/b/r/chromeos/multidevice_setup/` )
// in 3 ways:
//  (1) During onboarding, the user has just entered their password, so we
//      do not prompt the user to enter a password before continuing.
//  (2) During onboarding, once the user selects a host device, we continue to
//      the next OOBE/login task; in the post-OOBE mode, there is a "success"
//      screen.
//  (3) During onboarding, buttons are styled with custom OOBE buttons.

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
 * @implements {OobeI18nBehaviorInterface}
 */
const MultiDeviceSetupScreenBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, WebUIListenerBehavior],
    PolymerElement);

/**
 * @polymer
 */
class MultiDeviceSetupScreen extends MultiDeviceSetupScreenBase {
  static get is() {
    return 'multidevice-setup-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

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
