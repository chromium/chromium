// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordsListHandler is a container for a passwords list
 * responsible for handling events associated with the overflow menu (copy,
 * editing, removal, moving to account).
 */

import '../i18n_setup.js';
import './password_edit_dialog.js';
import './password_move_to_account_dialog.js';
import './password_remove_dialog.js';
import './password_list_item.js';
import './password_edit_dialog.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {StoredAccount, SyncBrowserProxyImpl} from '../people_page/sync_browser_proxy.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordDialogMode} from './password_edit_dialog.js';
import {PasswordListItemElement, PasswordMoreActionsClickedEvent} from './password_list_item.js';
import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {PasswordRemovalMixin, PasswordRemovalMixinInterface} from './password_removal_mixin.js';
import {PasswordRemoveDialogPasswordsRemovedEvent} from './password_remove_dialog.js';
import {PasswordRequestorMixin, PasswordRequestorMixinInterface} from './password_requestor_mixin.js';
import {PasswordRemovalUrlParams} from './password_view.js';
import {getTemplate} from './passwords_list_handler.html.js';

declare global {
  interface HTMLElementEventMap {
    'password-more-actions-clicked': PasswordMoreActionsClickedEvent;
  }
}

export interface PasswordsListHandlerElement {
  $: {
    copyToast: CrToastElement,
    menu: CrActionMenuElement,
    menuCopyPassword: HTMLElement,
    menuEditPassword: HTMLElement,
    menuMovePasswordToAccount: HTMLElement,
    menuRemovePassword: HTMLElement,
    removalNotification: HTMLElement,
    removalToast: CrToastElement,
  };
}

const PasswordsListHandlerElementBase =
    RouteObserverMixin(PasswordRemovalMixin(PasswordRequestorMixin(
        WebUIListenerMixin(I18nMixin(PolymerElement))))) as {
      new (): PolymerElement & I18nMixinInterface &
          WebUIListenerMixinInterface & PasswordRequestorMixinInterface &
          PasswordRemovalMixinInterface & RouteObserverMixinInterface,
    };

