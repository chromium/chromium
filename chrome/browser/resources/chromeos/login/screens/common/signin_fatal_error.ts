// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for signin fatal error.
 */

import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/buttons/oobe_text_button.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {OobeTypes} from '../../components/oobe_types.js';

import {getTemplate} from './signin_fatal_error.html.js';

const SigninFatalErrorBase =
    mixinBehaviors(
        [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
        PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          OobeDialogHostBehaviorInterface & LoginScreenBehaviorInterface,
    };

interface SigninFatalErrorScreenData {
  errorState: OobeTypes.FatalErrorCode;
  errorText: string|undefined;
  keyboardHint: string|undefined;
  details: string|undefined;
  helpLinkText: string|undefined;
  url: string|undefined;
}

export class SigninFatalScreen extends SigninFatalErrorBase {
  static get is() {
    return 'signin-fatal-error-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Subtitle that will be shown to the user describing the error
       */
      errorSubtitle: {
        type: String,
        computed: 'computeSubtitle(locale, errorState, params)',
      },

      /**
       * Error state from the screen
       */
      errorState: {
        type: Number,
        value: OobeTypes.FatalErrorCode.UNKNOWN,
      },

      /**
       * Additional information that will be used when creating the subtitle.
       */
      params: {
        type: Object,
        value: {},
      },

      keyboardHint: {
        type: String,
      },

      details: {
        type: String,
      },

      helpLinkText: {
        type: String,
      },
    };
  }

  private errorSubtitle: string;
  private errorState: number;
  private params: SigninFatalErrorScreenData;
  private keyboardHint: string|undefined;
  private details: string|undefined;
  private helpLinkText: string|undefined;

  override ready() {
    super.ready();
    this.initializeLoginScreen('SignInFatalErrorScreen');
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.BLOCKING;
  }

  /**
   * Returns the control which should receive initial focus.
   */
  override get defaultControl(): HTMLElement|null {
    const actionButton =
        this.shadowRoot?.querySelector<HTMLElement>('#actionButton');
    return actionButton ? actionButton : null;
  }

  /**
   * Invoked just before being shown. Contains all the data for the screen.
   * @param data Screen init payload.
   */
  override onBeforeShow(data: SigninFatalErrorScreenData) {
    this.params = data;
    this.errorState = data.errorState;
    this.keyboardHint = data.keyboardHint;
    this.details = data.details;
    this.helpLinkText = data.helpLinkText;
  }

  private onClick() {
    this.userActed('screen-dismissed');
  }

  /**
   * Generates the key for the button that is shown to the
   * user based on the error
   */
  private computeButtonKey(errorState: OobeTypes.FatalErrorCode) {
    if (errorState == OobeTypes.FatalErrorCode.INSECURE_CONTENT_BLOCKED) {
      return 'fatalErrorDoneButton';
    }

    return 'fatalErrorTryAgainButton';
  }

  /**
   * Generates the subtitle that is shown to the
   * user based on the error
   */
  private computeSubtitle(
      locale: string, errorState: OobeTypes.FatalErrorCode,
      params: SigninFatalErrorScreenData): string {
    switch (errorState) {
      case OobeTypes.FatalErrorCode.SCRAPED_PASSWORD_VERIFICATION_FAILURE:
        return this.i18nDynamic(locale, 'fatalErrorMessageVerificationFailed');
      case OobeTypes.FatalErrorCode.MISSING_GAIA_INFO:
        return this.i18nDynamic(locale, 'fatalErrorMessageNoAccountDetails');
      case OobeTypes.FatalErrorCode.INSECURE_CONTENT_BLOCKED:
        const url = params.url;
        assert(url);
        return this.i18nDynamic(locale, 'fatalErrorMessageInsecureURL', url);
      case OobeTypes.FatalErrorCode.CUSTOM:
        assert(params.errorText);
        return params.errorText;
      default:
        return '';
    }
  }

  private onHelpLinkClicked() {
    this.userActed('learn-more');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SigninFatalScreen.is]: SigninFatalScreen;
  }
}

customElements.define(SigninFatalScreen.is, SigninFatalScreen);
