// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-credential-management-dialog' is a
 * dialog for viewing and erasing credentials stored on a security key.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared.css.js';
import '../site_favicon.js';
import '../i18n_setup.js';
import './security_keys_pin_field.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import type {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Credential, SecurityKeysCredentialBrowserProxy, StartCredentialManagementResponse} from './security_keys_browser_proxy.js';
import {SecurityKeysCredentialBrowserProxyImpl} from './security_keys_browser_proxy.js';
import {getTemplate} from './security_keys_credential_management_dialog.html.js';
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
    credentialList: IronListElement,
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
    WebUiListenerMixin(I18nMixin(PolymerElement));

const MAX_INPUT_LENGTH: number = 62;

export class SettingsSecurityKeysCredentialManagementDialogElement extends
    SettingsSecurityKeysCredentialManagementDialogElementBase {
  static get is() {
    return 'settings-security-keys-credential-management-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The ID of the element currently shown in the dialog.
       */
      dialogPage_: {
        type: String,
        value: CredentialManagementDialogPage.INITIAL,
        observer: 'dialogPageChanged_',
      },

      /**
       * The list of credentials displayed in the dialog.
       */
      credentials_: {
        type: Array,
        notify: true,
      },

      /**
       * The message displayed on the "error" dialog page.
       */
      errorMsg_: String,

      cancelButtonVisible_: Boolean,
      closeButtonVisible_: Boolean,
      confirmButtonDisabled_: Boolean,
      confirmButtonLabel_: String,
      confirmButtonVisible_: Boolean,
      confirmMsg_: String,
      credentialIdToDelete_: String,
      displayNameInputError_: String,
      editingCredential_: Object,
      editButtonVisible_: Boolean,
      minPinLength_: Number,
      newDisplayName_: String,
      newUsername_: String,
      userHandle_: String,
      userNameInputError_: String,
    };
  }

  private cancelButtonVisible_: boolean;
  private closeButtonVisible_: boolean;
  private confirmButtonDisabled_: boolean;
  private confirmButtonLabel_: string;
  private confirmButtonVisible_: boolean;
  private confirmMsg_: string;
  private credentialIdToDelete_: string;
  private credentials_: Credential[];
  private dialogPage_: CredentialManagementDialogPage;
  private dialogTitle_: string;
  private displayNameInputError_: string;
  private editingCredential_: Credential;
  private editButtonVisible_: boolean;
  private errorMsg_: string;
  private minPinLength_: number;
  private newDisplayName_: string;
  private newUsername_: string;
  private userNameInputError_: string;

  private browserProxy_: SecurityKeysCredentialBrowserProxy =
      SecurityKeysCredentialBrowserProxyImpl.getInstance();
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
    this.$.credentialList.fire('iron-resize');
    this.dialogPage_ = CredentialManagementDialogPage.CREDENTIALS;
  }

  private dialogPageChanged_() {
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
        this.$.pin.focus();
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
    this.dispatchEvent(new CustomEvent(
        'credential-management-dialog-ready-for-testing',
        {bubbles: true, composed: true}));
  }

  private onConfirmButtonClick_() {
    switch (this.dialogPage_) {
      case CredentialManagementDialogPage.PIN_PROMPT:
        this.submitPin_();
        break;
      case CredentialManagementDialogPage.PIN_ERROR:
        this.$.dialog.close();
        this.dispatchEvent(new CustomEvent(
            'credential-management-set-pin', {bubbles: true, composed: true}));
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

  private onCancelButtonClick_() {
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

  private onDialogClosed_() {
    this.browserProxy_.close();
  }

  private close_() {
    this.$.dialog.close();
  }

  private isEmpty_(str: string|null): boolean {
    return !str || str.length === 0;
  }

  private onIronSelect_(e: Event) {
    // Prevent this event from bubbling since it is unnecessarily triggering
    // the listener within settings-animated-pages.
    e.stopPropagation();

    // Asynchronously notify the iron-list of the possible resize.
    setTimeout(() => this.$.credentialList.notifyResize(), 0);
  }

  private onDeleteButtonClick_(e: Event) {
    const target = e.target as CrIconButtonElement;
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
          for (let i = 0; i < this.credentials_.length; i++) {
            if (this.credentials_[i].credentialId ===
                this.credentialIdToDelete_) {
              this.credentials_.splice(i, 1);
              break;
            }
          }
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

  private onUpdateButtonClick_(e: Event) {
    const target = e.target as CrIconButtonElement;

    for (const credential of this.credentials_) {
      if (credential.credentialId === target.dataset['credentialid']!) {
        this.editingCredential_ = credential;
        break;
      }
    }

    this.newDisplayName_ = this.editingCredential_.userDisplayName;
    this.newUsername_ = this.editingCredential_.userName;

    this.dialogPage_ = CredentialManagementDialogPage.EDIT;
  }

  private updateUserInformation_() {
    assert(this.dialogPage_ === CredentialManagementDialogPage.EDIT);

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

          for (let i = 0; i < this.credentials_.length; i++) {
            if (this.credentials_[i].credentialId ===
                this.editingCredential_.credentialId) {
              const newCred: Credential =
                  Object.assign({}, this.credentials_[i]);

              newCred.userName = this.newUsername_;
              newCred.userDisplayName = this.newDisplayName_;

              this.credentials_.splice(i, 1, newCred);
              this.$.credentialList.fire('iron-resize');
              break;
            }
          }
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

customElements.define(
    SettingsSecurityKeysCredentialManagementDialogElement.is,
    SettingsSecurityKeysCredentialManagementDialogElement);
