// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-credential-management-dialog' is a
 * dialog for viewing and erasing credentials stored on a security key.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared_css.js';
import '../site_favicon.js';
import './security_keys_pin_field.js';

import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {Credential, SecurityKeysCredentialBrowserProxy, SecurityKeysCredentialBrowserProxyImpl} from './security_keys_browser_proxy.js';
import {SettingsSecurityKeysPinFieldElement} from './security_keys_pin_field.js';

export enum CredentialManagementDialogPage {
  INITIAL = 'initial',
  PIN_PROMPT = 'pinPrompt',
  CREDENTIALS = 'credentials',
  ERROR = 'error',
}

interface SettingsSecurityKeysCredentialManagementDialogElement {
  $: {
    dialog: CrDialogElement,
    pin: SettingsSecurityKeysPinFieldElement,
    credentialList: IronListElement,
  };
}

const SettingsSecurityKeysCredentialManagementDialogElementBase =
    WebUIListenerMixin(I18nMixin(PolymerElement));

class SettingsSecurityKeysCredentialManagementDialogElement extends
    SettingsSecurityKeysCredentialManagementDialogElementBase {
  static get is() {
    return 'settings-security-keys-credential-management-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
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
      credentials_: Array,

      /**
       * The message displayed on the "error" dialog page.
       */
      errorMsg_: String,

      cancelButtonVisible_: Boolean,
      confirmButtonVisible_: Boolean,
      confirmButtonDisabled_: Boolean,
      confirmButtonLabel_: String,
      closeButtonVisible_: Boolean,
      deleteInProgress_: Boolean,
      minPinLength_: Number,
    };
  }

  private dialogPage_: CredentialManagementDialogPage;
  private credentials_: Array<Credential>;
  private errorMsg_: string;
  private cancelButtonVisible_: boolean;
  private confirmButtonVisible_: boolean;
  private confirmButtonDisabled_: boolean;
  private confirmButtonLabel_: string;
  private closeButtonVisible_: boolean;
  private deleteInProgress_: boolean;
  private minPinLength_: number;

  private browserProxy_: SecurityKeysCredentialBrowserProxy =
      SecurityKeysCredentialBrowserProxyImpl.getInstance();
  private checkedCredentialIds_: Set<string>|null = null;
  private showSetPINButton_: boolean = false;

  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
    this.addWebUIListener(
        'security-keys-credential-management-finished',
        (error: string, requiresPINChange = false) =>
            this.onError_(error, requiresPINChange));
    this.checkedCredentialIds_ = new Set();
    this.browserProxy_.startCredentialManagement().then(([minPinLength]) => {
      this.minPinLength_ = minPinLength;
      this.dialogPage_ = CredentialManagementDialogPage.PIN_PROMPT;
    });
  }

  private onError_(error: string, requiresPINChange = false) {
    this.errorMsg_ = error;
    this.showSetPINButton_ = requiresPINChange;
    this.dialogPage_ = CredentialManagementDialogPage.ERROR;
  }

  private submitPIN_() {
    // Disable the confirm button to prevent concurrent submissions.
    this.confirmButtonDisabled_ = true;

    this.$.pin.trySubmit(pin => this.browserProxy_.providePIN(pin))
        .then(
            () => {
              // Leave confirm button disabled while enumerating credentials.
              this.browserProxy_.enumerateCredentials().then(
                  (credentials: Array<Credential>) =>
                      this.onCredentials_(credentials));
            },
            () => {
              // Wrong PIN.
              this.confirmButtonDisabled_ = false;
            });
  }

  private onCredentials_(credentials: Array<Credential>) {
    if (!credentials.length) {
      this.onError_(this.i18n('securityKeysCredentialManagementNoCredentials'));
      return;
    }
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
        break;
      case CredentialManagementDialogPage.PIN_PROMPT:
        this.cancelButtonVisible_ = true;
        this.confirmButtonLabel_ = this.i18n('continue');
        this.confirmButtonDisabled_ = false;
        this.confirmButtonVisible_ = true;
        this.closeButtonVisible_ = false;
        this.$.pin.focus();
        break;
      case CredentialManagementDialogPage.CREDENTIALS:
        this.cancelButtonVisible_ = true;
        this.confirmButtonLabel_ = this.i18n('delete');
        this.confirmButtonDisabled_ = true;
        this.confirmButtonVisible_ = true;
        this.closeButtonVisible_ = false;
        break;
      case CredentialManagementDialogPage.ERROR:
        this.cancelButtonVisible_ = true;
        this.confirmButtonLabel_ = this.i18n('securityKeysSetPinButton');
        this.confirmButtonVisible_ = this.showSetPINButton_;
        this.closeButtonVisible_ = false;
        break;
      default:
        assertNotReached();
    }
    this.dispatchEvent(new CustomEvent(
        'credential-management-dialog-ready-for-testing',
        {bubbles: true, composed: true}));
  }

  private confirmButtonClick_() {
    switch (this.dialogPage_) {
      case CredentialManagementDialogPage.PIN_PROMPT:
        this.submitPIN_();
        break;
      case CredentialManagementDialogPage.CREDENTIALS:
        this.deleteSelectedCredentials_();
        break;
      case CredentialManagementDialogPage.ERROR:
        this.$.dialog.close();
        this.dispatchEvent(new CustomEvent(
            'credential-management-set-pin', {bubbles: true, composed: true}));
        break;
      default:
        assertNotReached();
    }
  }

  private close_() {
    this.$.dialog.close();
  }

  /**
   * Stringifies the user entity of a Credential for display in the dialog.
   */
  private formatUser_(credential: Credential): string {
    if (this.isEmpty_(credential.userDisplayName)) {
      return credential.userName;
    }
    return `${credential.userDisplayName} (${credential.userName})`;
  }

  private onDialogClosed_() {
    this.browserProxy_.close();
  }

  private isEmpty_(str: string|null): boolean {
    return !str || str.length === 0;
  }

  private onIronSelect_(e: Event) {
    // Prevent this event from bubbling since it is unnecessarily triggering
    // the listener within settings-animated-pages.
    e.stopPropagation();
  }

  /**
   * Handler for checking or unchecking a credential.
   */
  private checkedCredentialsChanged_(e: Event) {
    const target = e.target as CrCheckboxElement;
    const credentialId = target.dataset['id']!;
    if (target.checked) {
      this.checkedCredentialIds_!.add(credentialId);
    } else {
      this.checkedCredentialIds_!.delete(credentialId);
    }
    this.confirmButtonDisabled_ = this.checkedCredentialIds_!.size === 0;
  }

  /**
   * @return true if the checkbox for |credentialId| is checked.
   */
  private credentialIsChecked_(credentialId: string): boolean {
    return this.checkedCredentialIds_!.has(credentialId);
  }

  private deleteSelectedCredentials_() {
    assert(this.dialogPage_ === CredentialManagementDialogPage.CREDENTIALS);
    assert(this.credentials_ && this.credentials_.length > 0);
    assert(this.checkedCredentialIds_!.size > 0);

    this.confirmButtonDisabled_ = true;
    this.deleteInProgress_ = true;
    this.browserProxy_
        .deleteCredentials(Array.from(this.checkedCredentialIds_!))
        .then((error) => {
          this.confirmButtonDisabled_ = false;
          this.deleteInProgress_ = false;
          this.onError_(error);
        });
  }
}

customElements.define(
    SettingsSecurityKeysCredentialManagementDialogElement.is,
    SettingsSecurityKeysCredentialManagementDialogElement);
