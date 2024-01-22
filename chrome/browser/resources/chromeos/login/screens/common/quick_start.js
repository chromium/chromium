// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying quick start screen.
 */

import '//resources/polymer/v3_0/paper-styles/color.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/quick_start_pin.js';

import {assert} from '//resources/ash/common/assert.js';
import {flush, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {OobeTypes} from '../../components/oobe_types.js';
import {QrCodeCanvas} from '../../components/qr_code_canvas.js';
import {loadTimeData} from '../../i18n_setup.js';

import {getTemplate} from './quick_start.html.js';


/**
 * UI mode for the screen.
 * @enum {string}
 */
export const QuickStartUIState = {
  DEFAULT: 'default',
  CONNECTING_TO_PHONE: 'connecting_to_phone',
  VERIFICATION: 'verification',
  CONNECTING_TO_WIFI: 'connecting_to_wifi',
  CONNECTED_TO_WIFI: 'connected_to_wifi',
  CONFIRM_GOOGLE_ACCOUNT: 'confirm_google_account',
  SIGNING_IN: 'signing_in',
  SETUP_COMPLETE: 'setup_complete',
};

const UserActions = {
  CANCEL: 'cancel',
  NEXT: 'next',
  TURN_ON_BLUETOOTH: 'turn_on_bluetooth',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const QuickStartScreenBase =
  mixinBehaviors([LoginScreenBehavior, MultiStepBehavior, OobeI18nBehavior], PolymerElement);

/**
 * @polymer
 */
class QuickStartScreen extends QuickStartScreenBase {
  static get is() {
    return 'quick-start-element';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      discoverableName_: {
        type: String,
        value: '',
      },
      pin_: {
        type: String,
        value: '0000',
      },
      // Whether to show the PIN for verification instead of a QR code.
      usePinInsteadOfQrForVerification_: {
        type: Boolean,
        value: false,
      },
      userEmail_: {
        type: String,
        value: '',
      },
      userFullName_: {
        type: String,
        value: '',
      },
      userAvatarUrl_: {
        type: String,
        value: '',
      },
      // Once account creation starts, it is no longer possible to cancel.
      canCancelSignin_: {
        type: Boolean,
        value: true,
      },
    };
  }

  constructor() {
    super();
    this.UI_STEPS = QuickStartUIState;
    this.discoverableName_ = '';
    this.usePinInsteadOfQrForVerification_ = false;
    this.qrCodeCanvas = null;
  }

  get EXTERNAL_API() {
    return [
      'setQRCode',
      'setPin',
      'showInitialUiStep',
      'showBluetoothDialog',
      'showConnectingToPhoneStep',
      'showConnectingToWifi',
      'setDiscoverableName',
      'showConfirmGoogleAccount',
      'showSigningInStep',
      'showCreatingAccountStep',
      'showSetupCompleteStep',
      'setUserEmail',
      'setUserFullName',
      'setUserAvatarUrl',
    ];
  }

  getVerificationSubtitle(title) {
    return this.i18nAdvanced('quickStartSetupSubtitle', {
      substitutions:
        [loadTimeData.getString('deviceType'), this.discoverableName_],
    });
  }

  getSetupCompleteTitle(locale) {
    return this.i18nAdvanced('quickStartSetupCompleteTitle', {
      substitutions: [loadTimeData.getString('deviceType')],
    });
  }

  getSetupCompleteSubtitle(locale, email) {
    return this.i18nAdvanced('quickStartSetupCompleteSubtitle', {
      substitutions: [this.userEmail_],
    });
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('QuickStartScreen');

    // Helper for drawing the QR code using circles as per spec.
    this.qrCodeCanvas = new QrCodeCanvas(this.getCanvas_());
  }

  onBeforeHide() {
    this.$.spinner.playing = false;
  }

  /** @override */
  defaultUIStep() {
    return QuickStartUIState.DEFAULT;
  }

  showInitialUiStep() {
    this.setUIStep(this.defaultUIStep());
  }

  showConnectingToPhoneStep() {
    this.$.quickStartBluetoothDialog.hideDialog();
    this.setUIStep(QuickStartUIState.CONNECTING_TO_PHONE);
  }

  showConnectingToWifi() {
    this.setUIStep(QuickStartUIState.CONNECTING_TO_WIFI);
  }

  /**
   * @param {!Array<boolean>} qrCode
   */
  setQRCode(qrCode) {
    this.$.quickStartBluetoothDialog.hideDialog();
    this.usePinInsteadOfQrForVerification_ = false;
    this.setUIStep(QuickStartUIState.VERIFICATION);
    flush();

    this.qrCodeCanvas.setData(qrCode);
  }

  setPin(pin) {
    this.usePinInsteadOfQrForVerification_ = true;
    this.setUIStep(QuickStartUIState.VERIFICATION);
    assert(pin.length === 4);
    this.pin_ = pin;
  }

  setDiscoverableName(discoverableName) {
    this.discoverableName_ = discoverableName;
  }

  showConfirmGoogleAccount() {
    this.setUIStep(QuickStartUIState.CONFIRM_GOOGLE_ACCOUNT);
  }

  showSigningInStep() {
    this.setUIStep(QuickStartUIState.SIGNING_IN);
    this.$.spinner.playing = true;
  }

  showCreatingAccountStep() {
    // Same UI as 'Signing in...' but without a cancel button.
    this.setUIStep(QuickStartUIState.SIGNING_IN);
    this.canCancelSignin_ = false;
  }

  showSetupCompleteStep() {
    this.setUIStep(QuickStartUIState.SETUP_COMPLETE);
  }

  setUserEmail(email) {
    this.userEmail_ = email;
  }

  setUserFullName(userFullName) {
    this.userFullName_ = userFullName;
  }

  setUserAvatarUrl(userAvatarUrl) {
    this.userAvatarUrl_ = userAvatarUrl;
  }

  getCanvas_() {
    return this.shadowRoot.querySelector('#qrCodeCanvas');
  }

  showBluetoothDialog() {
    // Shown on top of the QR code step.
    this.setUIStep(QuickStartUIState.VERIFICATION);
    this.$.quickStartBluetoothDialog.showDialog();
  }

  cancelBluetoothDialog_() {
    this.$.quickStartBluetoothDialog.hideDialog();
    this.userActed(UserActions.CANCEL);
  }

  turnOnBluetooth_() {
    this.$.quickStartBluetoothDialog.hideDialog();
    this.userActed(UserActions.TURN_ON_BLUETOOTH);
  }

  /**
   * Wrap the user avatar as an image into a html snippet.
   *
   * @param {string} avatarUri the icon uri to be wrapped.
   * @return {string} wrapped html snippet.
   *
   * @private
   */
  getWrappedAvatar_(avatarUri) {
    return ('data:text/html;charset=utf-8,' + encodeURIComponent(String.raw`
    <html>
      <style>
        body {
          margin: 0;
        }
        #avatar {
          width: 32px;
          height: 32px;
          user-select: none;
          border-radius: 50%;
        }
      </style>
    <body><img id="avatar" src="` + avatarUri + '"></body></html>'));
  }

  onCancelClicked_() {
    this.userActed(UserActions.CANCEL);
  }

  onNextClicked_() {
    this.userActed(UserActions.NEXT);
  }

  isEq_(a, b) {
    return a === b;
  }
}

customElements.define(QuickStartScreen.is, QuickStartScreen);
