// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/quick_start_pin.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {flush, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeCrLottie} from '../../components/oobe_cr_lottie.js';
import {QrCodeCanvas} from '../../components/qr_code_canvas.js';
import {loadTimeData} from '../../i18n_setup.js';

import {getTemplate} from './quick_start.html.js';

/**
 * UI mode for the screen.
 */
enum QuickStartUiState {
  DEFAULT = 'default',
  CONNECTING_TO_PHONE = 'connecting_to_phone',
  VERIFICATION = 'verification',
  CONNECTING_TO_WIFI = 'connecting_to_wifi',
  CONNECTED_TO_WIFI = 'connected_to_wifi',
  CONFIRM_GOOGLE_ACCOUNT = 'confirm_google_account',
  SIGNING_IN = 'signing_in',
  SETUP_COMPLETE = 'setup_complete',
}

enum UserActions {
  CANCEL = 'cancel',
  NEXT = 'next',
  TURN_ON_BLUETOOTH = 'turn_on_bluetooth',
}

const QuickStartScreenBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

export class QuickStartScreen extends QuickStartScreenBase {
  static get is() {
    return 'quick-start-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      pin: {
        type: String,
        value: '0000',
      },
      // Whether to show the PIN for verification instead of a QR code.
      usePinInsteadOfQrForVerification: {
        type: Boolean,
        value: false,
      },
      userEmail: {
        type: String,
        value: '',
      },
      userFullName: {
        type: String,
        value: '',
      },
      userAvatarUrl: {
        type: String,
        value: '',
      },
      // Once account creation starts, it is no longer possible to cancel.
      canCancelSignin: {
        type: Boolean,
        value: true,
      },
      willRequestWiFi: {
        type: Boolean,
        value: true,
      },
      didTransferWiFi: {
        type: Boolean,
        value: false,
      },
      // Set once a QR code is set. Until then, a placeholder is shown.
      qrCodeAvailable: {
        type: Boolean,
        value: false,
      },
    };
  }

  private pin: string;
  private usePinInsteadOfQrForVerification: boolean;
  private userEmail: string;
  private userFullName: string;
  private userAvatarUrl: string;
  private canCancelSignin: boolean;
  private willRequestWiFi: boolean;
  private qrCodeAvailable: boolean;
  private qrCodeCanvas: QrCodeCanvas|null;
  private didTransferWiFi: boolean;

  constructor() {
    super();
    this.qrCodeCanvas = null;
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setQRCode',
      'setPin',
      'showInitialUiStep',
      'showBluetoothDialog',
      'showConnectingToPhoneStep',
      'showConnectingToWifi',
      'showConfirmGoogleAccount',
      'showSigningInStep',
      'showCreatingAccountStep',
      'showSetupCompleteStep',
      'setUserEmail',
      'setUserFullName',
      'setUserAvatarUrl',
      'setWillRequestWiFi',
    ];
  }

  private getVerificationSubtitle(_title: string): TrustedHTML {
    if (this.willRequestWiFi) {
      return this.i18nAdvanced('quickStartSetupSubtitle', {
        substitutions: [loadTimeData.getString('deviceType')],
      });
    } else {
      return this.i18nAdvanced('quickStartSetupSubtitleAccountOnly', {
        substitutions: [loadTimeData.getString('deviceType')],
      });
    }
  }

  private getSetupCompleteTitle(locale: string): TrustedHTML {
    return this.i18nAdvancedDynamic(locale, 'quickStartSetupCompleteTitle', {
      substitutions: [loadTimeData.getString('deviceType')],
    });
  }

  private getSetupCompleteSubtitle(
      locale: string, _email: string, _didTransferWiFi: boolean): TrustedHTML {
    if (this.didTransferWiFi) {
      return this.i18nAdvancedDynamic(
          locale, 'quickStartSetupCompleteSubtitleBoth', {
            substitutions: [this.userEmail],
          });
    } else {
      return this.i18nAdvancedDynamic(
          locale, 'quickStartSetupCompleteSubtitleSignedIn', {
            substitutions: [this.userEmail],
          });
    }
  }

  private getCanvas(): HTMLCanvasElement {
    const canvas = this.shadowRoot?.querySelector('#qrCodeCanvas');
    assert(canvas instanceof HTMLCanvasElement);
    return canvas;
  }

  private getQuickStartBluetoothDialog(): OobeModalDialog {
    const dialog = this.shadowRoot?.querySelector('#quickStartBluetoothDialog');
    assert(dialog instanceof OobeModalDialog);
    return dialog;
  }

  private getSpinnerAnimation(): OobeCrLottie {
    const animation = this.shadowRoot?.querySelector('#spinner');
    assert(animation instanceof OobeCrLottie);
    return animation;
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('QuickStartScreen');

    // Helper for drawing the QR code using circles as per spec.
    this.qrCodeCanvas = new QrCodeCanvas(this.getCanvas());
  }

  override onBeforeHide(): void {
    super.onBeforeHide();
    this.getSpinnerAnimation().playing = false;
  }

  override get UI_STEPS() {
    return QuickStartUiState;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return QuickStartUiState.DEFAULT;
  }

  showInitialUiStep(): void {
    this.setUIStep(this.defaultUIStep());
  }

  showConnectingToPhoneStep(): void {
    this.getQuickStartBluetoothDialog().hideDialog();
    this.setUIStep(QuickStartUiState.CONNECTING_TO_PHONE);
  }

  showConnectingToWifi(): void {
    this.setUIStep(QuickStartUiState.CONNECTING_TO_WIFI);
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  setQRCode(qrCodeData: boolean[], qrCodeURL: string): void {
    this.getQuickStartBluetoothDialog().hideDialog();
    this.usePinInsteadOfQrForVerification = false;
    this.setUIStep(QuickStartUiState.VERIFICATION);
    flush();

    this.qrCodeCanvas?.setData(qrCodeData);
    this.shadowRoot?.querySelector('#qrCodeCanvas')
        ?.setAttribute('qr-code-url', qrCodeURL);
    this.qrCodeAvailable = true;
  }

  setPin(pin: string): void {
    this.usePinInsteadOfQrForVerification = true;
    this.setUIStep(QuickStartUiState.VERIFICATION);
    assert(pin.length === 4);
    this.pin = pin;
  }

  showConfirmGoogleAccount(): void {
    this.setUIStep(QuickStartUiState.CONFIRM_GOOGLE_ACCOUNT);
  }

  showSigningInStep(): void {
    this.setUIStep(QuickStartUiState.SIGNING_IN);
    this.getSpinnerAnimation().playing = true;
  }

  showCreatingAccountStep(): void {
    // Same UI as 'Signing in...' but without a cancel button.
    this.setUIStep(QuickStartUiState.SIGNING_IN);
    this.canCancelSignin = false;
  }

  showSetupCompleteStep(didTransferWiFi: boolean): void {
    this.didTransferWiFi = didTransferWiFi;
    this.setUIStep(QuickStartUiState.SETUP_COMPLETE);
  }

  setUserEmail(email: string): void {
    this.userEmail = email;
  }

  setUserFullName(userFullName: string): void {
    this.userFullName = userFullName;
  }

  setUserAvatarUrl(userAvatarUrl: string): void {
    this.userAvatarUrl = userAvatarUrl;
  }

  setWillRequestWiFi(willRequestWiFi: boolean): void {
    this.willRequestWiFi = willRequestWiFi;
  }

  showBluetoothDialog() {
    // Shown on top of the QR code step.
    this.setUIStep(QuickStartUiState.VERIFICATION);
    this.getQuickStartBluetoothDialog().showDialog();
  }

  private cancelBluetoothDialog() {
    this.getQuickStartBluetoothDialog().hideDialog();
    this.userActed(UserActions.CANCEL);
  }

  private turnOnBluetooth() {
    this.getQuickStartBluetoothDialog().hideDialog();
    this.userActed(UserActions.TURN_ON_BLUETOOTH);
  }

  /**
   * Wrap the user avatar as an image into a html snippet.
   *
   * @param avatarUri the icon uri to be wrapped.
   * @return wrapped html snippet.
   *
   */
  private getWrappedAvatar(avatarUri: string): string {
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

  private onCancelClicked(): void {
    this.userActed(UserActions.CANCEL);
  }

  private onNextClicked(): void {
    this.userActed(UserActions.NEXT);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [QuickStartScreen.is]: QuickStartScreen;
  }
}

customElements.define(QuickStartScreen.is, QuickStartScreen);
