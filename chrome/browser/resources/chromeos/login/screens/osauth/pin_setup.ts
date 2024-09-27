// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';

import {SetupPinKeyboardElement} from '//resources/ash/common/quick_unlock/setup_pin_keyboard.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeTypes} from '../../components/oobe_types.js';

import {getTemplate} from './pin_setup.html.js';

enum PinSetupState {
  START = 'start',
  CONFIRM = 'confirm',
  DONE = 'done',
}

const PinSetupBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

class PinSetup extends PinSetupBase {
  static get is() {
    return 'pin-setup-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Flag from <setup-pin-keyboard>.
       */
      enableSubmit: {
        type: Boolean,
        value: false,
      },

      /**
       * Flag from <setup-pin-keyboard>.
       */
      isConfirmStep: {
        type: Boolean,
        value: false,
        observer: 'onIsConfirmStepChanged',
      },

      /** QuickUnlockPrivate API token. */
      authToken: {
        type: String,
      },

      /**
       * Interface for chrome.quickUnlockPrivate calls. May be overridden by
       * tests.
       */
      quickUnlockPrivate: {
        type: Object,
        value: chrome.quickUnlockPrivate,
      },

      /**
       * Should be true when device has support for PIN login.
       */
      hasLoginSupport: {
        type: Boolean,
        value: false,
      },

      /**
       * True when PIN is being offered as the main sign-in factor.
       */
      usingPinAsMainSignInFactor: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether user is a child account.
       */
      isChildAccount: {
        type: Boolean,
        value: false,
      },
    };
  }

  private enableSubmit: boolean;
  private isConfirmStep: boolean;
  authToken: string;
  private quickUnlockPrivate: typeof chrome.quickUnlockPrivate;
  private hasLoginSupport: boolean;
  private usingPinAsMainSignInFactor: boolean;
  isChildAccount: boolean;

  override get UI_STEPS() {
    return PinSetupState;
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.ONBOARDING;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('PinSetupScreen');
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return PinSetupState.START;
  }

  private getPinKeyboard(): SetupPinKeyboardElement {
    const pinKeyboard = this.shadowRoot?.querySelector('#pinKeyboard');
    assert(pinKeyboard instanceof SetupPinKeyboardElement);
    return pinKeyboard;
  }

  override onBeforeShow(data: OobeTypes.PinSetupScreenParameters): void {
    super.onBeforeShow(data);
    this.getPinKeyboard().resetState();
    this.authToken = data.authToken;
    this.isChildAccount = data.isChildAccount;
    this.hasLoginSupport = data.hasLoginSupport;
    this.usingPinAsMainSignInFactor = data.usingPinAsMainSignInFactor;
  }

  private onIsConfirmStepChanged(): void {
    if (this.isConfirmStep) {
      this.setUIStep(PinSetupState.CONFIRM);
    }
  }

  private onPinSubmit(): void {
    this.getPinKeyboard().doSubmit();
  }

  private onSetPinDone(): void {
    this.setUIStep(PinSetupState.DONE);
  }

  private getSkipButtonLabel(usingPinAsMainSignInFactor: boolean): string {
    return usingPinAsMainSignInFactor ? 'discoverPinSetupPinAsMainFactorSkip' :
                                        'discoverPinSetupSkip';
  }

  private onSkipButton(): void {
    this.authToken = '';
    this.getPinKeyboard().resetState();
    if (this.uiStep === PinSetupState.CONFIRM) {
      this.userActed('skip-button-in-flow');
    } else {
      this.userActed('skip-button-on-start');
    }
  }

  private onBackButton(): void {
    this.getPinKeyboard().resetState();
    this.setUIStep(PinSetupState.START);
  }

  private onNextButton(): void {
    this.onPinSubmit();
  }

  private onDoneButton(): void {
    this.authToken = '';
    this.getPinKeyboard().resetState();
    this.userActed('done-button');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PinSetup.is]: PinSetup;
  }
}

customElements.define(PinSetup.is, PinSetup);
