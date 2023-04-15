// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {getTemplate} from './passwords_importer.html.js';

export interface PasswordsImporterElement {
  $: {
    linkRow: CrLinkRowElement,
  };
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

      inProgress_: {
        type: Boolean,
        value: false,
      },
    };
  }

  isUserSyncingPasswords: boolean;
  isAccountStoreUser: boolean;
  accountEmail: string;

  private inProgress_: boolean;
  private showSelectFileButton_: boolean;
  private bannerDescription_: string;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

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

  private async onSelectFileClick_() {
    this.inProgress_ = true;
    // For "non-account-store-users" users passwords are stored in the "profile"
    // (DEVICE) store.
    const destinationStore = chrome.passwordsPrivate.PasswordStoreSet.DEVICE;

    // TODO(crbug/1432962): Take into account selected store for
    // "account-store-users".
    await this.passwordManager_.importPasswords(destinationStore);

    // TODO(crbug/1432962): Add handler for the results of importPasswords.
    this.inProgress_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-importer': PasswordsImporterElement;
  }
}

customElements.define(PasswordsImporterElement.is, PasswordsImporterElement);
