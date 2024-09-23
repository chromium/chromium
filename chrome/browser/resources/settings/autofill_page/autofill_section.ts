// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-section' is the section containing saved
 * addresses for use in autofill and payments APIs.
 */

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared.css.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../controls/settings_toggle_button.js';
import './address_edit_dialog.js';
import './address_remove_confirmation_dialog.js';
import './passwords_shared.css.js';

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';

import type {AutofillManagerProxy, PersonalDataChangedListener} from './autofill_manager_proxy.js';
import {AutofillManagerImpl} from './autofill_manager_proxy.js';
import {getTemplate} from './autofill_section.html.js';

declare global {
  interface HTMLElementEventMap {
    'save-address': CustomEvent<chrome.autofillPrivate.AddressEntry>;
  }
}

export interface SettingsAutofillSectionElement {
  $: {
    autofillProfileToggle: SettingsToggleButtonElement,
    autofillSyncToggleWrapper: HTMLElement,
    autofillSyncToggle: CrToggleElement,
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

      isPlusAddressEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('plusAddressEnabled'),
      },
    };
  }

  prefs: {[key: string]: any};
  addresses: chrome.autofillPrivate.AddressEntry[];
  activeAddress: chrome.autofillPrivate.AddressEntry|null;
  private accountInfo_: chrome.autofillPrivate.AccountInfo|null = null;
  private showAddressDialog_: boolean;
  private showAddressRemoveConfirmationDialog_: boolean;
  private autofillManager_: AutofillManagerProxy =
      AutofillManagerImpl.getInstance();
  private setPersonalDataListener_: PersonalDataChangedListener|null = null;

  override ready() {
    super.ready();
    this.addEventListener('save-address', this.saveAddress_);

    // This is to mimic the behaviour of <settings-toggle-button>.
    this.$.autofillSyncToggleWrapper.addEventListener('click', () => {
      this.$.autofillSyncToggle.click();
    });
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
          this.accountInfo_ = accountInfo || null;
        };
    const setPersonalDataListener: PersonalDataChangedListener =
        (addressList, _cardList, _ibans, accountInfo?) => {
          this.addresses = addressList;
          this.accountInfo_ = accountInfo || null;
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
    const wasDeletionConfirmed =
        this.shadowRoot!
            .querySelector(
                'settings-address-remove-confirmation-dialog')!.wasConfirmed();
    if (wasDeletionConfirmed) {
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
    chrome.metricsPrivate.recordBoolean(
        'Autofill.ProfileDeleted.Settings',
        /*confirmed=*/ wasDeletionConfirmed);
    chrome.metricsPrivate.recordBoolean(
        'Autofill.ProfileDeleted.Any', /*confirmed=*/ wasDeletionConfirmed);
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
      accountInfo: chrome.autofillPrivate.AccountInfo|null): boolean {
    if (address.metadata?.recordType ===
        chrome.autofillPrivate.AddressRecordType.ACCOUNT) {
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

  private isAutofillSyncToggleVisible_(accountInfo:
                                           chrome.autofillPrivate.AccountInfo|
                                       null): boolean {
    return !!(accountInfo?.isAutofillSyncToggleAvailable);
  }

  /**
   * Triggered by settings-toggle-button#autofillSyncToggle. It passes
   * the toggle state to the native code. If the data changed the page
   * content will be refreshed automatically via `PersonalDataChangedListener`.
   */
  private onAutofillSyncEnabledChange_() {
    assert(
        this.accountInfo_ && this.accountInfo_.isAutofillSyncToggleAvailable);
    this.autofillManager_.setAutofillSyncToggleEnabled(
        this.$.autofillSyncToggle.checked);
  }

  private onPlusAddressClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('plusAddressManagementUrl'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-section': SettingsAutofillSectionElement;
  }
}

customElements.define(
    SettingsAutofillSectionElement.is, SettingsAutofillSectionElement);
