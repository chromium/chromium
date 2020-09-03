// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordsListHandler is a container for a passwords list
 * responsible for handling events associated with the overflow menu (copy,
 * editing, removal, moving to account).
 */

import './password_edit_dialog.js';
import './password_move_to_account_dialog.js';
import './password_remove_dialog.js';
import './password_list_item.js';
import './password_edit_dialog.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {SyncBrowserProxyImpl} from '../people_page/sync_browser_proxy.m.js';

// <if expr="chromeos">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {PasswordMoreActionsClickedEvent} from './password_list_item.js';
import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {PasswordRemoveDialogPasswordsRemovedEvent} from './password_remove_dialog.js';

Polymer({
  is: 'passwords-list-handler',

  _template: html`{__html_template__}`,

  properties: {

    /**
     * The model for any active menus or dialogs. The value is reset to null
     * whenever actions from the menus/dialogs are concluded.
     * @private {?PasswordListItemElement}
     */
    activePassword: {
      type: Object,
      value: null,
    },

    /**
     * Whether the edit dialog and removal notification should show information
     * about which location(s) a password is stored.
     */
    shouldShowStorageDetails: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether an option for moving a password to the account should be offered
     * in the overflow menu.
     */
    allowMoveToAccountOption: {
      type: Boolean,
      value: false,
    },

    // <if expr="chromeos">
    /** @type {BlockingRequestManager} */
    tokenRequestManager: Object,
    // </if>

    /** @private */
    enablePasswordCheck_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enablePasswordCheck');
      }
    },

    /** @private */
    editPasswordsInSettings_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('editPasswordsInSettings');
      }
    },

    /**
     * Check if editPasswordsInSettings flag is true and entry isn't federation
     * credential.
     * @private
     * */
    isEditDialog_: {
      type: Boolean,
      computed: 'computeIsEditDialog_(editPasswordsInSettings_, activePassword)'
    },

    /** @private */
    showPasswordEditDialog_: {type: Boolean, value: false},

    /** @private */
    showPasswordMoveToAccountDialog_: {type: Boolean, value: false},

    /** @private */
    showPasswordRemoveDialog_: {type: Boolean, value: false},

    /**
     * The element to return focus to, when the currently active dialog is
     * closed.
     * @private {?HTMLElement}
     */
    activeDialogAnchor_: {type: Object, value: null},

    /**
     * The message displayed in the toast following a password removal.
     * @private
     */
    removalNotification_: {
      type: String,
      value: '',
    },

    /**
     * The email of the first signed-in account, or the empty string if
     * there's none.
     * @private
     */
    firstSignedInAccountEmail_: {
      type: String,
      value: '',
    }
  },

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  listeners: {
    'password-more-actions-clicked': 'onPasswordMoreActionsClicked_',
    'password-remove-dialog-passwords-removed':
        'onPasswordRemoveDialogPasswordsRemoved_',
  },

  /** @private {?PasswordManagerProxy} */
  passwordManager_: null,

  /** @override */
  attached() {
    this.passwordManager_ = PasswordManagerImpl.getInstance();

    const extractFirstAccountEmail = accounts => {
      this.firstSignedInAccountEmail_ =
          accounts.length > 0 ? accounts[0].email : '';
    };
    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(
        extractFirstAccountEmail);
    this.addWebUIListener('stored-accounts-updated', extractFirstAccountEmail);
  },

  /** @override */
  detached() {
    if (this.$.toast.open) {
      this.$.toast.hide();
    }
  },

  /**
   * Helper function that checks if editPasswordsInSettings flag is true and
   * entry isn't federation credential.
   * @return {boolean}
   * @private
   * */
  computeIsEditDialog_() {
    return this.editPasswordsInSettings_ &&
        (!this.activePassword || !this.activePassword.entry.federationText);
  },

  /**
   * Closes the toast manager.
   */
  onSavedPasswordOrExceptionRemoved() {
    this.$.toast.hide();
  },

  /**
   * Opens the password action menu.
   * @param {PasswordMoreActionsClickedEvent} event
   * @private
   */
  onPasswordMoreActionsClicked_(event) {
    const target = event.detail.target;

    this.activePassword = event.detail.listItem;
    this.$.menu.showAt(target);
    this.activeDialogAnchor_ = target;
  },

  /**
   * Requests the plaintext password for the current active password.
   * @param {!chrome.passwordsPrivate.PlaintextReason} reason The reason why the
   *     plaintext password is requested.
   * @param {function(string): void} callback The callback that gets invoked
   *     with the retrieved password.
   * @private
   */
  requestActivePlaintextPassword_(reason, callback) {
    this.passwordManager_
        .requestPlaintextPassword(this.activePassword.entry.getAnyId(), reason)
        .then(callback, error => {
          // <if expr="chromeos">
          // If no password was found, refresh auth token and retry.
          this.tokenRequestManager.request(() => {
            this.requestActivePlaintextPassword_(reason, callback);
          });
          // </if>
        });
  },

  /** @private */
  onMenuEditPasswordTap_() {
    if (this.isEditDialog_) {
      this.requestActivePlaintextPassword_(
          chrome.passwordsPrivate.PlaintextReason.EDIT, password => {
            this.set('activePassword.entry.password', password);
            this.showPasswordEditDialog_ = true;
          });
    } else {
      this.showPasswordEditDialog_ = true;
    }
    this.$.menu.close();
    this.activePassword.hide();
  },

  /**
   * @return {string}
   * @private
   */
  getMenuEditPasswordName_() {
    return this.isEditDialog_ ? this.i18n('editPassword') :
                                this.i18n('passwordViewDetails');
  },

  /** @private */
  onPasswordEditDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
    this.activePassword.hide();
    this.activePassword = null;
  },

  /** @private */
  onMovePasswordToAccountDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
    this.activePassword = null;
  },

  /**
   * Copy selected password to clipboard.
   * @private
   */
  onMenuCopyPasswordButtonTap_() {
    // Copy to clipboard occurs inside C++ and we don't expect getting
    // result back to javascript.
    this.requestActivePlaintextPassword_(
        chrome.passwordsPrivate.PlaintextReason.COPY, _ => {
          this.activePassword = null;
        });

    this.$.menu.close();
  },

  /**
   * Handler for the remove option in the overflow menu. If the password only
   * exists in one location, deletes it directly. Otherwise, opens the remove
   * dialog to allow choosing from which locations to remove.
   * @private
   */
  onMenuRemovePasswordTap_() {
    this.$.menu.close();

    if (this.activePassword.entry.isPresentOnDevice() &&
        this.activePassword.entry.isPresentInAccount()) {
      this.showPasswordRemoveDialog_ = true;
      return;
    }

    const idToRemove = this.activePassword.entry.isPresentInAccount() ?
        this.activePassword.entry.accountId :
        this.activePassword.entry.deviceId;
    this.passwordManager_.removeSavedPassword(idToRemove);
    this.displayRemovalNotification_(
        this.activePassword.entry.isPresentInAccount(),
        this.activePassword.entry.isPresentOnDevice());
    this.activePassword = null;
  },

  /**
   * At least one of |removedFromAccount| or |removedFromDevice| must be true.
   * @param {boolean} removedFromAccount
   * @param {boolean} removedFromDevice
   * @private
   */
  displayRemovalNotification_(removedFromAccount, removedFromDevice) {
    assert(removedFromAccount || removedFromDevice);
    this.removalNotification_ = this.i18n('passwordDeleted');
    if (this.shouldShowStorageDetails) {
      if (removedFromAccount && removedFromDevice) {
        this.removalNotification_ =
            this.i18n('passwordDeletedFromAccountAndDevice');
      } else if (removedFromAccount) {
        this.removalNotification_ = this.i18n('passwordDeletedFromAccount');
      } else {
        this.removalNotification_ = this.i18n('passwordDeletedFromDevice');
      }
    }
    this.$.toast.show();
    this.fire('iron-announce', {text: this.removalNotification_});
    this.fire('iron-announce', {text: this.i18n('undoDescription')});
  },

  /**
   * @param {PasswordRemoveDialogPasswordsRemovedEvent} event
   */
  onPasswordRemoveDialogPasswordsRemoved_(event) {
    this.displayRemovalNotification_(
        event.detail.removedFromAccount, event.detail.removedFromDevice);
  },

  /**
   * @private
   */
  onUndoButtonClick_() {
    this.passwordManager_.undoRemoveSavedPasswordOrException();
    this.onSavedPasswordOrExceptionRemoved();
  },

  /**
   * Should only be called when |activePassword| has a device copy.
   * @private
   */
  onMenuMovePasswordToAccountTap_() {
    this.$.menu.close();
    this.showPasswordMoveToAccountDialog_ = true;
  },

  /**
   * @private
   */
  onPasswordMoveToAccountDialogClosed_() {
    this.showPasswordMoveToAccountDialog_ = false;
    this.activePassword = null;

    // The entry possibly disappeared, so don't reset the focus.
    this.activeDialogAnchor_ = null;
  },

  /**
   * @private
   */
  onPasswordRemoveDialogClosed_() {
    this.showPasswordRemoveDialog_ = false;
    this.activePassword = null;

    // A removal possibly happened, so don't reset the focus.
    this.activeDialogAnchor_ = null;
  },

  /**
   * Whether the move option should be present in the overflow menu.
   * @private
   * @return {boolean}
   */
  shouldShowMoveToAccountOption_() {
    const isFirstSignedInAccountPassword = !!this.activePassword &&
        this.activePassword.entry.urls.origin.includes('accounts.google.com') &&
        this.activePassword.entry.username === this.firstSignedInAccountEmail_;
    // It's not useful to move a password for an account into that same account.
    return this.allowMoveToAccountOption && !isFirstSignedInAccountPassword;
  },

});
