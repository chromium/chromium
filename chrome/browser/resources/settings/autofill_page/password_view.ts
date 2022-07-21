// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-view' is the subpage containing details about the
 * password such as the URL, the username, the password and the note.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import '../controls/settings_textarea.js';
import '../i18n_setup.js';
// <if expr="chromeos_ash or chromeos_lacros">
import '../controls/password_prompt_dialog.js';
// </if>
import '../settings_shared.css.js';
import './password_edit_dialog.js';
import './password_remove_dialog.js';
import './passwords_shared.css.js';

import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

// <if expr="chromeos_ash or chromeos_lacros">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {MergePasswordsStoreCopiesMixin, MergePasswordsStoreCopiesMixinInterface} from './merge_passwords_store_copies_mixin.js';
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {SavedPasswordEditedEvent} from './password_edit_dialog.js';
import {PasswordRemovalMixin, PasswordRemovalMixinInterface} from './password_removal_mixin.js';
import {PasswordRemoveDialogPasswordsRemovedEvent} from './password_remove_dialog.js';
import {PasswordRequestorMixin, PasswordRequestorMixinInterface} from './password_requestor_mixin.js';
import {getTemplate} from './password_view.html.js';

export interface PasswordViewElement {
  $: {
    toast: CrToastElement,
  };
}

const PasswordViewElementBase =
    PasswordRemovalMixin(PasswordRequestorMixin(MergePasswordsStoreCopiesMixin(
        RouteObserverMixin(I18nMixin(PolymerElement))))) as {
      new (): PolymerElement & I18nMixinInterface &
          RouteObserverMixinInterface &
          MergePasswordsStoreCopiesMixinInterface &
          PasswordRequestorMixinInterface & PasswordRemovalMixinInterface,
    };

export enum PasswordRemovalUrlParams {
  REMOVED_FROM_STORES = 'removedFromStores',
}

export enum PasswordViewPageUrlParams {
  ID = 'id',
}

export function recordPasswordViewInteraction(
    interaction: PasswordViewPageInteractions) {
  chrome.metricsPrivate.recordEnumerationValue(
      'PasswordManager.PasswordViewPage.UserActions', interaction,
      PasswordViewPageInteractions.COUNT);
}

