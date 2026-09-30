// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'address-remove-confirmation-dialog' is the dialog that allows
 * removing a saved address.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';

import {getHtml} from './address_remove_confirmation_dialog.html.js';


export interface SettingsAddressRemoveConfirmationDialogElement {
  $: {
    description: HTMLElement,
    cancel: HTMLElement,
    dialog: CrDialogElement,
    remove: HTMLElement,
  };
}

const SettingsAddressRemoveConfirmationDialogBase = I18nMixinLit(CrLitElement);

export class SettingsAddressRemoveConfirmationDialogElement extends
    SettingsAddressRemoveConfirmationDialogBase {
  static get is() {
    return 'settings-address-remove-confirmation-dialog';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      address: {type: Object},
      accountInfo: {type: Object},
    };
  }

  accessor address: chrome.autofillPrivate.AddressEntry = {fields: []};
  accessor accountInfo: chrome.autofillPrivate.AccountInfo|undefined;

  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  protected getConfirmationTitle_(): string {
    if (this.isAccountHomeAddress_()) {
      return this.i18n('removeHomeAddressConfirmationTitle');
    }

    if (this.isAccountWorkAddress_()) {
      return this.i18n('removeWorkAddressConfirmationTitle');
    }

    return this.isAccountNameEmailAddress_() ?
        this.i18n('removeNameEmailAddressConfirmationTitle') :
        this.i18n('removeAddressConfirmationTitle');
  }

  protected getConfirmationDescription_(): TrustedHTML {
    const isAccountAddress = this.address?.metadata?.recordType ===
        chrome.autofillPrivate.AddressRecordType.ACCOUNT;

    if (isAccountAddress) {
      return sanitizeInnerHtml(this.i18n(
          'deleteAccountAddressRecordTypeNotice',
          this.accountInfo?.email || ''));
    }

    if (this.isAccountHomeAddress_()) {
      return sanitizeInnerHtml(loadTimeData.getStringF(
          'deleteHomeAddressNotice',
          loadTimeData.getString('googleAccountHomeAddressUrl'),
          this.accountInfo?.email || ''));
    }

    if (this.isAccountWorkAddress_()) {
      return sanitizeInnerHtml(loadTimeData.getStringF(
          'deleteWorkAddressNotice',
          loadTimeData.getString('googleAccountWorkAddressUrl'),
          this.accountInfo?.email || ''));
    }

    if (this.isAccountNameEmailAddress_()) {
      return sanitizeInnerHtml(loadTimeData.getStringF(
          'deleteNameEmailAddressNotice',
          loadTimeData.getString('googleAccountNameEmailAddressEditUrl'),
          this.accountInfo?.email || ''));
    }

    const isSyncEnabled = !!this.accountInfo?.isSyncEnabledForAutofillProfiles;
    return sanitizeInnerHtml(this.i18n(
        isSyncEnabled ? 'removeSyncAddressConfirmationDescription' :
                        'removeLocalAddressConfirmationDescription'));
  }

  protected getRemoveButtonLabel_(): string {
    return this.isAccountHomeAddress_() || this.isAccountWorkAddress_() ||
            this.isAccountNameEmailAddress_() ?
        this.i18n('removeAddressFromChrome') :
        this.i18n('removeAddress');
  }

  private isAccountHomeAddress_(): boolean {
    return this.address?.metadata?.recordType ===
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_HOME;
  }

  private isAccountWorkAddress_(): boolean {
    return this.address?.metadata?.recordType ===
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_WORK;
  }

  private isAccountNameEmailAddress_(): boolean {
    return this.address?.metadata?.recordType ===
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_NAME_EMAIL;
  }

  protected onRemoveClick(): void {
    this.$.dialog.close();
  }

  protected onCancelClick(): void {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-address-remove-confirmation-dialog':
        SettingsAddressRemoveConfirmationDialogElement;
  }
}

customElements.define(
    SettingsAddressRemoveConfirmationDialogElement.is,
    SettingsAddressRemoveConfirmationDialogElement);
