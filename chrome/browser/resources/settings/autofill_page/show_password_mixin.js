// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// <if expr="chromeos">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

/**
 * This mixin bundles functionality required to show a password to the user.
 * It is used by both <password-list-item> and <password-edit-dialog>.
 *
 * @polymer
 * @mixinFunction
 * @polymerBehavior
 */
export const ShowPasswordMixin = dedupingMixin(superClass => {
  /**
   * @polymer
   * @mixinClass
   * @implements {ShowPasswordMixinInterface}
   */
  class ShowPasswordMixin extends superClass {
    static get properties() {
      return {
        /**
         * @type {!MultiStorePasswordUiEntry}
         */
        entry: Object,

        // <if expr="chromeos">
        /** @type BlockingRequestManager */
        tokenRequestManager: Object
        // </if>
      };
    }

    /** @override */
    getPasswordInputType() {
      return this.entry.password || this.entry.federationText ? 'text' :
                                                                'password';
    }

    /** @override */
    showPasswordTitle(password, hide, show) {
      return password ? hide : show;
    }

    /** @override */
    getIconClass() {
      return this.entry.password ? 'icon-visibility-off' : 'icon-visibility';
    }

    /** @override */
    getPassword() {
      const NUM_PLACEHOLDERS = 10;
      return this.entry.federationText || this.entry.password ||
          ' '.repeat(NUM_PLACEHOLDERS);
    }

    /** @override */
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
              error => {
                // <if expr="chromeos">
                // If no password was found, refresh auth token and retry.
                this.tokenRequestManager.request(
                    this.onShowPasswordButtonTap.bind(this));
                // </if>
              });
    }

    /** @override */
    hide() {
      this.set('entry.password', '');
    }
  }

  return ShowPasswordMixin;
});


/** @interface */
export class ShowPasswordMixinInterface {
  constructor() {
    /** @type {!MultiStorePasswordUiEntry} */
    this.entry;
  }

  /**
   * Gets the password input's type. Should be 'text' when password is visible
   * or when there's federated text otherwise 'password'.
   * @return {string}
   */
  getPasswordInputType() {}

  /**
   * Gets the title text for the show/hide icon.
   * @param {string} password
   * @param {string} hide
   * @param {string} show
   * @return {string}
   */
  showPasswordTitle(password, hide, show) {}

  /**
   * Get the right icon to display when hiding/showing a password.
   * @return {string}
   */
  getIconClass() {}

  /**
   * Gets the text of the password. Will use the value of |password| unless it
   * cannot be shown, in which case it will be a fixed number of spaces. It can
   * also be the federated text.
   * @return {string}
   */
  getPassword() {}

  /** Handler for tapping the show/hide button. */
  onShowPasswordButtonTap() {}

  /** Hides the password. */
  hide() {}
}
