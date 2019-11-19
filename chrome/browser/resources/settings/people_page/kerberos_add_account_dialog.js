// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'kerberos-add-account-dialog' is an element to add Kerberos accounts.
 */

Polymer({
  is: 'kerberos-add-account-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * If set, some fields are preset from this account (like username or
     * whether to remember the password).
     * @type {?settings.KerberosAccount}
     */
    presetAccount: Object,

    /**
     * Whether an existing |presetAccount| was successfully authenticated.
     * Always false if |presetAccount| is null (new accounts).
     */
    accountWasRefreshed: {
      type: Boolean,
      value: false,
    },

    /** @private */
    username_: {
      type: String,
      value: '',
    },

    /** @private */
    password_: {
      type: String,
      value: '',
    },

    /**
     * Current configuration in the Advanced Config dialog. Propagates to
     * |config| only if 'Save' button is pressed.
     * @private {string}
     */
    editableConfig_: {
      type: String,
      value: '',
    },

    /** @private */
    rememberPassword_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    generalErrorText_: {
      type: String,
      value: '',
    },

    /** @private */
    usernameErrorText_: {
      type: String,
      value: '',
    },

    /** @private */
    passwordErrorText_: {
      type: String,
      value: '',
    },

    /** @private */
    configErrorText_: {
      type: String,
      value: '',
    },

    /** @private */
    inProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isManaged_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showAdvancedConfig_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    rememberPasswordEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('kerberosRememberPasswordEnabled');
      },
    },
  },

  /** @private {boolean} */
  useRememberedPassword_: false,

  /** @private {string} */
  config_: '',

  /** @private {string} */
  title_: '',

  /** @private {string} */
  actionButtonLabel_: '',

  /** @private {?settings.KerberosAccountsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ =
        settings.KerberosAccountsBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.$.addDialog.showModal();

    if (this.presetAccount) {
      // Refresh an existing account.
      this.title_ = this.i18n('refreshKerberosAccount');
      this.actionButtonLabel_ =
          this.i18n('addKerberosAccountRefreshButtonLabel');

      // Preset username and make UI read-only.
      // Note: At least the focus() part needs to be after showModal.
      this.username_ = this.presetAccount.principalName;
      this.isManaged_ = this.presetAccount.isManaged;
      this.$.username.readonly = true;
      this.$.password.focus();

      if (this.presetAccount.passwordWasRemembered &&
          this.rememberPasswordEnabled_) {
        // The daemon knows the user's password, so prefill the password field
        // with some string (Chrome does not know the actual password for
        // security reasons). If the user does not change it, an empty password
        // is sent to the daemon, which is interpreted as "use remembered
        // password". Also, keep remembering the password by default.
        const FAKE_PASSWORD = 'xxxxxxxx';
        this.password_ = FAKE_PASSWORD;
        this.rememberPassword_ = true;
        this.useRememberedPassword_ = true;
      }

      this.config_ = this.presetAccount.config;
    } else {
      // Add a new Kerberos account.
      this.title_ = this.i18n('addKerberosAccount');
      this.actionButtonLabel_ = this.i18n('add');

      // Set a default configuration.
      this.config_ = loadTimeData.getString('defaultKerberosConfig');
    }
  },

  /** @private */
  onCancel_: function() {
    this.$.addDialog.cancel();
  },

  /** @private */
  onAdd_: function() {
    assert(!this.inProgress_);
    this.inProgress_ = true;

    // Keep the general error, wiping it might cause the error to disappear and
    // immediately reappear, causing 2 resizings of the dialog.
    this.usernameErrorText_ = '';
    this.passwordErrorText_ = '';

    // An empty password triggers the Kerberos daemon to use the remembered one.
    const passwordToSubmit = this.useRememberedPassword_ ? '' : this.password_;

    // For new accounts (no preset), bail if the account already exists.
    const allowExisting = !!this.presetAccount;

    this.browserProxy_
        .addAccount(
            this.username_, passwordToSubmit, this.rememberPassword_,
            this.config_, allowExisting)
        .then(error => {
          this.inProgress_ = false;

          // Success case. Close dialog.
          if (error == settings.KerberosErrorType.kNone) {
            this.accountWasRefreshed = this.presetAccount != null;
            this.$.addDialog.close();
            return;
          }

          // Triggers the UI to update error messages.
          this.updateErrorMessages_(error);
        });
  },

  /** @private */
  onPasswordInput_: function() {
    // On first input, don't reuse the remembered password, but submit the
    // changed one.
    this.useRememberedPassword_ = false;
  },

  /** @private */
  onAdvancedConfigClick_: function() {
    // Keep a copy of the config in case the user cancels.
    this.editableConfig_ = this.config_;
    this.showAdvancedConfig_ = true;
    Polymer.dom.flush();
    this.$$('#advancedConfigDialog').showModal();
  },

  /** @private */
  onAdvancedConfigCancel_: function() {
    this.configErrorText_ = '';
    this.showAdvancedConfig_ = false;
    this.$$('#advancedConfigDialog').cancel();
  },

  /** @private */
  onAdvancedConfigSave_: function() {
    assert(!this.inProgress_);
    this.inProgress_ = true;

    this.browserProxy_.validateConfig(this.editableConfig_).then(result => {
      this.inProgress_ = false;

      // Success case. Close dialog.
      if (result.error == settings.KerberosErrorType.kNone) {
        this.showAdvancedConfig_ = false;
        this.config_ = this.editableConfig_;
        this.configErrorText_ = '';
        this.$$('#advancedConfigDialog').close();
        return;
      }

      // Triggers the UI to update error messages.
      this.updateConfigErrorMessage_(result);
    });
  },

  onAdvancedConfigClose_: function(event) {
    // Note: 'Esc' doesn't trigger onAdvancedConfigCancel_() and some tests
    // that trigger onAdvancedConfigCancel_() don't trigger this for some
    // reason, hence this is needed here and above.
    this.showAdvancedConfig_ = false;

    // Since this is a sub-dialog, prevent event from bubbling up. Otherwise,
    // it might cause the add-dialog to be closed.
    event.stopPropagation();
  },

  /**
   * @param {!settings.KerberosErrorType} error Current error enum
   * @private
   */
  updateErrorMessages_: function(error) {
    this.generalErrorText_ = '';
    this.usernameErrorText_ = '';
    this.passwordErrorText_ = '';

    switch (error) {
      case settings.KerberosErrorType.kNone:
        break;

      case settings.KerberosErrorType.kNetworkProblem:
        this.generalErrorText_ = this.i18n('kerberosErrorNetworkProblem');
        break;
      case settings.KerberosErrorType.kParsePrincipalFailed:
        this.usernameErrorText_ = this.i18n('kerberosErrorUsernameInvalid');
        break;
      case settings.KerberosErrorType.kBadPrincipal:
        this.usernameErrorText_ = this.i18n('kerberosErrorUsernameUnknown');
        break;
      case settings.KerberosErrorType.kDuplicatePrincipalName:
        this.usernameErrorText_ =
            this.i18n('kerberosErrorDuplicatePrincipalName');
        break;
      case settings.KerberosErrorType.kContactingKdcFailed:
        this.usernameErrorText_ = this.i18n('kerberosErrorContactingServer');
        break;

      case settings.KerberosErrorType.kBadPassword:
        this.passwordErrorText_ = this.i18n('kerberosErrorPasswordInvalid');
        break;
      case settings.KerberosErrorType.kPasswordExpired:
        this.passwordErrorText_ = this.i18n('kerberosErrorPasswordExpired');
        break;

      case settings.KerberosErrorType.kKdcDoesNotSupportEncryptionType:
        this.generalErrorText_ = this.i18n('kerberosErrorKdcEncType');
        break;
      default:
        this.generalErrorText_ =
            this.i18n('kerberosErrorGeneral', error.toString());
    }
  },

  /**
   * @param {!settings.ValidateKerberosConfigResult} result Result from a
   *    validateKerberosConfig() call.
   * @private
   */
  updateConfigErrorMessage_: function(result) {
    // There should be an error at this point.
    assert(result.error != settings.KerberosErrorType.kNone);

    // Only handle kBadConfig here. Display generic error otherwise. Should only
    // occur if something is wrong with D-Bus, but nothing user-induced.
    if (result.error != settings.KerberosErrorType.kBadConfig) {
      this.configErrorText_ =
          this.i18n('kerberosErrorGeneral', result.error.toString());
      return;
    }

    let errorLine = '';

    // Don't fall for the classical blunder 0 == false.
    if (result.errorInfo.lineIndex != undefined) {
      const textArea = this.$$('#config').shadowRoot.querySelector('#input');
      errorLine = this.selectAndScrollTo_(textArea, result.errorInfo.lineIndex);
    }

    // If kBadConfig, the error code should be set.
    assert(result.errorInfo.code != settings.KerberosConfigErrorCode.kNone);
    this.configErrorText_ =
        this.getConfigErrorString_(result.errorInfo.code, errorLine);
  },

  /**
   * @param {!settings.KerberosConfigErrorCode} code Error code
   * @param {string} errorLine Line where the error occurred
   * @return {string} Localized error string that corresponds to code
   * @private
   */
  getConfigErrorString_: function(code, errorLine) {
    switch (code) {
      case settings.KerberosConfigErrorCode.kSectionNestedInGroup:
        return this.i18n('kerberosConfigErrorSectionNestedInGroup', errorLine);
      case settings.KerberosConfigErrorCode.kSectionSyntax:
        return this.i18n('kerberosConfigErrorSectionSyntax', errorLine);
      case settings.KerberosConfigErrorCode.kExpectedOpeningCurlyBrace:
        return this.i18n(
            'kerberosConfigErrorExpectedOpeningCurlyBrace', errorLine);
      case settings.KerberosConfigErrorCode.kExtraCurlyBrace:
        return this.i18n('kerberosConfigErrorExtraCurlyBrace', errorLine);
      case settings.KerberosConfigErrorCode.kRelationSyntax:
        return this.i18n('kerberosConfigErrorRelationSyntax', errorLine);
      case settings.KerberosConfigErrorCode.kKeyNotSupported:
        return this.i18n('kerberosConfigErrorKeyNotSupported', errorLine);
      case settings.KerberosConfigErrorCode.kSectionNotSupported:
        return this.i18n('kerberosConfigErrorSectionNotSupported', errorLine);
      case settings.KerberosConfigErrorCode.kKrb5FailedToParse:
        // Note: This error doesn't have an error line.
        return this.i18n('kerberosConfigErrorKrb5FailedToParse');
      default:
        assertNotReached();
    }
  },

  /**
   * Selects a line in a text area and scrolls to it.
   * @param {!Element} textArea A textarea element
   * @param {number} lineIndex 0-based index of the line to select
   * @return {string} The line at lineIndex.
   * @private
   */
  selectAndScrollTo_: function(textArea, lineIndex) {
    const lines = textArea.value.split('\n');
    assert(lineIndex >= 0 && lineIndex < lines.length);

    // Compute selection position in characters.
    let startPos = 0;
    for (let i = 0; i < lineIndex; i++) {
      startPos += lines[i].length + 1;
    }

    // Ignore starting and trailing whitespace for the selection.
    const trimmedLine = lines[lineIndex].trim();
    startPos += lines[lineIndex].indexOf(trimmedLine);
    const endPos = startPos + trimmedLine.length;

    // Set selection.
    textArea.focus();
    textArea.setSelectionRange(startPos, endPos);

    // Scroll to center the selected line.
    const lineHeight = textArea.clientHeight / textArea.rows;
    const firstLine = Math.max(0, lineIndex - textArea.rows / 2);
    textArea.scrollTop = lineHeight * firstLine;

    return lines[lineIndex];
  },

  /**
   * Whether an error element should be shown.
   * Note that !! is not supported in Polymer bindings.
   * @param {?string} errorText Error text to be displayed. Empty if no error.
   * @return {boolean} True iff errorText is not empty.
   * @private
   */
  showError_: function(errorText) {
    return !!errorText;
  }
});