export class PasswordsListHandlerElement extends
    PasswordsListHandlerElementBase {
  static get is() {
    return 'passwords-list-handler';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Saved passwords after deduplicating versions that are repeated in the
       * account and on the device.
       */
      savedPasswords: {
        type: Array,
        value: () => [],
      },

      /**
       * If true, the edit dialog and removal notification show
       * information about which location(s) a password is stored.
       */
      isAccountStoreUser: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether an option for moving a password to the account should be
       * offered in the overflow menu.
       */
      allowMoveToAccountOption: {
        type: Boolean,
        value: false,
      },

      /**
       * The model for any active menus or dialogs. The value is reset to null
       * whenever actions from the menus/dialogs are concluded.
       */
      activePassword_: {
        type: Object,
        value: null,
      },

      /**
       * Request to specify how to open the password edit dialog.
       */
      requestedDialogMode_: {type: Object, value: null},

      showPasswordEditDialog_: {type: Boolean, value: false},

      showPasswordMoveToAccountDialog_: {type: Boolean, value: false},

      showPasswordSendButton_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableSendPasswords');
        }
      },

      /**
       * The element to return focus to, when the currently active dialog is
       * closed.
       */
      activeDialogAnchor_: {type: Object, value: null},

      /**
       * The message displayed in the toast following a password removal.
       */
      removalNotification_: {
        type: String,
        value: '',
      },

      /**
       * The email of the first signed-in account, or the empty string if
       * there's none.
       */
      firstSignedInAccountEmail_: {
        type: String,
        value: '',
      },
    };
  }

  savedPasswords: Array<MultiStorePasswordUiEntry>;
  isAccountStoreUser: boolean;
  allowMoveToAccountOption: boolean;

  private activePassword_: PasswordListItemElement|null;
  private requestedDialogMode_: PasswordDialogMode|null;
  private showPasswordEditDialog_: boolean;
  private showPasswordMoveToAccountDialog_: boolean;
  private showSendPasswordButton_: boolean;
  private activeDialogAnchor_: HTMLElement|null;
  private removalNotification_: string;
  private firstSignedInAccountEmail_: string;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  override ready() {
    super.ready();

    this.addEventListener(
        'password-more-actions-clicked',
        this.passwordMoreActionsClickedHandler_);
  }

  override connectedCallback() {
    super.connectedCallback();

    const extractFirstAccountEmail = (accounts: Array<StoredAccount>) => {
      this.firstSignedInAccountEmail_ =
          accounts.length > 0 ? accounts[0].email : '';
    };
    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(
        extractFirstAccountEmail);
    this.addWebUIListener('stored-accounts-updated', extractFirstAccountEmail);
  }

  override currentRouteChanged(route: Route): void {
    if (route !== routes.PASSWORDS && route !== routes.DEVICE_PASSWORDS) {
      return;
    }

    const params = Router.getInstance().getQueryParameters();
    if (!params.get(PasswordRemovalUrlParams.REMOVED_FROM_ACCOUNT) ||
        !params.get(PasswordRemovalUrlParams.REMOVED_FROM_DEVICE)) {
      return;
    }

    this.displayRemovalNotification_(
        params.get(PasswordRemovalUrlParams.REMOVED_FROM_ACCOUNT) === 'true',
        params.get(PasswordRemovalUrlParams.REMOVED_FROM_DEVICE) === 'true');
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.hideToasts_();
  }

  /**
   * Closes the toast manager.
   */
  onSavedPasswordOrExceptionRemoved() {
    this.$.removalToast.hide();
  }

  /**
   * Opens the password action menu.
   */
  private passwordMoreActionsClickedHandler_(
      event: PasswordMoreActionsClickedEvent) {
    const target = event.detail.target;

    this.activePassword_ = event.detail.listItem;
    this.$.menu.showAt(target);
    this.activeDialogAnchor_ = target;
  }

  override onPasswordRemoveDialogPasswordsRemoved(
      event: PasswordRemoveDialogPasswordsRemovedEvent) {
    super.onPasswordRemoveDialogPasswordsRemoved(event);
    this.displayRemovalNotification_(
        event.detail.removedFromAccount, event.detail.removedFromDevice);
  }

  private isPasswordEditable_() {
    return !this.activePassword_ || !this.activePassword_.entry.federationText;
  }

  private onMenuEditPasswordTap_() {
    if (this.isPasswordEditable_()) {
      this.requestPlaintextPassword(
              this.activePassword_!.entry.getAnyId(),
              chrome.passwordsPrivate.PlaintextReason.EDIT)
          .then(password => {
            this.set('activePassword_.entry.password', password);
            this.requestedDialogMode_ = PasswordDialogMode.EDIT;
            this.showPasswordEditDialog_ = true;
          }, () => {});
    } else {
      this.requestedDialogMode_ = PasswordDialogMode.FEDERATED_VIEW;
      this.showPasswordEditDialog_ = true;
    }
    this.$.menu.close();
    this.activePassword_!.hide();
  }

  private getMenuEditPasswordName_(): string {
    return this.isPasswordEditable_() ? this.i18n('editPassword') :
                                        this.i18n('passwordViewDetails');
  }

  private onPasswordEditDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    assert(this.activeDialogAnchor_);
    focusWithoutInk(this.activeDialogAnchor_);
    this.requestedDialogMode_ = null;
    this.activeDialogAnchor_ = null;
    this.activePassword_!.hide();
    this.activePassword_ = null;
  }

  private onMovePasswordToAccountDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    assert(this.activeDialogAnchor_);
    focusWithoutInk(this.activeDialogAnchor_);
    this.activeDialogAnchor_ = null;
    this.activePassword_ = null;
  }

  private onMenuSendPasswordTap_() {
    // TODO(crbug.com/1298608): Implement the logic.
  }

  /**
   * Copy selected password to clipboard.
   */
  private onMenuCopyPasswordButtonTap_() {
    // Copy to clipboard occurs inside C++ and we don't expect getting
    // result back to javascript.
    this.requestPlaintextPassword(
            this.activePassword_!.entry.getAnyId(),
            chrome.passwordsPrivate.PlaintextReason.COPY)
        .then((_: string) => {
          this.activePassword_ = null;
          this.displayCopyNotification_();
        }, () => {});

    this.$.menu.close();
  }

  /** Handler for the remove option in the overflow menu. */
  private onMenuRemovePasswordClick_() {
    this.$.menu.close();
    const password = this.activePassword_!.entry;
    assert(password);
    if (!this.removePassword(password)) {
      return;
    }
    this.displayRemovalNotification_(
        password.isPresentInAccount(), password.isPresentOnDevice());
    this.activePassword_ = null;
  }

  private hideToasts_() {
    if (this.$.removalToast.open) {
      this.$.removalToast.hide();
    }
    if (this.$.copyToast.open) {
      this.$.copyToast.hide();
    }
  }

  /**
   * At least one of |removedFromAccount| or |removedFromDevice| must be true.
   */
  private displayRemovalNotification_(
      removedFromAccount: boolean, removedFromDevice: boolean) {
    assert(removedFromAccount || removedFromDevice);
    this.removalNotification_ = this.i18n('passwordDeleted');
    if (this.isAccountStoreUser) {
      if (removedFromAccount && removedFromDevice) {
        this.removalNotification_ =
            this.i18n('passwordDeletedFromAccountAndDevice');
      } else if (removedFromAccount) {
        this.removalNotification_ = this.i18n('passwordDeletedFromAccount');
      } else {
        this.removalNotification_ = this.i18n('passwordDeletedFromDevice');
      }
    }

    this.hideToasts_();
    this.$.removalToast.show();
  }

  private onUndoButtonClick_() {
    this.passwordManager_.undoRemoveSavedPasswordOrException();
    this.onSavedPasswordOrExceptionRemoved();
  }

  private displayCopyNotification_() {
    this.hideToasts_();
    this.$.copyToast.show();
  }

  /**
   * Should only be called when |activePassword_| has a device copy.
   */
  private onMenuMovePasswordToAccountTap_() {
    this.$.menu.close();
    this.showPasswordMoveToAccountDialog_ = true;
  }

  private onPasswordMoveToAccountDialogClosed_() {
    this.showPasswordMoveToAccountDialog_ = false;
    this.activePassword_ = null;

    // The entry possibly disappeared, so don't reset the focus.
    this.activeDialogAnchor_ = null;
  }

  override onPasswordRemoveDialogClose() {
    super.onPasswordRemoveDialogClose();
    this.activePassword_ = null;

    // A removal possibly happened, so don't reset the focus.
    this.activeDialogAnchor_ = null;
  }

  /**
   * Whether the move option should be present in the overflow menu.
   */
  private shouldShowMoveToAccountOption_(): boolean {
    const isFirstSignedInAccountPassword = !!this.activePassword_ &&
        this.activePassword_.entry.urls.origin.includes(
            'accounts.google.com') &&
        this.activePassword_.entry.username === this.firstSignedInAccountEmail_;
    // It's not useful to move a password for an account into that same account.
    return this.allowMoveToAccountOption && !isFirstSignedInAccountPassword;
  }
}

customElements.define(
    PasswordsListHandlerElement.is, PasswordsListHandlerElement);
