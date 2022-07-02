// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-check-edit-dialog' is the dialog that allows showing
 * a saved password.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../icons.html.js';
import '../settings_shared_css.js';
import '../settings_vars.css.js';
import './passwords_shared.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_check_edit_dialog.html.js';
import {PasswordCheckInteraction, PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';

export interface SettingsPasswordCheckEditDialogElement {
  $: {
    dialog: CrDialogElement,
    cancel: HTMLElement,
    passwordInput: CrInputElement,
    save: CrButtonElement,
  };
}

const SettingsPasswordCheckEditDialogElementBase = I18nMixin(PolymerElement);

export class SettingsPasswordCheckEditDialogElement extends
    SettingsPasswordCheckEditDialogElementBase {
  static get is() {
    return 'settings-password-check-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The password that the user is interacting with now.
       */
      item: Object,

      /**
       * Whether the password is visible or obfuscated.
       */
      visible: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the input is invalid.
       */
      inputInvalid_: Boolean,
    };
  }

  item: chrome.passwordsPrivate.InsecureCredential|null;
  private visible: boolean;
  private inputInvalid_: boolean;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
    focusWithoutInk(this.$.cancel);
  }

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  }

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   */
  private onCancel_() {
    this.close();
  }

  /**
   * Handler for tapping the 'save' button. Should just dismiss the dialog.
   */
  private onSave_() {
    this.passwordManager_.recordPasswordCheckInteraction(
        PasswordCheckInteraction.EDIT_PASSWORD);
    assert(this.item);
    this.passwordManager_
        .changeInsecureCredential(this.item, this.$.passwordInput.value)
        .finally(() => {
          this.close();
        });
  }

  /**
   * @return The title text for the show/hide icon.
   */
  private showPasswordTitle_(): string {
    return this.i18n(this.visible ? 'hidePassword' : 'showPassword');
  }

  /**
   * @return The visibility icon class, depending on whether the password is
   *     already visible.
   */
  private showPasswordIcon_(): string {
    return this.visible ? 'icon-visibility-off' : 'icon-visibility';
  }

  /**
   * @return The type of the password input field (text or password),
   *     depending on whether the password should be obfuscated.
   */
  private getPasswordInputType_(): string {
    return this.visible ? 'text' : 'password';
  }

  /**
   * Handler for tapping the show/hide button.
   */
  private onShowPasswordButtonClick_() {
    this.visible = !this.visible;
  }

  /**
   * @return The text to be displayed as the dialog's footnote.
   */
  private getFootnote_(): string {
    return this.i18n('editPasswordFootnote', this.item!.formattedOrigin);
  }

  /**
   * @return The label for the origin, depending on the whether it's a site or
   *     an app.
   */
  private getSiteOrApp_(): string {
    return this.i18n(
        this.item!.isAndroidCredential ? 'editCompromisedPasswordApp' :
                                         'editCompromisedPasswordSite');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-password-check-edit-dialog':
        SettingsPasswordCheckEditDialogElement;
  }
}

customElements.define(
    SettingsPasswordCheckEditDialogElement.is,
    SettingsPasswordCheckEditDialogElement);
