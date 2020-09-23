// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  const DEFAULT_EMAIL_DOMAIN = '@gmail.com';
  const INPUT_EMAIL_PATTERN = '^[a-zA-Z0-9.!#$%&\'*+=?^_`{|}~-]+(@[^\\s@]+)?$';

  /** @enum */
  const TRANSITION_TYPE = {FORWARD: 0, BACKWARD: 1, NONE: 2};

  Polymer({
    is: 'offline-gaia',

    behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

    properties: {
      disabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Management domain.
       * @type {?string}
       */
      domain: {
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
      this.domain = '';
      this.email_ = '';
      this.fullEmail_ = '';
      this.$.emailInput.isInvalid = false;
      this.$.passwordInput.isInvalid = false;
    },

    onForgotPasswordClicked_() {
      this.disabled = true;
      this.fire('dialogShown');
      this.$.forgotPasswordDlg.showModal();
    },

    onForgotPasswordCloseTap_() {
      this.$.forgotPasswordDlg.close();
    },

    onDialogOverlayClosed_() {
      this.fire('dialogHidden');
      this.disabled = false;
    },

    setEmail(email) {
      if (email) {
        if (this.emailDomain)
          email = email.replace(this.emailDomain, '');
        this.switchToPasswordCard(email, false /* animated */);
        this.$.passwordInput.isInvalid = true;
        this.fire('backButton', true);
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

    switchToEmailCard(animated) {
      this.$.emailInput.isInvalid = false;
      this.$.passwordInput.isInvalid = false;
      this.password_ = '';
      if (this.isEmailSectionActive_())
        return;

      this.animationInProgress = animated;
      this.activeSection = 'emailSection';
    },

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
        this.switchToPasswordCard(this.email_, true /* animated */);
      } else {
        this.$.emailInput.focusInput();
      }
    },

    onPasswordSubmitted_() {
      if (!this.$.passwordInput.validate())
        return;
      var msg = {
        'useOffline': true,
        'email': this.fullEmail_,
        'password': this.password_,
      };
      this.password_ = '';
      this.fire('authCompleted', msg);
    },

    onBackButtonClicked_() {
      if (!this.isEmailSectionActive_()) {
        this.switchToEmailCard(true);
      } else {
        this.fire('offline-gaia-cancel');
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
}
