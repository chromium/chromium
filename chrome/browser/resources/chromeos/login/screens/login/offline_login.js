// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

const DEFAULT_EMAIL_DOMAIN = '@gmail.com';
const INPUT_EMAIL_PATTERN = '^[a-zA-Z0-9.!#$%&\'*+=?^_`{|}~-]+(@[^\\s@]+)?$';

Polymer({
  is: 'offline-login-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: [
    'loadParams', 'reset', 'proceedToPasswordPage', 'showOnlineRequiredDialog'
  ],

  properties: {
    disabled: {
      type: Boolean,
      value: false,
    },

    /**
     * Domain manager.
     * @type {?string}
     */
    manager: {
      type: String,
      value: '',
    },

    /**
     * E-mail domain including initial '@' sign.
     * @type {?string}
     */
    emailDomain: {
      type: String,
      value: '',
    },

    /**
     * |domain| or empty string, depending on |email_| value.
     */
    displayDomain_: {
      type: String,
      computed: 'computeDomain_(emailDomain, email_)',
    },

    /**
     * Current value of e-mail input field.
     */
    email_: String,

    /**
     * Current value of password input field.
     */
    password_: String,

    /**
     * Proper e-mail with domain, displayed on password page.
     */
    fullEmail_: String,

    activeSection: {
      type: String,
      value: 'emailSection',
    },

    animationInProgress: Boolean,
  },

  /** @override */
  ready() {
    this.initializeLoginScreen('OfflineLoginScreen', {
      resetAllowed: true,
    });
  },

  attached() {
    if (this.isRTL_())
      this.setAttribute('rtl', '');
  },

  focus() {
    if (this.isEmailSectionActive_()) {
      this.$.emailInput.focusInput();
    } else {
      this.$.passwordInput.focusInput();
    }
  },

  back() {
    this.switchToEmailCard(true /* animated */);
  },

  onBeforeShow() {
    cr.ui.login.invokePolymerMethod(this.$.dialog, 'onBeforeShow');
    this.$.emailInput.pattern = INPUT_EMAIL_PATTERN;
  },

  reset() {
    this.disabled = false;
    this.emailDomain = '';
    this.manager = '';
    this.email_ = '';
    this.fullEmail_ = '';
    this.$.emailInput.invalid = false;
    this.$.passwordInput.invalid = false;
  },

  /**
   * @param {!Object} params parameters bag.
   */
  loadParams(params) {
    this.reset();
    if ('enterpriseDomainManager' in params)
      this.manager = params['enterpriseDomainManager'];
    if ('emailDomain' in params)
      this.emailDomain = '@' + params['emailDomain'];
    this.setEmail(params.email);
  },

  proceedToPasswordPage() {
    this.switchToPasswordCard(this.email_, true /* animated */);
  },

  showOnlineRequiredDialog() {
    this.disabled = true;
    this.$.onlineRequiredDialog.showModal();
  },

  onForgotPasswordClicked_() {
    this.disabled = true;
    this.$.forgotPasswordDlg.showModal();
  },

  onForgotPasswordCloseTap_() {
    this.$.forgotPasswordDlg.close();
  },

  onOnlineRequiredDialogCloseTap_() {
    this.$.onlineRequiredDialog.close();
    this.userActed('cancel');
  },

  onDialogOverlayClosed_() {
    this.disabled = false;
  },

  /**
   * @param {string} email
   */
  setEmail(email) {
    if (email) {
      if (this.emailDomain)
        email = email.replace(this.emailDomain, '');
      this.switchToPasswordCard(email, false /* animated */);
      this.$.passwordInput.invalid = true;
    } else {
      this.email_ = '';
      this.switchToEmailCard(false /* animated */);
    }
  },

  isRTL_() {
    return !!document.querySelector('html[dir=rtl]');
  },

  isEmailSectionActive_() {
    return this.activeSection == 'emailSection';
  },

  /**
   * @param {boolean} animated
   */
  switchToEmailCard(animated) {
    this.$.emailInput.invalid = false;
    this.$.passwordInput.invalid = false;
    this.password_ = '';
    if (this.isEmailSectionActive_())
      return;

    this.animationInProgress = animated;
    this.activeSection = 'emailSection';
  },

  /**
   * @param {string} email
   * @param {boolean} animated
   */
  switchToPasswordCard(email, animated) {
    this.email_ = email;
    if (email.indexOf('@') === -1) {
      if (this.emailDomain)
        email = email + this.emailDomain;
      else
        email = email + DEFAULT_EMAIL_DOMAIN;
    }
    this.fullEmail_ = email;

    if (!this.isEmailSectionActive_())
      return;

    this.animationInProgress = animated;
    this.activeSection = 'passwordSection';
  },

  onSlideAnimationEnd_() {
    this.animationInProgress = false;
    this.focus();
  },

  onEmailSubmitted_() {
    if (this.$.emailInput.validate()) {
      chrome.send('OfflineLogin.onEmailSubmitted', [this.email_]);
    } else {
      this.$.emailInput.focusInput();
    }
  },

  onPasswordSubmitted_() {
    if (!this.$.passwordInput.validate())
      return;
    this.email_ = this.fullEmail_;
    chrome.send('completeOfflineAuthentication', [this.email_, this.password_]);
    this.password_ = '';
  },

  onBackButtonClicked_() {
    if (!this.isEmailSectionActive_()) {
      this.switchToEmailCard(true);
    } else {
      this.userActed('cancel');
    }
  },

  onNextButtonClicked_() {
    if (this.isEmailSectionActive_()) {
      this.onEmailSubmitted_();
      return;
    }
    this.onPasswordSubmitted_();
  },

  computeDomain_(domain, email) {
    if (email && email.indexOf('@') !== -1)
      return '';
    return domain;
  },
});
})();
