// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../shared_style.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {ShowPasswordMixin} from '../show_password_mixin.js';

import {getTemplate} from './edit_password_dialog.html.js';

export interface EditPasswordDialogElement {
  $: {
    dialog: CrDialogElement,
    saveButton: CrButtonElement,
    cancelButton: CrButtonElement,
    usernameInput: CrInputElement,
    passwordInput: CrInputElement,
    showPasswordButton: CrIconButtonElement,
  };
}

/**
 * Computes possible conflicting username by finding all credentials with
 * matching signonRealms. Returns map where key is the username and value is
 * human readable representation of signonRealm. Username is considered
 * conflicting if shares any domain with |currentPassword|.
 */
function getConflictingUsernames(
    currentPassword: chrome.passwordsPrivate.PasswordUiEntry,
    passwords: chrome.passwordsPrivate.PasswordUiEntry[]): Map<string, string> {
  assert(currentPassword.affiliatedDomains);
  const currentSignonRealms =
      currentPassword.affiliatedDomains.map(domain => domain.signonRealm);
  return passwords.reduce(function(conflictingUsername, entry) {
    assert(entry.affiliatedDomains);
    const signonRealms =
        entry.affiliatedDomains.map(domain => domain.signonRealm);

    const signonRealm = signonRealms.filter(
        signonRealm => currentSignonRealms.includes(signonRealm))[0];
    if (signonRealm) {
      conflictingUsername.set(
          entry.username,
          entry.affiliatedDomains
              .find(domain => domain.signonRealm === signonRealm)!.name);
    }
    return conflictingUsername;
  }, new Map());
}

const EditPasswordDialogElementBase =
    ShowPasswordMixin(I18nMixin(PolymerElement));

export class EditPasswordDialogElement extends EditPasswordDialogElementBase {
  static get is() {
    return 'edit-password-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      credential: Object,

      username_: String,
      password_: String,
      note_: String,

      conflictingUsernames_: {
        type: Object,
        values: () => new Map(),
      },

      usernameErrorMessage_: {
        type: String,
        computed: 'computeUsernameErrorMessage_(credential, username_, ' +
            'conflictingUsernames_)',
      },
    };
  }

  credential: chrome.passwordsPrivate.PasswordUiEntry;
  private username_: string;
  private password_: string;
  private note_: string;
  private conflictingUsernames_: Map<string, string>;
  private usernameErrorMessage_: string|null;
  private setSavedPasswordsListener_: (
      (entries: chrome.passwordsPrivate.PasswordUiEntry[]) => void)|null = null;

  override connectedCallback() {
    super.connectedCallback();
    assert(this.credential.password);

    this.username_ = this.credential.username;
    this.password_ = this.credential.password;
    this.note_ = this.credential.note ?? '';
    this.setSavedPasswordsListener_ = passwordList => {
      this.conflictingUsernames_ =
          getConflictingUsernames(this.credential, passwordList);
    };

    PasswordManagerImpl.getInstance().getSavedPasswordList().then(
        this.setSavedPasswordsListener_);
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setSavedPasswordsListener_);
    PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
    this.setSavedPasswordsListener_ = null;
  }

  private computeUsernameErrorMessage_(): string|null {
    if (!this.conflictingUsernames_) {
      return null;
    }
    if (this.conflictingUsernames_.has(this.username_) &&
        this.username_ !== this.credential.username) {
      return this.i18n(
          'usernameAlreadyUsed',
          this.conflictingUsernames_.get(this.username_)!,
      );
    }
    return null;
  }

  private doesUsernameExistAlready_(): boolean {
    return !!this.usernameErrorMessage_;
  }


  private onCancel_() {
    this.$.dialog.close();
  }

  private getFootnote_(): string {
    assert(this.credential.affiliatedDomains);
    return this.i18n(
        'editPasswordFootnote',
        this.credential.affiliatedDomains[0]?.name ?? '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'edit-password-dialog': EditPasswordDialogElement;
  }
}

customElements.define(EditPasswordDialogElement.is, EditPasswordDialogElement);
