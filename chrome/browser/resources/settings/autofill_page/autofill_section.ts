// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-section' is the section containing saved
 * addresses for use in autofill and payments APIs.
 */

import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared.css.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '/shared/settings/controls/settings_toggle_button.js';
import './address_edit_dialog.js';
import './address_remove_confirmation_dialog.js';
import './passwords_shared.css.js';
import '../i18n_setup.js';

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AutofillManagerImpl, AutofillManagerProxy, PersonalDataChangedListener} from './autofill_manager_proxy.js';
import {getTemplate} from './autofill_section.html.js';

declare global {
  interface HTMLElementEventMap {
    'save-address': CustomEvent<chrome.autofillPrivate.AddressEntry>;
  }
}

export interface SettingsAutofillSectionElement {
  $: {
    autofillProfileToggle: SettingsToggleButtonElement,
    addressSharedMenu: CrActionMenuElement,
    addAddress: CrButtonElement,
    addressList: HTMLElement,
    menuEditAddress: HTMLElement,
    menuRemoveAddress: HTMLElement,
    noAddressesLabel: HTMLElement,
  };
}

const SettingsAutofillSectionElementBase = I18nMixin(PolymerElement);

export class SettingsAutofillSectionElement extends
    SettingsAutofillSectionElementBase {
  static get is() {
    return 'settings-autofill-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      accountInfo_: Object,

      /** An array of saved addresses. */
      addresses: Array,

      /** The model for any address related action menus or dialogs. */
      activeAddress: Object,

      showAddressDialog_: Boolean,
      showAddressRemoveConfirmationDialog_: Boolean,
    };
  }

  prefs: {[key: string]: any};
  addresses: chrome.autofillPrivate.AddressEntry[];
  activeAddress: chrome.autofillPrivate.AddressEntry|null;
  private accountInfo_?: chrome.autofillPrivate.AccountInfo;
  private showAddressDialog_: boolean;
  private showAddressRemoveConfirmationDialog_: boolean;
  private autofillManager_: AutofillManagerProxy =
      AutofillManagerImpl.getInstance();
  private setPersonalDataListener_: PersonalDataChangedListener|null = null;

  override ready() {
    super.ready();
    this.addEventListener('save-address', this.saveAddress_);
  }

  override connectedCallback() {
    super.connectedCallback();

    // Create listener functions.
    const setAddressesListener =
        (addressList: chrome.autofillPrivate.AddressEntry[]) => {
          this.addresses = addressList;
        };
    const setAccountListener =
        (accountInfo?: chrome.autofillPrivate.AccountInfo) => {
          this.accountInfo_ = accountInfo;
        };
    const setPersonalDataListener: PersonalDataChangedListener =
        (addressList, _cardList, _ibans, accountInfo?) => {
          this.addresses = addressList;
          this.accountInfo_ = accountInfo;
        };

    // Remember the bound reference in order to detach.
    this.setPersonalDataListener_ = setPersonalDataListener;

    // Request initial data.
    this.autofillManager_.getAddressList().then(setAddressesListener);
    this.autofillManager_.getAccountInfo().then(setAccountListener);

    // Listen for changes.
    this.autofillManager_.setPersonalDataManagerListener(
        setPersonalDataListener);

    // Record that the user opened the address settings.
    chrome.metricsPrivate.recordUserAction('AutofillAddressesViewed');
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.autofillManager_.removePersonalDataManagerListener(
        this.setPersonalDataListener_!);
    this.setPersonalDataListener_ = null;
  }

  /**
   * Open the address action menu.
   */
  private onAddressMenuClick_(
      e: DomRepeatEvent<chrome.autofillPrivate.AddressEntry>) {
    const item = e.model.item;

    // Copy item so dialog won't update model on cancel.
    this.activeAddress = Object.assign({}, item);

    const dotsButton = e.target as HTMLElement;
    this.$.addressSharedMenu.showAt(dotsButton);
  }

  /**
   * Handles tapping on the "Add address" button.
   */
  private onAddAddressClick_(e: Event) {
    e.preventDefault();
    this.activeAddress = {fields: []};
    this.showAddressDialog_ = true;
  }

  private onAddressDialogClose_() {
    this.showAddressDialog_ = false;
  }

  /**
   * Handles tapping on the "Edit" address button.
   */
  private onMenuEditAddressClick_(e: Event) {
    e.preventDefault();
    this.showAddressDialog_ = true;
    this.$.addressSharedMenu.close();
  }

  private onAddressRemoveConfirmationDialogClose_() {
    // Check if the dialog was confirmed before closing it.
    if (this.shadowRoot!
            .querySelector('settings-address-remove-confirmation-dialog')!
            .wasConfirmed()) {
      // Two corner cases are handled:
      // 1. removing the only address: the focus goes to the Add button
      // 2. removing the last address: the focus goes to the previous address
      // In other cases the focus remaining on the same node (reused in
      // subsequently updated address list), but the next address, works fine.
      if (this.addresses.length === 1) {
        focusWithoutInk(this.$.addAddress);
      } else {
        const lastIndex = this.addresses.length - 1;
        if (this.activeAddress!.guid === this.addresses[lastIndex]!.guid) {
          focusWithoutInk(this.$.addressList.querySelectorAll<HTMLElement>(
              '.address-menu')[lastIndex - 1]);
        }
      }

      this.autofillManager_.removeAddress(this.activeAddress!.guid as string);
      getAnnouncerInstance().announce(
          loadTimeData.getString('addressRemovedMessage'));
    }
    this.showAddressRemoveConfirmationDialog_ = false;
  }

  /**
   * Handles tapping on the "Remove" address button.
   */
  private onMenuRemoveAddressClick_() {
    this.showAddressRemoveConfirmationDialog_ = true;
    this.$.addressSharedMenu.close();
  }

  /**
   * @return Whether the list exists and has items.
   */
  private hasSome_(list: Object[]): boolean {
    return !!(list && list.length);
  }

  /**
   * Listens for the save-address event, and calls the private API.
   */
  private saveAddress_(event:
                           CustomEvent<chrome.autofillPrivate.AddressEntry>) {
    this.autofillManager_.saveAddress(event.detail);
  }

  private isCloudOffVisible_(
      address: chrome.autofillPrivate.AddressEntry,
      accountInfo?: chrome.autofillPrivate.AccountInfo): boolean {
    if (address.metadata?.source ===
        chrome.autofillPrivate.AddressSource.ACCOUNT) {
      return false;
    }

    if (!accountInfo) {
      return false;
    }

    if (accountInfo.isSyncEnabledForAutofillProfiles) {
      return false;
    }

    if (!loadTimeData.getBoolean(
            'syncEnableContactInfoDataTypeInTransportMode')) {
      return false;
    }

    // Local profile of a logged-in user with disabled address sync and
    // enabled feature.
    return true;
  }

  /**
   * @returns the title for the More Actions button corresponding to the address
   *     which is described by `label` and `sublabel`.
   */
  private moreActionsTitle_(label: string, sublabel: string) {
    return this.i18n(
        'moreActionsForAddress', label + (sublabel ? sublabel : ''));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-section': SettingsAutofillSectionElement;
  }
}

customElements.define(
    SettingsAutofillSectionElement.is, SettingsAutofillSectionElement);
