// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {PasswordRequestorMixin, PasswordRequestorMixinInterface} from './password_requestor_mixin.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * This mixin bundles functionality required to show a password to the user.
 * It is used by <password-list-item>.
 */
export const ShowPasswordMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T):
        (T|PasswordRequestorMixinInterface)&
    Constructor<ShowPasswordMixinInterface> => {
      class ShowPasswordMixin extends PasswordRequestorMixin
      (superClass) {
        static get properties() {
          return {
            entry: Object,
          };
        }

        entry: chrome.passwordsPrivate.PasswordUiEntry;

        getPasswordInputType() {
          return this.entry.password || this.entry.federationText ? 'text' :
                                                                    'password';
        }

        showPasswordTitle(password: string, hide: string, show: string) {
          return password ? hide : show;
        }

        getShowButtonLabel(password: string) {
          return loadTimeData.getStringF(
              (password) ? 'hidePasswordLabel' : 'showPasswordLabel',
              this.entry.username, this.entry.urls.shown);
        }

        getIconClass() {
          return this.entry.password ? 'icon-visibility-off' :
                                       'icon-visibility';
        }

        getPassword() {
          const NUM_PLACEHOLDERS = 10;
          return this.entry.federationText || this.entry.password ||
              ' '.repeat(NUM_PLACEHOLDERS);
        }

        /**
         * Handler for showing the passwords. If the password will be shown in
         * view password dialog, it should be handled by the dialog via the
         * event. If the password should be displayed inline, the method should
         * update the text.
         */
        onShowPasswordButtonClick() {
          if (this.entry.password) {
            this.hide();
            return;
          }

          this.requestPlaintextPassword(
                  this.entry.id, chrome.passwordsPrivate.PlaintextReason.VIEW)
              .then(password => {
                this.set('entry.password', password);
              }, () => {});
        }

        hide() {
          this.set('entry.password', '');
        }
      }

      return ShowPasswordMixin;
    });


export interface ShowPasswordMixinInterface {
  entry: chrome.passwordsPrivate.PasswordUiEntry;

  /**
   * Gets the password input's type. Should be 'text' when password is visible
   * or when there's federated text otherwise 'password'.
   */
  getPasswordInputType(): string;

  /**
   * Gets the title text for the show/hide icon.
   */
  showPasswordTitle(password: string, hide: string, show: string): string;

  /**
   * Gets the a11y label for the show/hide button.
   */
  getShowButtonLabel(password: string): string;

  /**
   * Get the right icon to display when hiding/showing a password.
   */
  getIconClass(): string;

  /**
   * Gets the text of the password. Will use the value of |password| unless it
   * cannot be shown, in which case it will be a fixed number of spaces. It can
   * also be the federated text.
   */
  getPassword(): string;

  /** Handler for clicking the show/hide button. */
  onShowPasswordButtonClick(): void;

  /** Hides the password. */
  hide(): void;
}
