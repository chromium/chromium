// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design offline login.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '../../components/gaia_button.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/dialogs/oobe_content_dialog.js';
import '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';

import type {CrDialogElement} from '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './offline_login.html.js';

const DEFAULT_EMAIL_DOMAIN = '@gmail.com';
const INPUT_EMAIL_PATTERN =
    '^[a-zA-Z0-9.!#$%&\'*+=?^_`\\{\\|\\}~\\-]+(@[^\\s@]+)?$';

enum LoginSection {
  EMAIL = 'emailSection',
  PASSWORD = 'passwordSection',
}

const OfflineLoginBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

interface OfflineLoginScreenData {
  enterpriseDomainManager: string;
  emailDomain: string;
}

export class OfflineLogin extends OfflineLoginBase {
  static get is() {
    return 'offline-login-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      disabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Domain manager.
       */
      manager: {
        type: String,
        value: '',
      },

      /**
       * E-mail domain including initial '@' sign.
       */
      emailDomain: {
        type: String,
        value: '',
      },

      /**
       * |domain| or empty string, depending on |email_| value.
       */
      displayDomain: {
        type: String,
        computed: 'computeDomain(emailDomain, email)',
      },

      /**
       * Current value of e-mail input field.
       */
      email: {
        type: String,
        value: '',
      },

      /**
       * Current value of password input field.
       */
      password: {
        type: String,
        value: '',
      },

      /**
       * Proper e-mail with domain, displayed on password page.
       */
      fullEmail: {
        type: String,
        value: '',
      },

      activeSection: {
        type: String,
        value: LoginSection.EMAIL,
      },

      animationInProgress: {
        type: Boolean,
        value: false,
      },
    };
  }

  private disabled: boolean;
  private manager: string;
  private emailDomain: string;
  private displayDomain: string;
  private email: string;
  private password: string;
  private fullEmail: string;
  private activeSection: string;
  private animationInProgress: boolean;

  override get EXTERNAL_API(): string[] {
    return [
      'reset',
      'proceedToPasswordPage',
      'showOnlineRequiredDialog',
      'showPasswordMismatchMessage',
    ];
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('OfflineLoginScreen');
  }

  override connectedCallback(): void {
    super.connectedCallback();
    const rtl = document.querySelector('html[dir=rtl]') != null;
    if (rtl) {
      this.setAttribute('rtl', '');
    }
  }

  override focus(): void {
    if (this.isEmailSectionActive()) {
      this.shadowRoot!.querySelector<CrInputElement>(
                          '#emailInput')!.focusInput();
    } else {
      this.shadowRoot!.querySelector<CrInputElement>(
                          '#passwordInput')!.focusInput();
    }
  }

  private back(): void {
    this.switchToEmailCard(true /* animated */);
  }

  private cancel(): void {
    if (this.disabled) {
      return;
    }
    this.onBackButtonClicked();
  }

  override onBeforeShow(params: OfflineLoginScreenData): void {
    super.onBeforeShow(params);
    this.reset();
    if ('enterpriseDomainManager' in params) {
      this.manager = params['enterpriseDomainManager'];
    }
    if ('emailDomain' in params) {
      this.emailDomain = '@' + params['emailDomain'];
    }
    this.shadowRoot!.querySelector<CrInputElement>('#emailInput')!.pattern =
        INPUT_EMAIL_PATTERN;
    if (!this.email) {
      this.switchToEmailCard(false /* animated */);
    }
  }

  reset(): void {
    this.animationInProgress = false;
    this.disabled = false;
    this.emailDomain = '';
    this.manager = '';
    this.email = '';
    this.fullEmail = '';
    this.shadowRoot!.querySelector<CrInputElement>('#emailInput')!.invalid =
        false;
    this.shadowRoot!.querySelector<CrInputElement>('#passwordInput')!.invalid =
        false;
    this.activeSection = LoginSection.EMAIL;
  }

  proceedToPasswordPage(): void {
    this.switchToPasswordCard(true /* animated */);
  }

  showOnlineRequiredDialog(): void {
    this.disabled = true;
    this.shadowRoot!.querySelector<CrDialogElement>(
                        '#onlineRequiredDialog')!.showModal();
  }

  private onForgotPasswordClicked(): void {
    this.disabled = true;
    this.shadowRoot!.querySelector<CrDialogElement>(
                        '#forgotPasswordDlg')!.showModal();
  }

  private onForgotPasswordCloseClicked() {
    this.shadowRoot!.querySelector<CrDialogElement>(
                        '#forgotPasswordDlg')!.close();
  }

  private onOnlineRequiredDialogCloseClicked() {
    this.shadowRoot!.querySelector<CrDialogElement>(
                        '#onlineRequiredDialog')!.close();
    this.userActed('cancel');
  }

  private onDialogOverlayClosed(): void {
    this.disabled = false;
  }

  private isEmailSectionActive(): boolean {
    return this.activeSection === LoginSection.EMAIL;
  }

  private switchToEmailCard(animated: boolean): void {
    this.shadowRoot!.querySelector<CrInputElement>('#emailInput')!.invalid =
        false;
    this.shadowRoot!.querySelector<CrInputElement>('#passwordInput')!.invalid =
        false;
    this.password = '';
    if (this.isEmailSectionActive()) {
      return;
    }

    this.animationInProgress = animated;
    this.disabled = animated;
    this.activeSection = LoginSection.EMAIL;
  }

  private switchToPasswordCard(animated: boolean): void {
    if (!this.isEmailSectionActive()) {
      return;
    }

    this.animationInProgress = animated;
    this.disabled = animated;
    this.activeSection = LoginSection.PASSWORD;
  }

  private onSlideAnimationEnd(): void {
    this.animationInProgress = false;
    this.disabled = false;
    this.focus();
  }

  private onEmailSubmitted(): void {
    if (this.shadowRoot!.querySelector<CrInputElement>(
                            '#emailInput')!.validate()) {
      this.fullEmail = this.computeFullEmail(this.email);
      this.userActed(['email-submitted', this.fullEmail]);
    } else {
      this.shadowRoot!.querySelector<CrInputElement>(
                          '#emailInput')!.focusInput();
    }
  }

  private onPasswordSubmitted(): void {
    if (!this.shadowRoot!.querySelector<CrInputElement>(
                             '#passwordInput')!.validate()) {
      return;
    }
    this.email = this.fullEmail;
    this.userActed(['complete-authentication', this.email, this.password]);
    this.disabled = true;
  }

  private onBackButtonClicked(): void {
    if (!this.isEmailSectionActive()) {
      this.switchToEmailCard(true);
    } else {
      this.userActed('cancel');
    }
  }

  private onNextButtonClicked(): void {
    if (this.isEmailSectionActive()) {
      this.onEmailSubmitted();
      return;
    }
    this.onPasswordSubmitted();
  }

  private computeDomain(domain: string, email: string) {
    if (email && email.indexOf('@') !== -1) {
      return '';
    }
    return domain;
  }

  private computeFullEmail(email: string): string {
    if (email.indexOf('@') === -1) {
      if (this.emailDomain) {
        email = email + this.emailDomain;
      } else {
        email = email + DEFAULT_EMAIL_DOMAIN;
      }
    }
    return email;
  }

  showPasswordMismatchMessage(): void {
    this.shadowRoot!.querySelector<CrInputElement>('#passwordInput')!.invalid =
        true;
    this.disabled = false;
    this.shadowRoot!.querySelector<CrInputElement>(
                        '#passwordInput')!.focusInput();
  }

  private setEmailForTest(email: string): void {
    this.email = email;
  }

  private onKeyDown(e: KeyboardEvent): void {
    if (e.keyCode !== 13 || this.disabled) {
      return;
    }
    this.onNextButtonClicked();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OfflineLogin.is]: OfflineLogin;
  }
}

customElements.define(OfflineLogin.is, OfflineLogin);
