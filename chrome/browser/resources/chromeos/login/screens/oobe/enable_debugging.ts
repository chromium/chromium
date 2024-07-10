// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Enable developer features screen implementation.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/action_link.css.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/buttons/oobe_text_button.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './enable_debugging.html.js';

/**
 * Possible UI states of the enable debugging screen.
 * These values must be kept in sync with EnableDebuggingScreenHandler::UIState
 * in C++ code and the order of the enum must be the same.
 */
enum EnableDebuggingState {
  ERROR = 'error',
  NONE = 'none',
  REMOVE_PROTECTION = 'remove-protection',
  SETUP = 'setup',
  WAIT = 'wait',
  DONE = 'done',
}

const EnableDebuggingBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

export class EnableDebugging extends EnableDebuggingBase {
  static get is() {
    return 'enable-debugging-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  override get EXTERNAL_API(): string[] {
    return ['updateState'];
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Current value of password input field.
       */
      password_: {type: String, value: ''},

      /**
       * Current value of repeat password input field.
       */
      passwordRepeat_: {type: String, value: ''},

      /**
       * Whether password input fields are matching.
       */
      passwordsMatch_: {
        type: Boolean,
        computed: 'computePasswordsMatch_(password_, passwordRepeat_)',
      },
    };
  }

  private password_: string;
  private passwordRepeat_: string;
  private passwordsMatch_: boolean;

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('EnableDebuggingScreen');
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return EnableDebuggingState.NONE;
  }

  override get UI_STEPS() {
    return EnableDebuggingState;
  }

  /**
   * Returns a control which should receive an initial focus.
   */
  override get defaultControl(): HTMLElement|null {
    if (this.uiStep === EnableDebuggingState.REMOVE_PROTECTION) {
      return this.shadowRoot!.querySelector('#removeProtectionProceedButton');
    } else if (this.uiStep === EnableDebuggingState.SETUP) {
      return this.shadowRoot!.querySelector('#password');
    } else if (this.uiStep === EnableDebuggingState.DONE) {
      return this.shadowRoot!.querySelector('#okButton');
    } else if (this.uiStep === EnableDebuggingState.ERROR) {
      return this.shadowRoot!.querySelector('#errorOkButton');
    } else {
      return null;
    }
  }

  /**
   * Cancels the enable debugging screen and drops the user back to the
   * network settings.
   */
  cancel(): void {
    this.userActed('cancel');
  }

  /**
   * Update UI for corresponding state of the screen.
   */
  updateState(state: number): void {
    // Use `state + 1` as index to locate the corresponding EnableDebuggingState
    this.setUIStep(Object.values(EnableDebuggingState)[state + 1]);

    if (this.defaultControl) {
      this.defaultControl.focus();
    }
  }

  private computePasswordsMatch_(password: string, password2: string): boolean {
    return (password.length === 0 && password2.length === 0) ||
        (password === password2 && password.length >= 4);
  }

  private onHelpLinkClicked_(): void {
    this.userActed('learnMore');
  }

  private onRemoveButtonClicked_(): void {
    this.userActed('removeRootFSProtection');
  }

  private onEnableButtonClicked_(): void {
    this.userActed(['setup', this.password_]);
    this.password_ = '';
    this.passwordRepeat_ = '';
  }

  private onOkButtonClicked_(): void {
    this.userActed('done');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EnableDebugging.is]: EnableDebugging;
  }
}

customElements.define(EnableDebugging.is, EnableDebugging);
