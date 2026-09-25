// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-credential-management-dialog' is a
 * dialog for viewing and erasing credentials stored on a security key.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../../site_favicon.js';
import '../../i18n_setup.js';
import './security_keys_pin_field.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Credential, SecurityKeysCredentialBrowserProxy, StartCredentialManagementResponse} from './security_keys_browser_proxy.js';
import {SecurityKeysCredentialBrowserProxyImpl} from './security_keys_browser_proxy.js';
import {getCss} from './security_keys_credential_management_dialog.css.js';
import {getHtml} from './security_keys_credential_management_dialog.html.js';
import type {SettingsSecurityKeysPinFieldElement} from './security_keys_pin_field.js';

export enum CredentialManagementDialogPage {
  INITIAL = 'initial',
  PIN_PROMPT = 'pinPrompt',
  PIN_ERROR = 'pinError',
  CREDENTIALS = 'credentials',
  EDIT = 'edit',
  ERROR = 'error',
  CONFIRM = 'confirm',
}

export interface SettingsSecurityKeysCredentialManagementDialogElement {
  $: {
    cancelButton: CrButtonElement,
    confirm: HTMLElement,
    confirmButton: CrButtonElement,
    container: HTMLElement,
    dialog: CrDialogElement,
    displayNameInput: CrInputElement,
    edit: HTMLElement,
    error: HTMLElement,
    pin: SettingsSecurityKeysPinFieldElement,
    pinError: HTMLElement,
    userNameInput: CrInputElement,
  };
}

const SettingsSecurityKeysCredentialManagementDialogElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

const MAX_INPUT_LENGTH: number = 62;

