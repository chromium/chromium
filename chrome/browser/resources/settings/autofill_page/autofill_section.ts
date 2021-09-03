// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-section' is the section containing saved
 * addresses for use in autofill and payments APIs.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared_css.js';
import '../controls/extension_controlled_indicator.js';
import '../controls/settings_toggle_button.js';
import '../prefs/prefs.js';
import './address_edit_dialog.js';
import './address_remove_confirmation_dialog.js';
import './passwords_shared_css.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {SettingsAddressRemoveConfirmationDialogElement} from './address_remove_confirmation_dialog.js';

type PersonalDataChangedListener =
    (addresses: Array<chrome.autofillPrivate.AddressEntry>,
     creditCards: Array<chrome.autofillPrivate.CreditCardEntry>) => void;

/**
 * Interface for all callbacks to the autofill API.
 */
export interface AutofillManager {
  /**
   * Add an observer to the list of personal data.
   */
  setPersonalDataManagerListener(listener: PersonalDataChangedListener): void;

  /**
   * Remove an observer from the list of personal data.
   */
  removePersonalDataManagerListener(listener: PersonalDataChangedListener):
      void;

  /**
   * Request the list of addresses.
   */
  getAddressList(
      callback: (entries: Array<chrome.autofillPrivate.AddressEntry>) => void):
      void;

  /**
   * Saves the given address.
   */
  saveAddress(address: chrome.autofillPrivate.AddressEntry): void;

  /** @param guid The guid of the address to remove.  */
  removeAddress(guid: string): void;
}

/**
 * Implementation that accesses the private API.
 */
export class AutofillManagerImpl implements AutofillManager {
  setPersonalDataManagerListener(listener: PersonalDataChangedListener) {
    chrome.autofillPrivate.onPersonalDataChanged.addListener(listener);
  }

  removePersonalDataManagerListener(listener: PersonalDataChangedListener) {
    chrome.autofillPrivate.onPersonalDataChanged.removeListener(listener);
  }

  getAddressList(
      callback: (entries: Array<chrome.autofillPrivate.AddressEntry>) => void) {
    chrome.autofillPrivate.getAddressList(callback);
  }

  saveAddress(address: chrome.autofillPrivate.AddressEntry) {
    chrome.autofillPrivate.saveAddress(address);
  }

  removeAddress(guid: string) {
    chrome.autofillPrivate.removeEntry(assert(guid));
  }

  static getInstance(): AutofillManager {
    return instance || (instance = new AutofillManagerImpl());
  }

  static setInstance(obj: AutofillManager) {
    instance = obj;
  }
}

let instance: AutofillManager|null = null;

declare global {
  interface HTMLElementEventMap {
    'save-address': CustomEvent<chrome.autofillPrivate.AddressEntry>;
  }
}

interface RepeaterEvent extends CustomEvent {
  model: {
    item: chrome.autofillPrivate.AddressEntry,
  };
}

interface SettingsAutofillSectionElement {
  $: {
    addressSharedMenu: CrActionMenuElement,
    addAddress: HTMLElement,
  };
}

class SettingsAutofillSectionElement extends PolymerElement {
  static get is() {
    return 'settings-autofill-section';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** An array of saved addresses. */
      addresses: Array,

      /** The model for any address related action menus or dialogs. */
      activeAddress: Object,

      showAddressDialog_: Boolean,
      showAddressRemoveConfirmationDialog_: Boolean,
    };
  }

  addresses: Array<chrome.autofillPrivate.AddressEntry>;
  activeAddress: chrome.autofillPrivate.AddressEntry|null;
  private showAddressDialog_: boolean;
  private showAddressRemoveConfirmationDialog_: boolean;
  private activeDialogAnchor_: HTMLElement|null;
  private autofillManager_: AutofillManager = AutofillManagerImpl.getInstance();
  private setPersonalDataListener_: PersonalDataChangedListener|null = null;

  constructor() {
    super();

    /**
     * The element to return focus to, when the currently active dialog is
     * closed.
     */
    this.activeDialogAnchor_ = null;
  }

  ready() {
    super.ready();
    this.addEventListener('save-address', this.saveAddress_);
  }

  connectedCallback() {
    super.connectedCallback();

    // Create listener functions.
    const setAddressesListener =
        (addressList: Array<chrome.autofillPrivate.AddressEntry>) => {
          this.addresses = addressList;
        };

    const setPersonalDataListener: PersonalDataChangedListener =
        (addressList, _cardList) => {
          this.addresses = addressList;
        };

    // Remember the bound reference in order to detach.
    this.setPersonalDataListener_ = setPersonalDataListener;

    // Request initial data.
    this.autofillManager_.getAddressList(setAddressesListener);

    // Listen for changes.
    this.autofillManager_.setPersonalDataManagerListener(
        setPersonalDataListener);

    // Record that the user opened the address settings.
    chrome.metricsPrivate.recordUserAction('AutofillAddressesViewed');
  }

  disconnectedCallback() {
    super.disconnectedCallback();

    this.autofillManager_.removePersonalDataManagerListener(
        this.setPersonalDataListener_!);
    this.setPersonalDataListener_ = null;
  }

  /**
   * Open the address action menu.
   */
  private onAddressMenuTap_(e: RepeaterEvent) {
    const item = e.model.item;

    // Copy item so dialog won't update model on cancel.
    this.activeAddress = Object.assign({}, item);

    const dotsButton = e.target as HTMLElement;
    this.$.addressSharedMenu.showAt(dotsButton);
    this.activeDialogAnchor_ = dotsButton;
  }

  /**
   * Handles tapping on the "Add address" button.
   */
  private onAddAddressTap_(e: Event) {
    e.preventDefault();
    this.activeAddress = {};
    this.showAddressDialog_ = true;
    this.activeDialogAnchor_ = this.$.addAddress;
  }

  private onAddressDialogClose_() {
    this.showAddressDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_!));
    this.activeDialogAnchor_ = null;
  }

  /**
   * Handles tapping on the "Edit" address button.
   */
  private onMenuEditAddressTap_(e: Event) {
    e.preventDefault();
    this.showAddressDialog_ = true;
    this.$.addressSharedMenu.close();
  }

  private onAddressRemoveConfirmationDialogClose_() {
    // Check if the dialog was confirmed before closing it.
    if (this.shadowRoot!
            .querySelector('settings-address-remove-confirmation-dialog')!
            .wasConfirmed()) {
      this.autofillManager_.removeAddress(this.activeAddress!.guid as string);
    }
    this.showAddressRemoveConfirmationDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_!));
    this.activeDialogAnchor_ = null;
  }

  /**
   * Handles tapping on the "Remove" address button.
   */
  private onMenuRemoveAddressTap_() {
    this.showAddressRemoveConfirmationDialog_ = true;
    this.$.addressSharedMenu.close();
  }

  /**
   * @return Whether the list exists and has items.
   */
  private hasSome_(list: Array<Object>): boolean {
    return !!(list && list.length);
  }

  /**
   * Listens for the save-address event, and calls the private API.
   */
  private saveAddress_(event:
                           CustomEvent<chrome.autofillPrivate.AddressEntry>) {
    this.autofillManager_.saveAddress(event.detail);
  }
}

customElements.define(
    SettingsAutofillSectionElement.is, SettingsAutofillSectionElement);
