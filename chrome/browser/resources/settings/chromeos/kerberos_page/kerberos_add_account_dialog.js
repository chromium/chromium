// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'kerberos-add-account-dialog' is an element to add Kerberos accounts.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../controls/settings_textarea.js';
import '../../settings_shared.css.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

import {KerberosAccount, KerberosAccountsBrowserProxy, KerberosAccountsBrowserProxyImpl, KerberosConfigErrorCode, KerberosErrorType, ValidateKerberosConfigResult} from './kerberos_accounts_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const KerberosAddAccountDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class KerberosAddAccountDialogElement extends
    KerberosAddAccountDialogElementBase {
  static get is() {
    return 'kerberos-add-account-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * If set, some fields are preset from this account (like username or
       * whether to remember the password).
       * @type {?KerberosAccount}
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

      /**
       * Whether the remember password options is allowed by policy.
       * @private {boolean}
       */
      rememberPasswordEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('kerberosRememberPasswordEnabled');
        },
      },

      /**
       * Whether the user is in guest mode.
       * @private {boolean}
       */
      isGuestMode_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {boolean} */
    this.useRememberedPassword_ = false;

    /** @private {string} */
    this.config_ = '';

    /** @private {string} */
    this.title_ = '';

    /** @private {string} */
    this.actionButtonLabel_ = '';

    /** @private {!KerberosAccountsBrowserProxy} */
    this.browserProxy_ = KerberosAccountsBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

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
  }

  /** @private */
  onCancel_() {
    this.$.addDialog.cancel();
  }

  /** @private */
  onAdd_() {
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
          if (error === KerberosErrorType.kNone) {
            this.accountWasRefreshed = this.presetAccount != null;
            this.$.addDialog.close();
            return;
          }

          // Triggers the UI to update error messages.
          this.updateErrorMessages_(error);
        });
    recordSettingChange();
  }

  /** @private */
  onPasswordInput_() {
    // On first input, don't reuse the remembered password, but submit the
    // changed one.
    this.useRememberedPassword_ = false;
  }

  /** @private */
  onAdvancedConfigClick_() {
    // Keep a copy of the config in case the user cancels.
    this.editableConfig_ = this.config_;
    this.showAdvancedConfig_ = true;
    flush();
    this.shadowRoot.querySelector('#advancedConfigDialog').showModal();
  }

  /** @private */
  onAdvancedConfigCancel_() {
    this.configErrorText_ = '';
    this.showAdvancedConfig_ = false;
    this.shadowRoot.querySelector('#advancedConfigDialog').cancel();
  }

  /** @private */
  onAdvancedConfigSave_() {
    assert(!this.inProgress_);
    this.inProgress_ = true;

    this.browserProxy_.validateConfig(this.editableConfig_).then(result => {
      this.inProgress_ = false;

      // Success case. Close dialog.
      if (result.error === KerberosErrorType.kNone) {
        this.showAdvancedConfig_ = false;
        this.config_ = this.editableConfig_;
        this.configErrorText_ = '';
        this.shadowRoot.querySelector('#advancedConfigDialog').close();
        return;
      }

      // Triggers the UI to update error messages.
      this.updateConfigErrorMessage_(result);
    });
    recordSettingChange();
  }

  onAdvancedConfigClose_(event) {
    // Note: 'Esc' doesn't trigger onAdvancedConfigCancel_() and some tests
    // that trigger onAdvancedConfigCancel_() don't trigger this for some
    // reason, hence this is needed here and above.
    this.showAdvancedConfig_ = false;

    // Since this is a sub-dialog, prevent event from bubbling up. Otherwise,
    // it might cause the add-dialog to be closed.
    event.stopPropagation();
  }

  /**
   * @param {!KerberosErrorType} error Current error enum
   * @private
   */
  updateErrorMessages_(error) {
    this.generalErrorText_ = '';
    this.usernameErrorText_ = '';
    this.passwordErrorText_ = '';

    switch (error) {
      case KerberosErrorType.kNone:
        break;

      case KerberosErrorType.kNetworkProblem:
        this.generalErrorText_ = this.i18n('kerberosErrorNetworkProblem');
        break;
      case KerberosErrorType.kParsePrincipalFailed:
        this.usernameErrorText_ = this.i18n('kerberosErrorUsernameInvalid');
        break;
      case KerberosErrorType.kBadPrincipal:
        this.usernameErrorText_ = this.i18n('kerberosErrorUsernameUnknown');
        break;
      case KerberosErrorType.kDuplicatePrincipalName:
        this.usernameErrorText_ =
            this.i18n('kerberosErrorDuplicatePrincipalName');
        break;
      case KerberosErrorType.kContactingKdcFailed:
        this.usernameErrorText_ = this.i18n('kerberosErrorContactingServer');
        break;

      case KerberosErrorType.kBadPassword:
        this.passwordErrorText_ = this.i18n('kerberosErrorPasswordInvalid');
        break;
      case KerberosErrorType.kPasswordExpired:
        this.passwordErrorText_ = this.i18n('kerberosErrorPasswordExpired');
        break;

      case KerberosErrorType.kKdcDoesNotSupportEncryptionType:
        this.generalErrorText_ = this.i18n('kerberosErrorKdcEncType');
        break;
      default:
        this.generalErrorText_ =
            this.i18n('kerberosErrorGeneral', error.toString());
    }
  }

  /**
   * @param {!ValidateKerberosConfigResult} result Result from a
   *    validateKerberosConfig() call.
   * @private
   */
  updateConfigErrorMessage_(result) {
    // There should be an error at this point.
    assert(result.error !== KerberosErrorType.kNone);

    // Only handle kBadConfig here. Display generic error otherwise. Should only
    // occur if something is wrong with D-Bus, but nothing user-induced.
    if (result.error !== KerberosErrorType.kBadConfig) {
      this.configErrorText_ =
          this.i18n('kerberosErrorGeneral', result.error.toString());
      return;
    }

    let errorLine = '';

    // Don't fall for the classical blunder 0 == false.
    if (result.errorInfo.lineIndex !== undefined) {
      const textArea =
          this.shadowRoot.querySelector('#config').shadowRoot.querySelector(
              '#input');
      errorLine = this.selectAndScrollTo_(textArea, result.errorInfo.lineIndex);
    }

    // If kBadConfig, the error code should be set.
    assert(result.errorInfo.code !== KerberosConfigErrorCode.kNone);
    this.configErrorText_ =
        this.getConfigErrorString_(result.errorInfo.code, errorLine);
  }

  /**
   * @param {!KerberosConfigErrorCode} code Error code
   * @param {string} errorLine Line where the error occurred
   * @return {string} Localized error string that corresponds to code
   * @private
   */
  getConfigErrorString_(code, errorLine) {
    switch (code) {
      case KerberosConfigErrorCode.kSectionNestedInGroup:
        return this.i18n('kerberosConfigErrorSectionNestedInGroup', errorLine);
      case KerberosConfigErrorCode.kSectionSyntax:
        return this.i18n('kerberosConfigErrorSectionSyntax', errorLine);
      case KerberosConfigErrorCode.kExpectedOpeningCurlyBrace:
        return this.i18n(
            'kerberosConfigErrorExpectedOpeningCurlyBrace', errorLine);
      case KerberosConfigErrorCode.kExtraCurlyBrace:
        return this.i18n('kerberosConfigErrorExtraCurlyBrace', errorLine);
      case KerberosConfigErrorCode.kRelationSyntax:
        return this.i18n('kerberosConfigErrorRelationSyntax', errorLine);
      case KerberosConfigErrorCode.kKeyNotSupported:
        return this.i18n('kerberosConfigErrorKeyNotSupported', errorLine);
      case KerberosConfigErrorCode.kSectionNotSupported:
        return this.i18n('kerberosConfigErrorSectionNotSupported', errorLine);
      case KerberosConfigErrorCode.kKrb5FailedToParse:
        // Note: This error doesn't have an error line.
        return this.i18n('kerberosConfigErrorKrb5FailedToParse');
      case KerberosConfigErrorCode.kTooManyNestedGroups:
        return this.i18n('kerberosConfigErrorTooManyNestedGroups', errorLine);
      default:
        assertNotReached();
    }
  }

  /**
   * Selects a line in a text area and scrolls to it.
   * @param {!Element} textArea A textarea element
   * @param {number} lineIndex 0-based index of the line to select
   * @return {string} The line at lineIndex.
   * @private
   */
  selectAndScrollTo_(textArea, lineIndex) {
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
  }

  /**
   * Whether an error element should be shown.
   * Note that !! is not supported in Polymer bindings.
   * @param {?string} errorText Error text to be displayed. Empty if no error.
   * @return {boolean} True iff errorText is not empty.
   * @private
   */
  showError_(errorText) {
    return !!errorText;
  }
}

customElements.define(
    KerberosAddAccountDialogElement.is, KerberosAddAccountDialogElement);
