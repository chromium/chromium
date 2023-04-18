// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {getTemplate} from './passwords_importer.html.js';

export interface PasswordsImporterElement {
  $: {
    linkRow: CrLinkRowElement,
  };
}

enum DialogState {
  NO_DIALOG,
  STORE_PICKER,
}

const PasswordsImporterElementBase = I18nMixin(PolymerElement);

export class PasswordsImporterElement extends PasswordsImporterElementBase {
  static get is() {
    return 'passwords-importer';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      inProgress_: {
        type: Boolean,
        value: false,
      },

      dialogState_: Number,

      dialogStateEnum_: {
        type: Object,
        value: DialogState,
        readOnly: true,
      },

      storeOptionEnum_: {
        type: Object,
        value: chrome.passwordsPrivate.PasswordStoreSet,
        readOnly: true,
      },

      selectedStoreOption_: String,

      showSelectFileButton_: {
        type: Boolean,
        computed: 'computeShowSelectFileButton_(isAccountStoreUser,' +
            'inProgress_)',
      },

      bannerDescription_: {
        type: String,
        computed: 'computeBannerDescription_(isUserSyncingPasswords,' +
            'isAccountStoreUser, accountEmail)',
      },
    };
  }

  static get observers() {
    return [
      'updateDefaultStore_(isAccountStoreUser)',
      'updatePasswordsSavedToAccount_(isUserSyncingPasswords)',
    ];
  }

  isUserSyncingPasswords: boolean;
  isAccountStoreUser: boolean;
  accountEmail: string;

  private inProgress_: boolean;
  private dialogState_: DialogState = DialogState.NO_DIALOG;
  // Refers both to syncing users with sync enabled for passwords and account
  // store users who choose to import passwords to their account.
  private passwordsSavedToAccount_: boolean;
  private selectedStoreOption_: string;
  private showSelectFileButton_: boolean;
  private bannerDescription_: string;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  private updateDefaultStore_() {
    if (this.isAccountStoreUser) {
      PasswordManagerImpl.getInstance().isAccountStoreDefault().then(
          isAccountStoreDefault => {
            this.selectedStoreOption_ = isAccountStoreDefault ?
                chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT :
                chrome.passwordsPrivate.PasswordStoreSet.DEVICE;
          });
    }
  }

  private updatePasswordsSavedToAccount_() {
    if (this.isUserSyncingPasswords) {
      this.passwordsSavedToAccount_ = true;
    } else {
      this.passwordsSavedToAccount_ = false;
    }
  }

  private isState_(state: DialogState): boolean {
    return this.dialogState_ === state;
  }

  private computeShowSelectFileButton_(): boolean {
    return !this.inProgress_ && !this.isAccountStoreUser;
  }

  private computeBannerDescription_(): string {
    if (this.isAccountStoreUser) {
      return this.i18n('importPasswordsGenericDescription');
    }
    if (this.isUserSyncingPasswords) {
      return this.i18n(
          'importPasswordsDescriptionAccount',
          this.i18n('localPasswordManager'), this.accountEmail);
    }
    return this.i18n('importPasswordsDescriptionDevice');
  }

  private onBannerClick_() {
    if (this.isAccountStoreUser && !this.inProgress_ &&
        this.isState_(DialogState.NO_DIALOG)) {
      this.dialogState_ = DialogState.STORE_PICKER;
    }
  }

  private closeDialog_() {
    this.dialogState_ = DialogState.NO_DIALOG;
    // TODO(crbug/1432962): Make sure that focus behaves correctly when the
    // dialog is closed.
  }

  private onCloseClick_() {
    this.closeDialog_();
  }

  private async onSelectFileClick_() {
    this.inProgress_ = true;
    // For "non-account-store-users" users passwords are stored in the "profile"
    // (DEVICE) store.
    let destinationStore = chrome.passwordsPrivate.PasswordStoreSet.DEVICE;
    if (this.isAccountStoreUser) {
      const storePicker =
          this.shadowRoot!.querySelector<HTMLSelectElement>('#storePicker');
      assert(storePicker);
      this.passwordsSavedToAccount_ = storePicker.value ===
          chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
      if (this.passwordsSavedToAccount_) {
        destinationStore = chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
      }
    }
    // Close the dialog while import is in progress or the user selects a file.
    this.closeDialog_();

    await this.passwordManager_.importPasswords(destinationStore);

    // TODO(crbug/1432962): Add handler for the results of importPasswords.
    this.inProgress_ = false;
  }

  private getStoreOptionAccountText_(): string {
    assert(this.accountEmail);
    return this.i18n(
        'passwordsStoreOptionAccount', this.i18n('localPasswordManager'),
        this.accountEmail);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-importer': PasswordsImporterElement;
  }
}

customElements.define(PasswordsImporterElement.is, PasswordsImporterElement);
