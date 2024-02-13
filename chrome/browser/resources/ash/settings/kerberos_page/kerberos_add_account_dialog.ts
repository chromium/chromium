// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'kerberos-add-account-dialog' is an element to add Kerberos accounts.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_textarea/cr_textarea.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {CrTextareaElement} from 'chrome://resources/ash/common/cr_elements/cr_textarea/cr_textarea.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

import {KerberosAccount, KerberosAccountsBrowserProxy, KerberosAccountsBrowserProxyImpl, KerberosConfigErrorCode, KerberosErrorType, ValidateKerberosConfigResult} from './kerberos_accounts_browser_proxy.js';
import {getTemplate} from './kerberos_add_account_dialog.html.js';

export interface KerberosAddAccountDialogElement {
  $: {
    addDialog: CrDialogElement,
    username: CrInputElement,
    password: CrInputElement,
  };
}

/**
 * The default placeholder that is shown in the username field of the
 * authentication dialog.
 */
const DEFAULT_USERNAME_PLACEHOLDER = 'user@example.com';

const KerberosAddAccountDialogElementBase = I18nMixin(PolymerElement);

export class KerberosAddAccountDialogElement extends
    KerberosAddAccountDialogElementBase {
  static get is() {
    return 'kerberos-add-account-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * If set, some fields are preset from this account (like username or
       * whether to remember the password).
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

      username_: {
        type: String,
        value: '',
      },

      password_: {
        type: String,
        value: '',
      },

      /**
       * Current configuration in the Advanced Config dialog. Propagates to
       * |config| only if 'Save' button is pressed.
       */
      editableConfig_: {
        type: String,
        value: '',
      },

      generalErrorText_: {
        type: String,
        value: '',
      },

      usernameErrorText_: {
        type: String,
        value: '',
      },

      passwordErrorText_: {
        type: String,
        value: '',
      },

      configErrorText_: {
        type: String,
        value: '',
      },

      inProgress_: {
        type: Boolean,
        value: false,
      },

      isManaged_: {
        type: Boolean,
        value: false,
      },

      showAdvancedConfig_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the password should be remembered by default.
       */
      rememberPasswordByDefault_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('kerberosRememberPasswordByDefault');
        },
      },

      /**
       * Whether the remember password option is allowed by policy.
       */
      rememberPasswordEnabledByPolicy_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('kerberosRememberPasswordEnabled');
        },
      },

      /**
       * Prefilled domain for the new tickets if kerberosDomainAutocomplete
       * policy is enabled. Empty by default. Starts with '@' if it is
       * not empty.
       */
      prefillDomain_: {
        type: String,
        value: '',
      },

      /**
       * Whether the user is in guest mode.
       */
      isGuestMode_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
      },
    };
  }

  accountWasRefreshed: boolean;
  presetAccount: KerberosAccount|null;

  private actionButtonLabel_: string;
  private browserProxy_: KerberosAccountsBrowserProxy;
  private configErrorText_: string;
  private config_: string;
  private editableConfig_: string;
  private generalErrorText_: string;
  private inProgress_: boolean;
  private isGuestMode_: boolean;
  private isManaged_: boolean;
  private passwordErrorText_: string;
  private password_: string;
  private prefillDomain_: string;
  private rememberPasswordByDefault_: boolean;
  private rememberPasswordEnabledByPolicy_: boolean;
  private rememberPasswordChecked_: boolean;
  private showAdvancedConfig_: boolean;
  private title_: string;
  private useStoredPassword_: boolean;
  private usernameErrorText_: string;
  private username_: string;

  constructor() {
    super();

    this.useStoredPassword_ = false;
    this.rememberPasswordChecked_ = this.rememberPasswordByDefault_ &&
        this.rememberPasswordEnabledByPolicy_ && !this.isGuestMode_;
    this.config_ = '';
    this.title_ = '';
    this.actionButtonLabel_ = '';
    this.browserProxy_ = KerberosAccountsBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
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
          this.rememberPasswordEnabledByPolicy_) {
        // The daemon knows the user's password, so prefill the password field
        // with some string (Chrome does not know the actual password for
        // security reasons). If the user does not change it, an empty password
        // is sent to the daemon, which is interpreted as "use stored password".
        // Also, keep remembering the password by default.
        const FAKE_PASSWORD = 'xxxxxxxx';
        this.password_ = FAKE_PASSWORD;
        this.rememberPasswordChecked_ = true;
        this.useStoredPassword_ = true;
      }

      this.config_ = this.presetAccount!.config;
    } else {
      // Add a new Kerberos account.
      this.title_ = this.i18n('addKerberosAccount');
      this.actionButtonLabel_ = this.i18n('add');

      // Get the prefill domain and add '@' to it if it's not empty.
      // Also the domain is considered invalid if it already has '@' in it.
      const domain = loadTimeData.getString('kerberosDomainAutocomplete');
      if (domain && domain.indexOf('@') === -1) {
        this.prefillDomain_ = '@' + domain;
      }

      // Set a default configuration.
      this.config_ = loadTimeData.getString('defaultKerberosConfig');
    }
  }

  private onCancel_(): void {
    this.$.addDialog.cancel();
  }

  private onAdd_(): void {
    assert(!this.inProgress_);
    this.inProgress_ = true;

    // Keep the general error, wiping it might cause the error to disappear and
    // immediately reappear, causing 2 resizings of the dialog.
    this.usernameErrorText_ = '';
    this.passwordErrorText_ = '';

    // An empty password triggers the Kerberos daemon to use the stored one.
    const passwordToSubmit = this.useStoredPassword_ ? '' : this.password_;

    // For new accounts (no preset), bail if the account already exists.
    const allowExisting = !!this.presetAccount;

    this.browserProxy_
        .addAccount(
            this.computeUsername_(this.username_, this.prefillDomain_),
            passwordToSubmit, this.rememberPasswordChecked_, this.config_,
            allowExisting)
        .then(error => {
          this.inProgress_ = false;

          // Success case. Close dialog.
          if (error === KerberosErrorType.NONE) {
            this.accountWasRefreshed = this.presetAccount != null;
            this.$.addDialog.close();
            recordSettingChange(Setting.kAddKerberosTicketV2);
            return;
          }

          // Triggers the UI to update error messages.
          this.updateErrorMessages_(error);
        });
  }

  private onPasswordInput_(): void {
    // On first input, don't reuse the stored password, but submit the changed
    // one.
    this.useStoredPassword_ = false;
  }

  private getAdvancedConfigDialog(): CrDialogElement {
    return castExists(this.shadowRoot!.querySelector<CrDialogElement>(
        '#advancedConfigDialog'));
  }

  private onAdvancedConfigClick_(): void {
    this.browserProxy_.validateConfig(this.config_).then(result => {
      // Success case.
      if (result.error === KerberosErrorType.NONE) {
        this.configErrorText_ = '';
        return;
      }

      // Triggers the UI to update error messages.
      this.updateConfigErrorMessage_(result);
    });

    // Keep a copy of the config in case the user cancels.
    this.editableConfig_ = this.config_;
    this.showAdvancedConfig_ = true;
    flush();
    this.getAdvancedConfigDialog().showModal();
  }

  private onAdvancedConfigCancel_(): void {
    this.configErrorText_ = '';
    this.showAdvancedConfig_ = false;
    this.getAdvancedConfigDialog().cancel();
  }

  private onAdvancedConfigSave_(): void {
    assert(!this.inProgress_);
    this.inProgress_ = true;

    this.browserProxy_.validateConfig(this.editableConfig_).then(result => {
      this.inProgress_ = false;

      // Success case. Close dialog.
      if (result.error === KerberosErrorType.NONE) {
        this.showAdvancedConfig_ = false;
        this.config_ = this.editableConfig_;
        this.configErrorText_ = '';
        this.getAdvancedConfigDialog().close();
        return;
      }

      // Triggers the UI to update error messages.
      this.updateConfigErrorMessage_(result);
    });
  }

  private onAdvancedConfigClose_(event: Event): void {
    // Note: 'Esc' doesn't trigger onAdvancedConfigCancel_() and some tests
    // that trigger onAdvancedConfigCancel_() don't trigger this for some
    // reason, hence this is needed here and above.
    this.showAdvancedConfig_ = false;

    // Since this is a sub-dialog, prevent event from bubbling up. Otherwise,
    // it might cause the add-dialog to be closed.
    event.stopPropagation();
  }

  private updateErrorMessages_(error: KerberosErrorType): void {
    this.generalErrorText_ = '';
    this.usernameErrorText_ = '';
    this.passwordErrorText_ = '';

    switch (error) {
      case KerberosErrorType.NONE:
        break;

      case KerberosErrorType.NETWORK_PROBLEM:
        this.generalErrorText_ = this.i18n('kerberosErrorNetworkProblem');
        break;
      case KerberosErrorType.PARSE_PRINCIPAL_FAILED:
        this.usernameErrorText_ = this.i18n('kerberosErrorUsernameInvalid');
        break;
      case KerberosErrorType.BAD_PRINCIPAL:
        this.usernameErrorText_ = this.i18n('kerberosErrorUsernameUnknown');
        break;
      case KerberosErrorType.DUPLICATE_PRINCIPAL_NAME:
        this.usernameErrorText_ =
            this.i18n('kerberosErrorDuplicatePrincipalName');
        break;
      case KerberosErrorType.CONTACTING_KDC_FAILED:
        this.usernameErrorText_ = this.i18n('kerberosErrorContactingServer');
        break;

      case KerberosErrorType.BAD_PASSWORD:
        this.passwordErrorText_ = this.i18n('kerberosErrorPasswordInvalid');
        break;
      case KerberosErrorType.PASSWORD_EXPIRED:
        this.passwordErrorText_ = this.i18n('kerberosErrorPasswordExpired');
        break;

      case KerberosErrorType.KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE:
        this.generalErrorText_ = this.i18n('kerberosErrorKdcEncType');
        break;
      default:
        this.generalErrorText_ =
            this.i18n('kerberosErrorGeneral', error.toString());
    }
  }

  /**
   * @param result Result from a validateKerberosConfig() call.
   */
  private updateConfigErrorMessage_(result: ValidateKerberosConfigResult):
      void {
    // There should be an error at this point.
    assert(result.error !== KerberosErrorType.NONE);

    // Only handle kBadConfig here. Display generic error otherwise. Should only
    // occur if something is wrong with D-Bus, but nothing user-induced.
    if (result.error !== KerberosErrorType.BAD_CONFIG) {
      this.configErrorText_ =
          this.i18n('kerberosErrorGeneral', result.error.toString());
      return;
    }

    let errorLine = '';

    // Don't fall for the classical blunder 0 == false.
    if (result.errorInfo.lineIndex !== undefined) {
      const textArea = castExists(
          this.shadowRoot!.querySelector<CrTextareaElement>('#config')!
              .shadowRoot!.querySelector<HTMLTextAreaElement>('#input'));
      errorLine = this.selectAndScrollTo_(textArea, result.errorInfo.lineIndex);
    }

    // If kBadConfig, the error code should be set.
    assert(result.errorInfo.code !== KerberosConfigErrorCode.NONE);
    this.configErrorText_ =
        this.getConfigErrorString_(result.errorInfo.code, errorLine);
  }

  /**
   * @param code Error code
   * @param errorLine Line where the error occurred
   * @return Localized error string that corresponds to code
   */
  private getConfigErrorString_(
      code: KerberosConfigErrorCode, errorLine: string): string {
    switch (code) {
      case KerberosConfigErrorCode.SECTION_NESTED_IN_GROUP:
        return this.i18n('kerberosConfigErrorSectionNestedInGroup', errorLine);
      case KerberosConfigErrorCode.SECTION_SYNTAX:
        return this.i18n('kerberosConfigErrorSectionSyntax', errorLine);
      case KerberosConfigErrorCode.EXPECTED_OPENING_CURLY_BRACE:
        return this.i18n(
            'kerberosConfigErrorExpectedOpeningCurlyBrace', errorLine);
      case KerberosConfigErrorCode.EXTRA_CURLY_BRACE:
        return this.i18n('kerberosConfigErrorExtraCurlyBrace', errorLine);
      case KerberosConfigErrorCode.RELATION_SYNTAX:
        return this.i18n('kerberosConfigErrorRelationSyntax', errorLine);
      case KerberosConfigErrorCode.KEY_NOT_SUPPORTED:
        return this.i18n('kerberosConfigErrorKeyNotSupported', errorLine);
      case KerberosConfigErrorCode.SECTION_NOT_SUPPORTED:
        return this.i18n('kerberosConfigErrorSectionNotSupported', errorLine);
      case KerberosConfigErrorCode.KRB5_FAILED_TO_PARSE:
        // Note: This error doesn't have an error line.
        return this.i18n('kerberosConfigErrorKrb5FailedToParse');
      case KerberosConfigErrorCode.TOO_MANY_NESTED_GROUPS:
        return this.i18n('kerberosConfigErrorTooManyNestedGroups', errorLine);
      case KerberosConfigErrorCode.LINE_TOO_LONG:
        return this.i18n('kerberosConfigErrorLineTooLong', errorLine);
      default:
        assertNotReached();
    }
  }

  /**
   * Selects a line in a text area and scrolls to it.
   * @param textArea A textarea element
   * @param lineIndex 0-based index of the line to select
   * @return The line at lineIndex.
   */
  private selectAndScrollTo_(textArea: HTMLTextAreaElement, lineIndex: number):
      string {
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
   * @param errorText Error text to be displayed. Empty if no error.
   * @return True iff errorText is not empty.
   */
  private showError_(errorText: string): boolean {
    return !!errorText;
  }

  /**
   * Prefilled domain is not shown if the username contains '@',
   * giving the user an opportunity to use some other domain.
   * @param {string} username The username typed by the user.
   * @param {string} domain Prefilled domain, prefixed with '@'.
   * @return {string}
   */
  private computeDomain_(username: string, domain: string): string {
    if (username && username.indexOf('@') !== -1) {
      return '';
    }
    return domain;
  }

  /**
   * Return username if it contains '@', otherwise append prefilled
   * domain (which could be empty) to the current username.
   * @param username The username typed by the user.
   * @param domain Prefilled domain, prefixed with '@'.
   */
  private computeUsername_(username: string, domain: string): string {
    if (username && username.indexOf('@') === -1) {
      return username + domain;
    }
    return username;
  }

  /**
   * If prefilled domain is present return an empty string,
   * otherwise show the default placeholder.
   * @param prefillDomain Prefilled domain, prefixed with '@'.
   */
  private computePlaceholder_(prefillDomain: string): string {
    if (prefillDomain) {
      return '';
    }
    return DEFAULT_USERNAME_PLACEHOLDER;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'kerberos-add-account-dialog': KerberosAddAccountDialogElement;
  }
}

customElements.define(
    KerberosAddAccountDialogElement.is, KerberosAddAccountDialogElement);
