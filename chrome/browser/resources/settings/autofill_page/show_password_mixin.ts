// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

// <if expr="chromeos_ash or chromeos_lacros">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * This mixin bundles functionality required to show a password to the user.
 * It is used by <password-list-item>.
 */
export const ShowPasswordMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<ShowPasswordMixinInterface> => {
      class ShowPasswordMixin extends superClass {
        static get properties() {
          return {
            entry: Object,

            // <if expr="chromeos_ash or chromeos_lacros">
            tokenRequestManager: Object
            // </if>
          };
        }

        entry: MultiStorePasswordUiEntry;

        // <if expr="chromeos_ash or chromeos_lacros">
        tokenRequestManager: BlockingRequestManager;
        // </if>

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

        onShowPasswordButtonTap() {
          if (this.entry.password) {
            this.hide();
            return;
          }
          PasswordManagerImpl.getInstance()
              .requestPlaintextPassword(
                  this.entry.getAnyId(),
                  chrome.passwordsPrivate.PlaintextReason.VIEW)
              .then(
                  password => {
                    this.set('entry.password', password);
                  },
                  _error => {
                    // <if expr="chromeos_ash or chromeos_lacros">
                    // If no password was found, refresh auth token and retry.
                    this.tokenRequestManager.request(
                        () => this.onShowPasswordButtonTap());
                    // </if>
                  });
        }

        hide() {
          this.set('entry.password', '');
        }
      }

      return ShowPasswordMixin;
    });


export interface ShowPasswordMixinInterface {
  entry: MultiStorePasswordUiEntry;

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

  /** Handler for tapping the show/hide button. */
  onShowPasswordButtonTap(): void;

  /** Hides the password. */
  hide(): void;
}
