// Copyright 2022 The Chromium Authors
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
import {CrTextareaElement} from 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {Page, Router} from '../router.js';
import {ShowPasswordMixin} from '../show_password_mixin.js';

import {getTemplate} from './add_password_dialog.html.js';

export interface AddPasswordDialogElement {
  $: {
    addButton: CrButtonElement,
    dialog: CrDialogElement,
    noteInput: CrTextareaElement,
    passwordInput: CrInputElement,
    showPasswordButton: CrIconButtonElement,
    usernameInput: CrInputElement,
    viewExistingPasswordLink: HTMLAnchorElement,
    websiteInput: CrInputElement,
  };
}

/**
 * When user enters more than or equal to 900 characters in the note field, a
 * footer will be displayed below the note to warn the user.
 */
export const PASSWORD_NOTE_WARNING_CHARACTER_COUNT = 900;

/**
 * When user enters more than 1000 characters, the note will become invalid and
 * save button will be disabled.
 */
export const PASSWORD_NOTE_MAX_CHARACTER_COUNT = 1000;

const AddPasswordDialogElementBase =
    ShowPasswordMixin(I18nMixin(PolymerElement));

function getUsernamesByOrigin(
    passwords: chrome.passwordsPrivate.PasswordUiEntry[]):
    Map<string, Set<string>> {
  // Group existing usernames by signonRealm.
  return passwords.reduce(function(usernamesByOrigin, entry) {
    assert(entry.affiliatedDomains);
    for (const domain of entry.affiliatedDomains) {
      if (!usernamesByOrigin.has(domain.signonRealm)) {
        usernamesByOrigin.set(domain.signonRealm, new Set());
      }
      usernamesByOrigin.get(domain.signonRealm).add(entry.username);
    }
    return usernamesByOrigin;
  }, new Map());
}

export class AddPasswordDialogElement extends AddPasswordDialogElementBase {
  static get is() {
    return 'add-password-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      website_: String,
      username_: String,

      password_: {
        type: String,
        value: '',
      },

      note_: String,

      urlCollection_: Object,

      usernamesBySignonRealm_: {
        type: Object,
        values: () => new Map(),
      },

      /**
       * Error message if the website input is invalid.
       */
      websiteErrorMessage_: {type: String, value: null},

      usernameErrorMessage_: {
        type: String,
        computed: 'computeUsernameErrorMessage_(urlCollection_, username_, ' +
            'usernamesBySignonRealm_)',
      },

      canAddPassword_: {
        type: Boolean,
        computed: 'computeCanAddPassword_(websiteErrorMessage_, username_, ' +
            'password_, note_)',
      },
    };
  }

  private website_: string;
  private username_: string;
  private password_: string;
  private note_: string;
  private usernamesBySignonRealm_: Map<string, Set<string>>;
  private websiteErrorMessage_: string|null;
  private usernameErrorMessage_: string|null;
  private urlCollection_: chrome.passwordsPrivate.UrlCollection|null;

  private setSavedPasswordsListener_: (
      (entries: chrome.passwordsPrivate.PasswordUiEntry[]) => void)|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.setSavedPasswordsListener_ = passwordList => {
      this.usernamesBySignonRealm_ = getUsernamesByOrigin(passwordList);
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

  private onCancel_() {
    this.$.dialog.close();
  }

  /**
   * Helper function that checks whether the entered url is valid.
   */
  private async validateWebsite_() {
    PasswordManagerImpl.getInstance()
        .getUrlCollection(this.website_)
        .then(urlCollection => {
          this.urlCollection_ = urlCollection;
          this.websiteErrorMessage_ =
              !urlCollection ? this.i18n('notValidWebsite') : null;
        })
        .catch(() => this.websiteErrorMessage_ = this.i18n('notValidWebsite'));
  }

  private onWebsiteInputBlur_() {
    if (!this.websiteErrorMessage_ && !this.website_.includes('.')) {
      this.websiteErrorMessage_ =
          this.i18n('missingTLD', `${this.website_}.com`);
    }
  }

  private isWebsiteInputInvalid_(): boolean {
    return !!this.websiteErrorMessage_;
  }

  private computeUsernameErrorMessage_(): string|null {
    const signonRealm = this.urlCollection_?.signonRealm;
    if (!signonRealm) {
      return null;
    }
    if (this.usernamesBySignonRealm_.has(signonRealm) &&
        this.usernamesBySignonRealm_.get(signonRealm)!.has(this.username_)) {
      return this.i18n('usernameAlreadyUsed', this.website_);
    }
    return null;
  }

  private doesUsernameExistAlready_(): boolean {
    return !!this.usernameErrorMessage_;
  }

  private isNoteInputInvalid_(): boolean {
    return this.note_.length >= PASSWORD_NOTE_MAX_CHARACTER_COUNT;
  }

  private getFirstNoteFooter_(): string {
    return this.note_.length < PASSWORD_NOTE_WARNING_CHARACTER_COUNT ?
        '' :
        this.i18n(
            'passwordNoteCharacterCountWarning',
            PASSWORD_NOTE_MAX_CHARACTER_COUNT);
  }

  private getSecondNoteFooter_(): string {
    return this.note_.length < PASSWORD_NOTE_WARNING_CHARACTER_COUNT ?
        '' :
        this.i18n(
            'passwordNoteCharacterCount', this.note_.length,
            PASSWORD_NOTE_MAX_CHARACTER_COUNT);
  }

  private computeCanAddPassword_(): boolean {
    if (this.isWebsiteInputInvalid_()) {
      return false;
    }
    if (this.doesUsernameExistAlready_()) {
      return false;
    }
    if (this.password_.length === 0) {
      return false;
    }
    if (this.isNoteInputInvalid_()) {
      return false;
    }
    return true;
  }

  private onAddClick_() {
    assert(this.computeCanAddPassword_());
    assert(this.urlCollection_);
    PasswordManagerImpl.getInstance()
        .addPassword({
          url: this.urlCollection_.signonRealm,
          username: this.username_,
          password: this.password_,
          note: this.note_,
          useAccountStore: false,
        })
        .then(() => {
          this.$.dialog.close();
        })
        .catch(() => {});
  }

  private getViewExistingPasswordAriaDescription_(): string {
    return this.urlCollection_ ?
        this.i18n(
            'viewExistingPasswordAriaDescription', this.username_,
            this.urlCollection_.shown) :
        '';
  }

  private onViewExistingPasswordClick_(e: Event) {
    e.preventDefault();
    Router.getInstance().navigateTo(
        Page.PASSWORD_DETAILS, this.urlCollection_?.shown);
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'add-password-dialog': AddPasswordDialogElement;
  }
}

customElements.define(AddPasswordDialogElement.is, AddPasswordDialogElement);
