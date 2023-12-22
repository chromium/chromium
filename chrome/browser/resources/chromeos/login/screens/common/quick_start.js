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

import { assert } from '//resources/ash/common/assert.js';
import {afterNextRender, dom, flush, html, mixinBehaviors, Polymer, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {OobeTypes} from '../../components/oobe_types.js';
import { OobeI18nBehavior, OobeI18nBehaviorInterface } from '../../components/behaviors/oobe_i18n_behavior.js';
import { loadTimeData } from '../../i18n_setup.js';
import { QrCodeCanvas } from '../../components/qr_code_canvas.js';


/**
 * UI mode for the screen.
 * @enum {string}
 */
export const QuickStartUIState = {
  LOADING: 'loading',
  VERIFICATION: 'verification',
  CONNECTING_TO_WIFI: 'connecting_to_wifi',
  CONNECTED_TO_WIFI: 'connected_to_wifi',
  GAIA_CREDENTIALS: 'gaia_credentials',
  FIDO_ASSERTION_RECEIVED: 'fido_assertion_received',
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
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      shapes_: {
        type: Object,
        // Should be in sync with the C++ enum (ash::quick_start::Shape).
        value: {CIRCLE: 0, DIAMOND: 1, TRIANGLE: 2, SQUARE: 3},
        readOnly: true,
      },
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
      fidoAssertionEmail_: {
        type: String,
        value: '',
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
      'showConnectingToWifi',
      'setDiscoverableName',
      'showTransferringGaiaCredentials',
      'showFidoAssertionReceived',
    ];
  }

  getVerificationSubtitle(title) {
    return this.i18nAdvanced('quickStartSetupSubtitle', {
      substitutions:
        [loadTimeData.getString('deviceType'), this.discoverableName_],
    });
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('QuickStartScreen');

    // Helper for drawing the QR code using circles as per spec.
    this.qrCodeCanvas = new QrCodeCanvas(this.getCanvas_());
  }

  /** @override */
  defaultUIStep() {
    return QuickStartUIState.LOADING;
  }

  showConnectingToWifi() {
    this.setUIStep(QuickStartUIState.CONNECTING_TO_WIFI);
  }

  /**
   * @param {!Array<boolean>} qrCode
   */
  setQRCode(qrCode) {
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

  showTransferringGaiaCredentials() {
    this.setUIStep(QuickStartUIState.GAIA_CREDENTIALS);
  }

  showFidoAssertionReceived(email) {
    this.fidoAssertionEmail_ = email;
    this.setUIStep(QuickStartUIState.FIDO_ASSERTION_RECEIVED);
  }

  getCanvas_() {
    return this.shadowRoot.querySelector('#qrCodeCanvas');
  }

  onCancelClicked_() {
    this.userActed('cancel');
  }

  isEq_(a, b) {
    return a === b;
  }
}

customElements.define(QuickStartScreen.is, QuickStartScreen);
