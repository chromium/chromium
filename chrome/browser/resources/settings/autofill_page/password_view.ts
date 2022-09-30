// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-view' is the subpage containing details about the
 * password such as the URL, the username, the password and the note.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import '../i18n_setup.js';
// <if expr="is_chromeos">
import '../controls/password_prompt_dialog.js';
// </if>
import '../settings_shared.css.js';
import './password_edit_dialog.js';
import './password_remove_dialog.js';
import './passwords_shared.css.js';

import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

import {SavedPasswordEditedEvent} from './password_edit_dialog.js';
import {PasswordListItemElement} from './password_list_item.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {PasswordRemovalMixin, PasswordRemovalMixinInterface} from './password_removal_mixin.js';
import {PasswordRemoveDialogPasswordsRemovedEvent} from './password_remove_dialog.js';
import {PasswordRequestorMixin, PasswordRequestorMixinInterface} from './password_requestor_mixin.js';
import {getTemplate} from './password_view.html.js';

declare global {
  interface HTMLElementEventMap {
    [PASSWORD_VIEW_PAGE_REQUESTED_EVENT_NAME]: PasswordViewPageRequestedEvent;
  }
}

export type PasswordViewPageRequestedEvent =
    CustomEvent<PasswordListItemElement>;

export const PASSWORD_VIEW_PAGE_REQUESTED_EVENT_NAME =
    'password-view-page-requested';

export interface PasswordViewElement {
  $: {
    toast: CrToastElement,
  };
}

const PasswordViewElementBase =
    PasswordRemovalMixin(PasswordRequestorMixin(
        RouteObserverMixin(I18nMixin(PolymerElement)))) as {
      new (): PolymerElement & I18nMixinInterface &
          RouteObserverMixinInterface & PasswordRequestorMixinInterface &
          PasswordRemovalMixinInterface,
    };

export enum PasswordRemovalUrlParams {
  REMOVED_FROM_STORES = 'removedFromStores',
}

export enum PasswordViewPageUrlParams {
  ID = 'id',
}

export const PASSWORD_MANAGER_AUTH_TIMEOUT_PARAM = 'authTimeout';

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
  TIMED_OUT_IN_EDIT_DIALOG = 9,
  TIMED_OUT_IN_VIEW_PAGE = 10,
  CREDENTIAL_REQUESTED_BY_URL = 11,
  // Must be last.
  COUNT = 12,
}

export class PasswordViewElement extends PasswordViewElementBase {
  // TODO(crbug/1345899): Reroute to password list when the credential is
  // deleted or update the page when credential is updated from other sources
  // (e.g: sync).
  static get is() {
    return 'password-view';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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

      showEditDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private toastText_: string;
  credential: chrome.passwordsPrivate.PasswordUiEntry|null;
  private isPasswordNotesEnabled_: boolean;
  private isPasswordVisible_: boolean;
  private showEditDialog_: boolean;
  private visibilityChangedListener_: () => void;
  private passwordManagerAuthTimeoutListener_: () => void;

  override connectedCallback() {
    super.connectedCallback();

    this.passwordManagerAuthTimeoutListener_ = () => {
      if (Router.getInstance().getCurrentRoute() !== routes.PASSWORD_VIEW) {
        return;
      }
      recordPasswordViewInteraction(
          this.showEditDialog_ ?
              PasswordViewPageInteractions.TIMED_OUT_IN_EDIT_DIALOG :
              PasswordViewPageInteractions.TIMED_OUT_IN_VIEW_PAGE);

      const params = new URLSearchParams();
      params.set(PASSWORD_MANAGER_AUTH_TIMEOUT_PARAM, 'true');
      Router.getInstance().navigateTo(routes.PASSWORDS, params);
    };

    PasswordManagerImpl.getInstance().addPasswordManagerAuthTimeoutListener(
        this.passwordManagerAuthTimeoutListener_);

    FocusOutlineManager.forDocument(document);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    PasswordManagerImpl.getInstance().removePasswordManagerAuthTimeoutListener(
        this.passwordManagerAuthTimeoutListener_);
  }

  override currentRouteChanged(route: Route): void {
    if (route !== routes.PASSWORD_VIEW) {
      this.hideToast_();
      this.credential = null;
      this.isPasswordVisible_ = false;
      this.showEditDialog_ = false;
      return;
    }

    if (!this.credential) {
      this.requestCredential_();
    }
  }

  override ready() {
    super.ready();

    if (document.visibilityState !== 'visible') {
      this.visibilityChangedListener_ = () => {
        if (document.visibilityState === 'visible' &&
            Router.getInstance().getCurrentRoute() === routes.PASSWORD_VIEW &&
            !this.credential) {
          this.requestCredential_();
          document.removeEventListener(
              'visibilitychange', this.visibilityChangedListener_);
        }
      };
      document.addEventListener(
          'visibilitychange', this.visibilityChangedListener_);
    }
  }


  override onPasswordRemoveDialogPasswordsRemoved(
      event: PasswordRemoveDialogPasswordsRemovedEvent) {
    super.onPasswordRemoveDialogPasswordsRemoved(event);
    this.rerouteAndShowRemovalNotification_(event.detail.removedFromStores);
  }

  private getId_() {
    const idInput = Router.getInstance().getQueryParameters().get(
        PasswordViewPageUrlParams.ID);

    if (!idInput || Number.isNaN(Number(idInput))) {
      return null;
    }
    return Number(idInput);
  }

  // This method is responsible for requesting the credential details (password,
  // note). If the user does not authenticate, the page will be redirected to
  // the passwords main page.
  // The method also is disabled when the tab is not visible to the user (e.g: a
  // background tab) so that the native authentication dialog will not be shown.
  private requestCredential_() {
    const credentialId = this.getId_();
    if (credentialId === null || document.visibilityState !== 'visible') {
      return;
    }

    // wrap the id in a PasswordListItemElement:
    const eventDetail = {entry: {id: credentialId}} as unknown as
        PasswordListItemElement;

    this.dispatchEvent(
        new CustomEvent(PASSWORD_VIEW_PAGE_REQUESTED_EVENT_NAME, {
          bubbles: true,
          composed: true,
          detail: eventDetail,
        }));

    recordPasswordViewInteraction(
        PasswordViewPageInteractions.CREDENTIAL_REQUESTED_BY_URL);
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
   * set. If the credential is a federated credential, it shows the federation
   * text.
   */
  private getPasswordOrFederationText_(): string {
    return this.credential?.password || this.credential?.federationText ||
        ' '.repeat(10);
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
    this.showEditDialog_ = true;
  }

  private onEditDialogClosed_() {
    this.showEditDialog_ = false;
  }

  private onSavedPasswordEdited_(event: SavedPasswordEditedEvent) {
    // The dialog is recently closed. Use the new IDs to update the URL.
    const newParams = Router.getInstance().getQueryParameters();
    this.credential = event.detail;
    newParams.set(PasswordViewPageUrlParams.ID, String(event.detail.id));
    Router.getInstance().updateRouteParams(newParams);
  }

  /** Handler for tapping the show/hide button. */
  private onShowPasswordButtonClick_() {
    assert(!this.isFederated_());
    if (this.isPasswordVisible_) {
      this.isPasswordVisible_ = false;
      return;
    }
    recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_SHOW_BUTTON_CLICKED);
    this.isPasswordVisible_ = true;
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