/**
 * Should be kept in sync with
 * |password_manager::metrics_util::PasswordViewPageInteractions|.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum PasswordViewPageInteractions {
  CREDENTIAL_ROW_CLICKED = 0,
  CREDENTIAL_FOUND = 1,
  CREDENTIAL_NOT_FOUND = 2,
  USERNAME_COPY_BUTTON_CLICKED = 3,
  PASSWORD_COPY_BUTTON_CLICKED = 4,
  PASSWORD_SHOW_BUTTON_CLICKED = 5,
  PASSWORD_EDIT_BUTTON_CLICKED = 6,
  PASSWORD_DELETE_BUTTON_CLICKED = 7,
  CREDENTIAL_EDITED = 8,
  // Must be last.
  COUNT = 9,
}

export class PasswordViewElement extends PasswordViewElementBase {
  static get is() {
    return 'password-view';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      id_: Number,

      activeDialogAnchorStack_: {
        type: Array,
        value: () => [],
      },

      toastText_: {
        type: String,
        value: '',
      },

      credential: {
        type: Object,
        value: null,
        notify: true,
      },

      isPasswordNotesEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePasswordNotes');
        },
      },

      isPasswordVisible_: {
        type: Boolean,
        value: false,
      },

      password_: {
        type: String,
        value: '',
      },

      showEditDialog_: {
        type: Boolean,
        value: false,
      },

      // <if expr="chromeos_ash or chromeos_lacros">
      showPasswordPromptDialog_: Boolean,
      // </if>

      /**
       * Used to keep the password view page open when a credential is
       * modified. savedPasswords may take its time to update.
       */
      recentlyEdited_: Boolean,

    };
  }

  static get observers() {
    return ['savedPasswordsChanged_(savedPasswords.splices, id_)'];
  }

  /**
   * Valid value for id is null or number, undefined is set for early return in
   * the observer.
   */
  private id_: number|null|undefined;
  private activeDialogAnchorStack_: HTMLElement[];
  private toastText_: string;
  credential: MultiStorePasswordUiEntry|null;
  private isPasswordNotesEnabled_: boolean;
  private isPasswordVisible_: boolean;
  private password_: string;
  private recentlyEdited_: boolean;
  // <if expr="chromeos_ash or chromeos_lacros">
  private showPasswordPromptDialog_: boolean;
  // </if>
  private showEditDialog_: boolean;

  // <if expr="chromeos_ash or chromeos_lacros">
  override connectedCallback() {
    super.connectedCallback();

    // If the user's account supports the password check, an auth token will be
    // required in order for them to view or export passwords. Otherwise there
    // is no additional security so |tokenRequestManager| will immediately
    // resolve requests.
    if (loadTimeData.getBoolean('userCannotManuallyEnterPassword')) {
      this.tokenRequestManager = new BlockingRequestManager();
    } else {
      this.tokenRequestManager =
          new BlockingRequestManager(() => this.openPasswordPromptDialog_());
    }
  }
  // </if>

  override currentRouteChanged(route: Route): void {
    if (route !== routes.PASSWORD_VIEW) {
      this.id_ = undefined;
      this.recentlyEdited_ = false;
      this.password_ = '';
      this.credential = null;
      this.hideToast_();
      return;
    }
    const queryParameters = Router.getInstance().getQueryParameters();

    const convertToNullOrNumber = (input: string|null) => {
      if (!input || Number.isNaN(Number(input))) {
        return null;
      }
      return Number(input);
    };
    this.id_ = convertToNullOrNumber(
        queryParameters.get(PasswordViewPageUrlParams.ID));
  }

  override onPasswordRemoveDialogPasswordsRemoved(
      event: PasswordRemoveDialogPasswordsRemovedEvent) {
    super.onPasswordRemoveDialogPasswordsRemoved(event);
    this.rerouteAndShowRemovalNotification_(event.detail.removedFromStores);
  }

  /** Gets the title text for the show/hide icon. */
  private getPasswordButtonTitle_(): string {
    assert(!this.isFederated_());
    return this.i18n(this.isPasswordVisible_ ? 'hidePassword' : 'showPassword');
  }

  /** Get the right icon to display when hiding/showing a password. */
  private getIconClass_(): string {
    assert(!this.isFederated_());
    return this.isPasswordVisible_ ? 'icon-visibility-off' : 'icon-visibility';
  }

  private getNoteClass_(): string {
    return this.credential!.note ? '' : 'empty-note';
  }

  private getNoteValue_(): string {
    return this.credential!.note || this.i18n('passwordNoNoteAdded');
  }

  /**
   * Show the password or a placeholder with 10 characters when password is not
   * set.
   */
  private getPassword_(): string {
    return this.password_ || ' '.repeat(10);
  }

  /**
   * Gets the password input's type. Should be 'text' when input content is
   * visible otherwise 'password'. If the entry is a federated credential,
   * the content (federation text) is always visible.
   */
  private getPasswordInputType_(): string {
    return this.isFederated_() || this.isPasswordVisible_ ? 'text' : 'password';
  }

  private isFederated_(): boolean {
    return !!this.credential && !!this.credential.federationText;
  }

  private isNoteEnabled_(): boolean {
    return !this.isFederated_() && this.isPasswordNotesEnabled_;
  }

  /** Handler to copy the password from the password field. */
  private onCopyPasswordButtonClick_() {
    assert(!this.isFederated_());
    recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_COPY_BUTTON_CLICKED);
    this.requestPlaintextPassword(
            this.credential!.id, chrome.passwordsPrivate.PlaintextReason.COPY)
        .then(() => {
          this.toastText_ = this.i18n('passwordCopiedToClipboard');
          this.showToast_();
        })
        .catch(() => {});
  }

  /** Handler to copy the username from the username field. */
  private onCopyUsernameButtonClick_() {
    navigator.clipboard.writeText(this.credential!.username).then(() => {
      this.toastText_ = this.i18n('passwordUsernameCopiedToClipboard');
      this.showToast_();
    });
    recordPasswordViewInteraction(
        PasswordViewPageInteractions.USERNAME_COPY_BUTTON_CLICKED);
  }

  /** Handler for the remove button. */
  private onDeleteButtonClick_() {
    assert(this.credential);
    recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_DELETE_BUTTON_CLICKED);
    if (!this.removePassword(this.credential)) {
      return;
    }
    this.rerouteAndShowRemovalNotification_(this.credential.storedIn);
  }

  /** Handler to open edit dialog for the password. */
  private onEditButtonClick_() {
    assert(!this.isFederated_());
    recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_EDIT_BUTTON_CLICKED);
    this.requestPlaintextPassword(
            this.credential!.id, chrome.passwordsPrivate.PlaintextReason.EDIT)
        .then(password => {
          this.credential!.password = password;
          this.showEditDialog_ = true;
        }, () => {});
  }

  private onEditDialogClosed_() {
    this.showEditDialog_ = false;
  }

  private onSavedPasswordEdited_(event: SavedPasswordEditedEvent) {
    this.recentlyEdited_ = true;
    // The dialog is recently closed. Use the new IDs to update the URL.
    const newParams = Router.getInstance().getQueryParameters();

    if (event.detail.accountId !== undefined) {
      newParams.set(
          PasswordViewPageUrlParams.ID, event.detail.accountId.toString());
    } else if (event.detail.deviceId !== undefined) {
      newParams.set(
          PasswordViewPageUrlParams.ID, event.detail.deviceId.toString());
    }
    Router.getInstance().updateRouteParams(newParams);
  }

  // <if expr="chromeos_ash or chromeos_lacros">
  /**
   * When this event fired, it means that the password-prompt-dialog succeeded
   * in creating a fresh token in the quickUnlockPrivate API. Because new tokens
   * can only ever be created immediately following a GAIA password check, the
   * passwordsPrivate API can now safely grant requests for secure data (i.e.
   * saved passwords) for a limited time. This observer resolves the request,
   * triggering a callback that requires a fresh auth token to succeed and that
   * was provided to the BlockingRequestManager by another DOM element seeking
   * secure data.
   *
   * @param e Contains newly created auth token
   *     chrome.quickUnlockPrivate.TokenInfo. Note that its precise value is not
   *     relevant here, only the facts that it's created.
   */
  private onTokenObtained_(
      e: CustomEvent<chrome.quickUnlockPrivate.TokenInfo>) {
    assert(e.detail);
    this.tokenRequestManager.resolve();
  }

  private onPasswordPromptClose_() {
    this.showPasswordPromptDialog_ = false;
    const toFocus = this.activeDialogAnchorStack_.pop();
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  private openPasswordPromptDialog_() {
    this.activeDialogAnchorStack_.push(getDeepActiveElement() as HTMLElement);
    this.showPasswordPromptDialog_ = true;
  }
  // </if>

  /** Handler for tapping the show/hide button. */
  private onShowPasswordButtonClick_() {
    assert(!this.isFederated_());
    if (this.isPasswordVisible_) {
      this.password_ = '';
      this.isPasswordVisible_ = false;
      return;
    }
    recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_SHOW_BUTTON_CLICKED);
    this.requestPlaintextPassword(
            this.credential!.id, chrome.passwordsPrivate.PlaintextReason.VIEW)
        .then(password => {
          this.password_ = password;
          this.isPasswordVisible_ = true;
        }, () => {});
  }

  /** Reroutes to PASSWORDS page and shows the removal notification */
  private rerouteAndShowRemovalNotification_(
      removedFromStores: chrome.passwordsPrivate.PasswordStoreSet): void {
    // TODO(https://crbug.com/1298027): find a way to reroute to
    // DEVICE_PASSWORDS if view is opened from there.
    const params = new URLSearchParams();
    params.set(
        PasswordRemovalUrlParams.REMOVED_FROM_STORES,
        removedFromStores.toString());
    Router.getInstance().navigateTo(routes.PASSWORDS, params);
  }

  private savedPasswordsChanged_() {
    this.credential = null;
    this.password_ = '';
    this.isPasswordVisible_ = false;
    // When an observed property changes, the observer will be called. Make sure
    // that all properties are set.
    if (!this.savedPasswords.length || this.id_ === undefined) {
      return;
    }
    const item = this.savedPasswords.find((item: MultiStorePasswordUiEntry) => {
      return item.id === this.id_;
    });

    if (!item) {
      if (!this.recentlyEdited_) {
        // Rerouting might have happened due to the edited username. Do not
        // reroute back.
        recordPasswordViewInteraction(
            PasswordViewPageInteractions.CREDENTIAL_NOT_FOUND);
        Router.getInstance().navigateTo(routes.PASSWORDS);
      }
      return;
    }

    this.credential = item;
    if (item.federationText) {
      this.password_ = item.federationText!;
    }
    this.showEditDialog_ = false;
    this.recentlyEdited_ = false;
    recordPasswordViewInteraction(
        PasswordViewPageInteractions.CREDENTIAL_FOUND);
  }

  private hideToast_() {
    this.$.toast.hide();
  }

  private showToast_() {
    this.$.toast.show();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-view': PasswordViewElement;
  }
}

customElements.define(PasswordViewElement.is, PasswordViewElement);
