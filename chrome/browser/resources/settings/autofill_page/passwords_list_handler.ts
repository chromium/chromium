// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordsListHandler is a container for a passwords list
 * responsible for handling events associated with the overflow menu (copy,
 * editing, removal, moving to account).
 */

import '../i18n_setup.js';
// <if expr="is_chromeos">
import '/shared/settings/controls/password_prompt_dialog.js';
// </if>
import './password_edit_dialog.js';
import './password_remove_dialog.js';
import './password_list_item.js';
import './password_edit_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, Router} from '../router.js';

import {PASSWORD_MORE_ACTIONS_CLICKED_EVENT_NAME, PasswordListItemElement, PasswordMoreActionsClickedEvent} from './password_list_item.js';
import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {PasswordRemovalMixin} from './password_removal_mixin.js';
import {PasswordRemoveDialogPasswordsRemovedEvent} from './password_remove_dialog.js';
import {PasswordRequestorMixin} from './password_requestor_mixin.js';
import {PASSWORD_VIEW_PAGE_REQUESTED_EVENT_NAME, PasswordRemovalUrlParams, PasswordViewPageRequestedEvent} from './password_view.js';
import {getTemplate} from './passwords_list_handler.html.js';

export interface PasswordsListHandlerElement {
  $: {
    copyToast: CrToastElement,
    menu: CrActionMenuElement,
    menuCopyPassword: HTMLElement,
    menuEditPassword: HTMLElement,
    menuRemovePassword: HTMLElement,
    removalNotification: HTMLElement,
    removalToast: CrToastElement,
  };
}

type FocusConfig = Map<string, string|(() => void)>;

const PasswordsListHandlerElementBase = RouteObserverMixin(PasswordRemovalMixin(
    PasswordRequestorMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

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
       * The model for any active menus or dialogs. The value is reset to null
       * whenever actions from the menus/dialogs are concluded.
       */
      activePassword_: {
        type: Object,
        value: null,
      },

      showPasswordEditDialog_: {type: Boolean, value: false},

      showPasswordSendButton_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableSendPasswords');
        },
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

      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },

      enablePasswordViewPage_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePasswordViewPage');
        },
      },
    };
  }

  savedPasswords: chrome.passwordsPrivate.PasswordUiEntry[];
  isAccountStoreUser: boolean;

  private activePassword_: PasswordListItemElement|null;
  private showPasswordEditDialog_: boolean;
  private showSendPasswordButton_: boolean;
  private activeDialogAnchor_: HTMLElement|null;
  private removalNotification_: string;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  focusConfig: FocusConfig;
  private enablePasswordViewPage_: boolean;

  override ready() {
    super.ready();

    this.addEventListener(
        PASSWORD_MORE_ACTIONS_CLICKED_EVENT_NAME,
        this.passwordMoreActionsClickedHandler_);

    this.addEventListener(
        PASSWORD_VIEW_PAGE_REQUESTED_EVENT_NAME,
        this.onPasswordViewPageRequestedEvent);
  }

  override currentRouteChanged(route: Route): void {
    if (route !== routes.PASSWORDS && route !== routes.DEVICE_PASSWORDS) {
      return;
    }

    const params = Router.getInstance().getQueryParameters();
    if (!params.get(PasswordRemovalUrlParams.REMOVED_FROM_STORES)) {
      return;
    }
    this.displayRemovalNotification_(
        params.get(PasswordRemovalUrlParams.REMOVED_FROM_STORES) as
        chrome.passwordsPrivate.PasswordStoreSet);
    params.delete(PasswordRemovalUrlParams.REMOVED_FROM_STORES);
    Router.getInstance().updateRouteParams(params);
    // TODO(https://crbug.com/1298027): find a way to announce the removal toast
    // before the first item in the page
    getAnnouncerInstance().announce(this.removalNotification_);
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
    this.displayRemovalNotification_(event.detail.removedFromStores);
  }

  private onPasswordViewPageRequestedEvent(event:
                                               PasswordViewPageRequestedEvent) {
    this.activePassword_ = event.detail;
  }

  private isPasswordEditable_() {
    return !this.activePassword_ || !this.activePassword_.entry.federationText;
  }

  private onMenuEditPasswordClick_() {
    if (this.isPasswordEditable_()) {
      this.requestPlaintextPassword(
              this.activePassword_!.entry.id,
              chrome.passwordsPrivate.PlaintextReason.EDIT)
          .then(password => {
            this.set('activePassword_.entry.password', password);
            this.showPasswordEditDialog_ = true;
          }, () => {});
    } else {
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
    this.activeDialogAnchor_ = null;
    this.activePassword_!.hide();
    this.activePassword_ = null;
  }

  private onMenuSendPasswordClick_() {
    // TODO(crbug.com/1298608): Implement the logic.
  }

  /**
   * Copy selected password to clipboard.
   */
  private onMenuCopyPasswordButtonClick_() {
    // Copy to clipboard occurs inside C++ and we don't expect getting
    // result back to javascript.
    this.requestPlaintextPassword(
            this.activePassword_!.entry.id,
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
    this.displayRemovalNotification_(password.storedIn);
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
      removedFromStores: chrome.passwordsPrivate.PasswordStoreSet) {
    if (this.isAccountStoreUser) {
      switch (removedFromStores) {
        case chrome.passwordsPrivate.PasswordStoreSet.DEVICE:
          this.removalNotification_ = this.i18n('passwordDeletedFromDevice');
          break;
        case chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT:
          this.removalNotification_ = this.i18n('passwordDeletedFromAccount');
          break;
        case chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT:
          this.removalNotification_ =
              this.i18n('passwordDeletedFromAccountAndDevice');
          break;
      }
    } else {
      this.removalNotification_ = this.i18n('passwordDeleted');
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

  override onPasswordRemoveDialogClose() {
    super.onPasswordRemoveDialogClose();
    this.activePassword_ = null;

    // A removal possibly happened, so don't reset the focus.
    this.activeDialogAnchor_ = null;
  }

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    if (!this.enablePasswordViewPage_) {
      return;
    }
    assert(!oldConfig);
    this.focusConfig.set(routes.PASSWORD_VIEW.path, () => {
      if (!this.activePassword_) {
        return;
      }
      focusWithoutInk(this.activePassword_);
      this.activePassword_ = null;
    });
  }
}

customElements.define(
    PasswordsListHandlerElement.is, PasswordsListHandlerElement);
