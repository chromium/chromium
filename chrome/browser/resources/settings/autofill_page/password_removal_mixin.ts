// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This mixin bundles the functionality for removal of a
 * chrome.passwordsPrivate.PasswordUiEntry. The UI elements inheriting this
 * mixin should also have a child PasswordRemoveDialogElement.
 */

import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';
import {PasswordRemoveDialogPasswordsRemovedEvent} from './password_remove_dialog.js';

type Constructor<T> = new (...args: any[]) => T;

export const PasswordRemovalMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PasswordRemovalMixinInterface> => {
      class PasswordRemovalMixin extends superClass {
        static get properties() {
          return {
            showPasswordRemoveDialog: {type: Boolean, value: false},
          };
        }

        showPasswordRemoveDialog: boolean;

        removePassword(password: chrome.passwordsPrivate.PasswordUiEntry):
            boolean {
          // TODO(https://crbug.com/1298027): Use Promise API to simplify the
          // logic.
          if (password.storedIn ===
              chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT) {
            this.showPasswordRemoveDialog = true;
            return false;
          }

          PasswordManagerImpl.getInstance().removeSavedPassword(
              password.id, password.storedIn);
          return true;
        }

        onPasswordRemoveDialogClose(): void {
          this.showPasswordRemoveDialog = false;
        }

        onPasswordRemoveDialogPasswordsRemoved(
            _event: PasswordRemoveDialogPasswordsRemovedEvent): void {
          return;
        }
      }

      return PasswordRemovalMixin;
    });

export interface PasswordRemovalMixinInterface {
  /**
   * If the credential is both in account and device, a remove dialog is shown.
   */
  showPasswordRemoveDialog: boolean;

  /**
   * If the password only exists in one location, deletes it directly.
   * Otherwise, opens the remove dialog to allow choosing from which locations
   * to remove.
   * @return Whether the password was removed.
   */
  removePassword(password: chrome.passwordsPrivate.PasswordUiEntry): boolean;


  /** Sets the property |showPasswordRemoveDialog| to false. */
  onPasswordRemoveDialogClose(): void;

  /**
   * This method is a no-op. UI elements inheriting this mixin should override
   * this method and handle the event.
   */
  onPasswordRemoveDialogPasswordsRemoved(
      event: PasswordRemoveDialogPasswordsRemovedEvent): void;
}