export class SettingsSecurityKeysCredentialManagementDialogElement extends
    SettingsSecurityKeysCredentialManagementDialogElementBase {
  static get is() {
    return 'settings-security-keys-credential-management-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The ID of the element currently shown in the dialog.
       */
      dialogPage_: {type: String},

      dialogTitle_: {type: String},

      /**
       * The list of credentials displayed in the dialog.
       */
      credentials_: {type: Array},

      /**
       * The message displayed on the "error" dialog page.
       */
      errorMsg_: {type: String},

      cancelButtonVisible_: {type: Boolean},
      closeButtonVisible_: {type: Boolean},
      confirmButtonDisabled_: {type: Boolean},
      confirmButtonLabel_: {type: String},
      confirmButtonVisible_: {type: Boolean},
      confirmMsg_: {type: String},
      displayNameInputError_: {type: String},
      editButtonVisible_: {type: Boolean},
      minPinLength_: {type: Number},
      newDisplayName_: {type: String},
      newUsername_: {type: String},
      userNameInputError_: {type: String},
    };
  }

  protected accessor dialogPage_: CredentialManagementDialogPage =
      CredentialManagementDialogPage.INITIAL;
  protected accessor dialogTitle_: string = '';
  protected accessor credentials_: Credential[] = [];
  protected accessor errorMsg_: string = '';
  protected accessor cancelButtonVisible_: boolean = true;
  protected accessor closeButtonVisible_: boolean = false;
  protected accessor confirmButtonDisabled_: boolean = false;
  protected accessor confirmButtonLabel_: string = '';
  protected accessor confirmButtonVisible_: boolean = false;
  protected accessor confirmMsg_: string = '';
  protected accessor displayNameInputError_: string = '';
  protected accessor editButtonVisible_: boolean = false;
  protected accessor minPinLength_: number = 0;
  protected accessor newDisplayName_: string = '';
  protected accessor newUsername_: string = '';
  protected accessor userNameInputError_: string = '';

  private browserProxy_: SecurityKeysCredentialBrowserProxy =
      SecurityKeysCredentialBrowserProxyImpl.getInstance();
  private credentialIdToDelete_: string = '';
  private editingCredential_: Credential|null = null;
  private showSetPINButton_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
    this.addWebUiListener(
        'security-keys-credential-management-finished',
        (error: string, requiresPINChange = false) =>
            this.onPinError_(error, requiresPINChange));
    this.browserProxy_.startCredentialManagement().then(
        (response: StartCredentialManagementResponse) => {
          this.minPinLength_ = response.minPinLength;
          this.editButtonVisible_ = response.supportsUpdateUserInformation;
          this.dialogPage_ = CredentialManagementDialogPage.PIN_PROMPT;
        });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('dialogPage_')) {
      this.updateDialogPageState_();
    }
    if (changedPrivateProperties.has('newDisplayName_') ||
        changedPrivateProperties.has('newUsername_')) {
      this.validateInput_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('dialogPage_')) {
      if (this.dialogPage_ === CredentialManagementDialogPage.PIN_PROMPT) {
        this.$.pin.focus();
      }
      this.fire('credential-management-dialog-ready-for-testing');
    }
  }

  private onPinError_(error: string, requiresPINChange = false) {
    this.errorMsg_ = error;
    this.showSetPINButton_ = requiresPINChange;
    this.dialogPage_ = CredentialManagementDialogPage.PIN_ERROR;
  }

  private onError_(error: string) {
    this.errorMsg_ = error;
    this.dialogPage_ = CredentialManagementDialogPage.ERROR;
  }

  private submitPin_() {
    // Disable the confirm button to prevent concurrent submissions.
    this.confirmButtonDisabled_ = true;

    this.$.pin.trySubmit(pin => this.browserProxy_.providePin(pin))
        .then(
            () => {
              // Leave confirm button disabled while enumerating credentials.
              this.browserProxy_.enumerateCredentials().then(
                  (credentials: Credential[]) =>
                      this.onCredentials_(credentials));
            },
            () => {
              // Wrong PIN.
              this.confirmButtonDisabled_ = false;
            });
  }

  private onCredentials_(credentials: Credential[]) {
    this.credentials_ = credentials;
    this.dialogPage_ = CredentialManagementDialogPage.CREDENTIALS;
  }

  private updateDialogPageState_() {
    switch (this.dialogPage_) {
      case CredentialManagementDialogPage.INITIAL:
        this.cancelButtonVisible_ = true;
        this.confirmButtonVisible_ = false;
        this.closeButtonVisible_ = false;
        this.dialogTitle_ =
            this.i18n('securityKeysCredentialManagementDialogTitle');
        break;
      case CredentialManagementDialogPage.PIN_PROMPT:
        this.cancelButtonVisible_ = false;
        this.confirmButtonLabel_ = this.i18n('continue');
        this.confirmButtonDisabled_ = false;
        this.confirmButtonVisible_ = true;
        this.closeButtonVisible_ = false;
        this.dialogTitle_ =
            this.i18n('securityKeysCredentialManagementDialogTitle');
        break;
      case CredentialManagementDialogPage.PIN_ERROR:
        this.cancelButtonVisible_ = true;
        this.confirmButtonLabel_ = this.i18n('securityKeysSetPinButton');
        this.confirmButtonVisible_ = this.showSetPINButton_;
        this.confirmButtonDisabled_ = false;
        this.closeButtonVisible_ = false;
        this.dialogTitle_ =
            this.i18n('securityKeysCredentialManagementDialogTitle');
        break;
      case CredentialManagementDialogPage.CREDENTIALS:
        this.cancelButtonVisible_ = false;
        this.confirmButtonLabel_ = this.i18n('done');
        this.confirmButtonDisabled_ = false;
        this.confirmButtonVisible_ = true;
        this.closeButtonVisible_ = false;
        this.dialogTitle_ =
            this.i18n('securityKeysCredentialManagementDialogTitle');
        break;
      case CredentialManagementDialogPage.EDIT:
        this.cancelButtonVisible_ = true;
        this.confirmButtonLabel_ = this.i18n('save');
        this.confirmButtonDisabled_ = false;
        this.confirmButtonVisible_ = true;
        this.closeButtonVisible_ = false;
        this.dialogTitle_ =
            this.i18n('securityKeysUpdateCredentialDialogTitle');
        break;
      case CredentialManagementDialogPage.ERROR:
        this.cancelButtonVisible_ = false;
        this.confirmButtonLabel_ = this.i18n('continue');
        this.confirmButtonDisabled_ = false;
        this.confirmButtonVisible_ = true;
        this.closeButtonVisible_ = false;
        this.dialogTitle_ =
            this.i18n('securityKeysCredentialManagementDialogTitle');
        break;
      case CredentialManagementDialogPage.CONFIRM:
        this.cancelButtonVisible_ = true;
        this.confirmButtonLabel_ = this.i18n('delete');
        this.confirmButtonVisible_ = true;
        this.closeButtonVisible_ = false;
        this.dialogTitle_ =
            this.i18n('securityKeysCredentialManagementConfirmDeleteTitle');
        break;
      default:
        assertNotReached();
    }
  }

  protected onConfirmButtonClick_() {
    switch (this.dialogPage_) {
      case CredentialManagementDialogPage.PIN_PROMPT:
        this.submitPin_();
        break;
      case CredentialManagementDialogPage.PIN_ERROR:
        this.$.dialog.close();
        this.fire('credential-management-set-pin');
        break;
      case CredentialManagementDialogPage.CREDENTIALS:
        this.$.dialog.close();
        break;
      case CredentialManagementDialogPage.EDIT:
        this.updateUserInformation_();
        break;
      case CredentialManagementDialogPage.ERROR:
        this.dialogPage_ = CredentialManagementDialogPage.CREDENTIALS;
        break;
      case CredentialManagementDialogPage.CONFIRM:
        this.deleteCredential_();
        break;
      default:
        assertNotReached();
    }
  }

  protected onCancelButtonClick_() {
    switch (this.dialogPage_) {
      case CredentialManagementDialogPage.INITIAL:
      case CredentialManagementDialogPage.PIN_PROMPT:
      case CredentialManagementDialogPage.PIN_ERROR:
      case CredentialManagementDialogPage.CREDENTIALS:
        this.$.dialog.close();
        break;
      case CredentialManagementDialogPage.EDIT:
      case CredentialManagementDialogPage.ERROR:
      case CredentialManagementDialogPage.CONFIRM:
        this.dialogPage_ = CredentialManagementDialogPage.CREDENTIALS;
        break;
      default:
        assertNotReached();
    }
  }

  protected onDialogClose_() {
    this.browserProxy_.close();
  }

  protected onCloseButtonClick_() {
    this.$.dialog.close();
  }

  protected isEmpty_(str: string|null): boolean {
    return !str || str.length === 0;
  }

  protected onIronSelect_(e: Event) {
    // Prevent this event from bubbling since it is unnecessarily triggering
    // the listener within settings-animated-pages.
    e.stopPropagation();
  }

  protected onDeleteButtonClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    this.credentialIdToDelete_ = target.dataset['credentialid']!;
    assert(!this.isEmpty_(this.credentialIdToDelete_));

    this.confirmMsg_ =
        this.i18n('securityKeysCredentialManagementConfirmDeleteCredential');
    this.dialogPage_ = CredentialManagementDialogPage.CONFIRM;
  }

  private deleteCredential_() {
    this.browserProxy_.deleteCredentials([this.credentialIdToDelete_])
        .then((response) => {
          if (!response.success) {
            this.onError_(response.message);
            return;
          }
          this.credentials_ = this.credentials_.filter(
              c => c.credentialId !== this.credentialIdToDelete_);
          this.dialogPage_ = CredentialManagementDialogPage.CREDENTIALS;
        });
  }

  private validateInput_() {
    this.displayNameInputError_ =
        this.newDisplayName_.length > MAX_INPUT_LENGTH ?
        this.i18n('securityKeysInputTooLong') :
        '';
    this.userNameInputError_ = this.newUsername_.length > MAX_INPUT_LENGTH ?
        this.i18n('securityKeysInputTooLong') :
        '';

    this.confirmButtonDisabled_ =
        !this.isEmpty_(this.displayNameInputError_ + this.userNameInputError_);
  }

  protected onUpdateButtonClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const credentialId = target.dataset['credentialid']!;

    for (const credential of this.credentials_) {
      if (credential.credentialId === credentialId) {
        this.editingCredential_ = credential;
        break;
      }
    }

    assert(this.editingCredential_);
    this.newDisplayName_ = this.editingCredential_.userDisplayName;
    this.newUsername_ = this.editingCredential_.userName;

    this.dialogPage_ = CredentialManagementDialogPage.EDIT;
  }

  protected onNewDisplayNameValueChanged_(e: CustomEvent<{value: string}>) {
    this.newDisplayName_ = e.detail.value;
  }

  protected onNewUsernameValueChanged_(e: CustomEvent<{value: string}>) {
    this.newUsername_ = e.detail.value;
  }

  private updateUserInformation_() {
    assert(this.dialogPage_ === CredentialManagementDialogPage.EDIT);
    assert(this.editingCredential_);

    if (this.isEmpty_(this.newUsername_)) {
      this.newUsername_ = this.editingCredential_.userName;
    }
    if (this.isEmpty_(this.newDisplayName_)) {
      this.newDisplayName_ = this.editingCredential_.userDisplayName;
    }

    this.browserProxy_
        .updateUserInformation(
            this.editingCredential_.credentialId,
            this.editingCredential_.userHandle, this.newUsername_,
            this.newDisplayName_)
        .then((response) => {
          if (!response.success) {
            this.onError_(response.message);
            return;
          }

          this.credentials_ = this.credentials_.map(c => {
            if (c.credentialId === this.editingCredential_!.credentialId) {
              return {
                ...c,
                userName: this.newUsername_,
                userDisplayName: this.newDisplayName_,
              };
            }
            return c;
          });
        });
    this.dialogPage_ = CredentialManagementDialogPage.CREDENTIALS;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-keys-credential-management-dialog':
        SettingsSecurityKeysCredentialManagementDialogElement;
  }
}

export type SecurityKeysCredentialManagementDialogElement =
    SettingsSecurityKeysCredentialManagementDialogElement;

customElements.define(
    SettingsSecurityKeysCredentialManagementDialogElement.is,
    SettingsSecurityKeysCredentialManagementDialogElement);
