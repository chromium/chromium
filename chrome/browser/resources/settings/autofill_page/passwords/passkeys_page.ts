// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A settings page that allows the user to see and manage the
    passkeys on their computer.
 */
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '../../settings_page/settings_subpage.js';
import '../../site_favicon.js';
import '../../simple_confirmation_dialog.js';
// <if expr="is_macosx">
import './passkey_edit_dialog.js';

// </if>
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';
import {SettingsViewMixinLit} from '../../settings_page/settings_view_mixin_lit.js';

// <if expr="is_macosx">
import type {SavedPasskeyEditedEvent} from './passkey_edit_dialog.js';
// </if>

import type {Passkey, PasskeysBrowserProxy} from './passkeys_browser_proxy.js';
import {PasskeysBrowserProxyImpl} from './passkeys_browser_proxy.js';
import {getCss} from './passkeys_page.css.js';
import {getHtml} from './passkeys_page.html.js';

export interface SettingsPasskeysPageElement {
  $: {
    deleteErrorDialog: CrLazyRenderLitElement<CrDialogElement>,
    menu: CrActionMenuElement,
  };
}

const SettingsPasskeysPageElementBase = SettingsViewMixinLit(CrLitElement);

export class SettingsPasskeysPageElement extends
    SettingsPasskeysPageElementBase {
  static get is() {
    return 'settings-passkeys-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** Substring to filter the passkeys by. */
      filter_: {type: String},
      passkeys_: {type: Array},
      showDeleteConfirmationDialog_: {type: Boolean},
      noManagement_: {type: Boolean},

      // <if expr="is_macosx">
      showEditDialog_: {type: Boolean},
      username_: {type: String},
      relyingPartyId_: {type: String},
      // </if>
    };
  }

  // <if expr="is_macosx">
  protected accessor showEditDialog_: boolean = false;
  protected accessor username_: string = '';
  protected accessor relyingPartyId_: string = '';
  // </if>

  protected accessor filter_: string = '';
  protected accessor passkeys_: Passkey[] = [];
  protected accessor showDeleteConfirmationDialog_: boolean = false;
  // Set if the current platform doesn't support passkey management.
  // (E.g. Windows prior to 2022H2.)
  protected accessor noManagement_: boolean = false;
  // Contains the credentialId of the passkey that the action menu was opened
  // for.
  private credentialIdForActionMenu_: string|null = null;

  private browserProxy_: PasskeysBrowserProxy =
      PasskeysBrowserProxyImpl.getInstance();

  override firstUpdated() {
    this.browserProxy_.enumerate().then(this.onEnumerateComplete_.bind(this));
  }

  protected onSearchTermChanged_(e: CustomEvent<{value: string}>) {
    this.filter_ = e.detail.value;
  }

  /**
   * Used to filter the displayed passkeys when search text is entered.
   */
  protected getFilteredPasskeys_(): Passkey[] {
    return this.passkeys_.filter(
        passkey => [passkey.relyingPartyId, passkey.userName].some(
            str =>
                str.toLowerCase().includes(this.filter_.trim().toLowerCase())));
  }

  /**
   * Called when the browser has a new list of passkeys.
   */
  private onEnumerateComplete_(passkeys: Passkey[]|null) {
    if (passkeys === null) {
      this.noManagement_ = true;
      this.passkeys_ = [];
      return;
    }

    // `passkeys` will have been sorted already because it's easier to do a
    // Public Suffix List-based sort in C++.
    this.passkeys_ = passkeys;
  }

  protected getIconUrl_(passkey: Passkey): string {
    // `passkey.relyingPartyId` comes from the OS and hopefully can be trusted,
    // but don't let bad data form an unexpected URL. Thus drop any passkeys
    // with characters in the RP ID that are meaningful in a host per
    // https://datatracker.ietf.org/doc/html/rfc1738#section-3.1
    if (['@', ':', '/'].every(c => passkey.relyingPartyId.indexOf(c) === -1)) {
      return 'https://' + passkey.relyingPartyId + '/';
    }

    return '';
  }

  /**
   * Called when the user clicks on the three-dots icon for a passkey.
   */
  protected onDotsClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    this.credentialIdForActionMenu_ = target.dataset['credentialId']!;
    this.$.menu.showAt(target, {
      anchorAlignmentY: AnchorAlignment.AFTER_END,
    });
    // <if expr="is_macosx">
    const existingEntry = this.passkeys_.find(entry => {
      return entry.credentialId === this.credentialIdForActionMenu_;
    })!;
    this.username_ = existingEntry.userName;
    this.relyingPartyId_ = existingEntry.relyingPartyId;
    // </if>
  }

  /**
   * Called when the user clicks to delete a passkey.
   */
  protected onDeleteClick_() {
    assert(this.credentialIdForActionMenu_);
    this.$.menu.close();
    this.showDeleteConfirmationDialog_ = true;
  }

  /**
   * Called when a delete confirmation dialog is closed (whether successful or
   * not).
   */
  protected onConfirmDialogClose_() {
    const dialog =
        this.shadowRoot.querySelector('settings-simple-confirmation-dialog');
    assert(dialog);
    const confirmed = dialog.wasConfirmed();
    this.showDeleteConfirmationDialog_ = false;

    if (confirmed) {
      assert(this.credentialIdForActionMenu_);
      this.browserProxy_.delete(this.credentialIdForActionMenu_)
          .then(this.onDeleteComplete_.bind(
              this, this.credentialIdForActionMenu_));
    }

    this.credentialIdForActionMenu_ = null;
  }

  /**
   * Called when a delete operation has completed.
   */
  private onDeleteComplete_(
      deletedCredentialId: string, newPasskeys: Passkey[]|null) {
    if (newPasskeys !== null &&
        newPasskeys.findIndex(
            (cred) => cred.credentialId === deletedCredentialId) !== -1) {
      // The passkey is still present thus the deletion failed.
      this.$.deleteErrorDialog.get().showModal();
    }
    this.onEnumerateComplete_(newPasskeys);
  }

  /**
   * Called when the user clicks the "ok" button on the error dialog.
   */
  protected onErrorDialogOkClick_() {
    this.$.deleteErrorDialog.get().close();
  }

  /**
   * Returns the a11y label for the "More actions" button next to a passkey.
   */
  protected getMoreActionsLabel_(passkey: Passkey): string {
    return loadTimeData.getStringF(
        'managePasskeysMoreActionsLabel', passkey.userName,
        passkey.relyingPartyId);
  }

  // <if expr="is_macosx">
  protected onEditClick_() {
    this.shadowRoot.querySelector('cr-action-menu')!.close();
    this.showEditDialog_ = true;
  }

  protected onEditDialogClose_() {
    this.showEditDialog_ = false;
  }

  /**
   * Called when an edit operation has completed.
   */
  private onEditComplete_(newPasskeys: Passkey[]|null) {
    this.onEnumerateComplete_(newPasskeys);
  }

  /**
   * Called when the user clicks save in the passkey edit dialog.
   */
  protected onSavedPasskeyEdited_(event: SavedPasskeyEditedEvent) {
    assert(this.credentialIdForActionMenu_);
    this.browserProxy_.edit(this.credentialIdForActionMenu_, event.detail)
        .then(this.onEditComplete_.bind(this));
  }
  // </if>

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

export type PasskeysPageElement = SettingsPasskeysPageElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-passkeys-page': SettingsPasskeysPageElement;
  }
}

customElements.define(
    SettingsPasskeysPageElement.is, SettingsPasskeysPageElement);
