// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A settings subpage that allows the user to see and manage the
    passkeys on their computer.
 */
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import '../site_favicon.js';
import '../simple_confirmation_dialog.js';
// <if expr="is_macosx">
import './passkey_edit_dialog.js';

// </if>
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

// <if expr="is_macosx">
import type {PasskeyEditDialogElement, SavedPasskeyEditedEvent} from './passkey_edit_dialog.js';
// </if>

import type {Passkey, PasskeysBrowserProxy} from './passkeys_browser_proxy.js';
import {PasskeysBrowserProxyImpl} from './passkeys_browser_proxy.js';
import {getTemplate} from './passkeys_subpage.html.js';

export interface SettingsPasskeysSubpageElement {
  $: {
    deleteErrorDialog: CrLazyRenderElement<CrDialogElement>,
    menu: CrActionMenuElement,
    // <if expr="is_macosx">
    editPasskeyDialog: PasskeyEditDialogElement,
    // </if>
  };
}

export class SettingsPasskeysSubpageElement extends PolymerElement {
  static get is() {
    return 'settings-passkeys-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Substring to filter the passkeys by. */
      filter: {
        type: String,
        value: '',
      },
      // <if expr="is_macosx">
      showEditDialog_: Boolean,
      username_: String,
      relyingPartyId_: String,
      // </if>
    };
  }

  // <if expr="is_macosx">
  private showEditDialog_: boolean;
  private username_: string;
  private relyingPartyId_: string;
  // </if>

  private filter: string;
  private passkeys_: Passkey[];
  private showDeleteConfirmationDialog_: boolean;
  // Set if the current platform doesn't support passkey management.
  // (E.g. Windows prior to 2022H2.)
  private noManagement_: boolean;
  // Contains the credentialId of the passkey that the action menu was opened
  // for.
  private credentialIdForActionMenu_: string|null;

  private browserProxy_: PasskeysBrowserProxy =
      PasskeysBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();
    this.browserProxy_.enumerate().then(this.onEnumerateComplete_.bind(this));
  }

  /**
   * Used to filter the displayed passkeys when search text is entered.
   */
  private filterFunction_(): ((passkey: Passkey) => boolean) {
    return passkey => [passkey.relyingPartyId, passkey.userName].some(
               str => str.toLowerCase().includes(
                   this.filter.trim().toLowerCase()));
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

  private getIconUrl_(passkey: Passkey): string {
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
  private onDotsClick_(e: Event) {
    this.credentialIdForActionMenu_ =
        (e.target as HTMLElement).dataset['credentialId']!;
    this.$.menu.showAt(e.target as HTMLElement, {
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
  private onDeleteClick_() {
    assert(this.credentialIdForActionMenu_);
    this.$.menu.close();
    this.showDeleteConfirmationDialog_ = true;
  }

  /**
   * Called when a delete confirmation dialog is closed (whether successful or
   * not).
   */
  private onConfirmDialogClose_() {
    const dialog =
        this.shadowRoot!.querySelector('settings-simple-confirmation-dialog');
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
  private onErrorDialogOkClick_() {
    this.$.deleteErrorDialog.get().close();
  }

  /**
   * Returns the a11y label for the "More actions" button next to a passkey.
   */
  private getMoreActionsLabel_(passkey: Passkey): string {
    return loadTimeData.getStringF(
        'managePasskeysMoreActionsLabel', passkey.userName,
        passkey.relyingPartyId);
  }

  // <if expr="is_macosx">
  private onEditClick_() {
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    this.showEditDialog_ = true;
  }

  private onEditDialogClose_() {
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
  private onSavedPasskeyEdited_(event: SavedPasskeyEditedEvent) {
    assert(this.credentialIdForActionMenu_);
    this.browserProxy_.edit(this.credentialIdForActionMenu_, event.detail)
        .then(this.onEditComplete_.bind(this));
  }
  // </if>
}
declare global {
  interface HTMLElementTagNameMap {
    'settings-passkeys-subpage': SettingsPasskeysSubpageElement;
  }
}

customElements.define(
    SettingsPasskeysSubpageElement.is, SettingsPasskeysSubpageElement);
