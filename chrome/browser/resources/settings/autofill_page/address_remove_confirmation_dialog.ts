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
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './address_remove_confirmation_dialog.html.js';


export interface SettingsAddressRemoveConfirmationDialogElement {
  $: {
    description: HTMLElement,
    cancel: HTMLElement,
    dialog: CrDialogElement,
    remove: HTMLElement,
  };
}

const SettingsAddressRemoveConfirmationDialogBase = I18nMixin(PolymerElement);

export class SettingsAddressRemoveConfirmationDialogElement extends
    SettingsAddressRemoveConfirmationDialogBase {
  static get is() {
    return 'settings-address-remove-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      address: Object,
      accountInfo: Object,

      /**
       * The title of the confirmation dialog.
       */
      confirmationTitle_: {
        type: String,
        computed: 'computeConfirmationTitle_()',
      },

      /**
       * The body of the confirmation dialog.
       */
      confirmationDescription_: {
        type: String,
        computed: 'computeConfirmationDescription_(address, accountInfo)',
      },

      /**
       * The label for the remove button.
       */
      removeButtonLabel_: {
        type: String,
        computed: 'computeRemoveButtonLabel_()',
      },
    };
  }

  declare address: chrome.autofillPrivate.AddressEntry;
  declare accountInfo?: chrome.autofillPrivate.AccountInfo;

  declare private confirmationTitle_: string;
  declare private confirmationDescription_: string;
  declare private removeButtonLabel_: string;

  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  private computeConfirmationTitle_(): string {
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

  private computeConfirmationDescription_(
      address: chrome.autofillPrivate.AddressEntry,
      accountInfo?: chrome.autofillPrivate.AccountInfo): TrustedHTML {
    const isAccountAddress = address?.metadata?.recordType ===
        chrome.autofillPrivate.AddressRecordType.ACCOUNT;

    if (isAccountAddress) {
      return sanitizeInnerHtml(this.i18n(
          'deleteAccountAddressRecordTypeNotice', accountInfo?.email || ''));
    }

    if (this.isAccountHomeAddress_()) {
      return sanitizeInnerHtml(loadTimeData.getStringF(
          'deleteHomeAddressNotice',
          loadTimeData.getString('googleAccountHomeAddressUrl'),
          accountInfo?.email || ''));
    }

    if (this.isAccountWorkAddress_()) {
      return sanitizeInnerHtml(loadTimeData.getStringF(
          'deleteWorkAddressNotice',
          loadTimeData.getString('googleAccountWorkAddressUrl'),
          accountInfo?.email || ''));
    }

    if (this.isAccountNameEmailAddress_()) {
      return sanitizeInnerHtml(loadTimeData.getStringF(
          'deleteNameEmailAddressNotice',
          loadTimeData.getString('googleAccountNameEmailAddressEditUrl'),
          accountInfo?.email || ''));
    }

    const isSyncEnabled = !!accountInfo?.isSyncEnabledForAutofillProfiles;
    return sanitizeInnerHtml(this.i18n(
        isSyncEnabled ? 'removeSyncAddressConfirmationDescription' :
                        'removeLocalAddressConfirmationDescription'));
  }

  private computeRemoveButtonLabel_(): string {
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

  private onRemoveClick() {
    this.$.dialog.close();
  }

  private onCancelClick() {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-address-remove-confirmation-dialog':
        SettingsAddressRemoveConfirmationDialogElement;
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
