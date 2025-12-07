// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-section' is the section containing saved
 * addresses for use in autofill and payments APIs.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '/shared/settings/prefs/prefs.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import './address_edit_dialog.js';
import './address_remove_confirmation_dialog.js';
import './passwords_shared.css.js';
import './your_saved_info_shared.css.js';

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import type {AutofillManagerProxy, PersonalDataChangedListener} from './autofill_manager_proxy.js';
import {AutofillManagerImpl} from './autofill_manager_proxy.js';
import {getTemplate} from './autofill_section.html.js';


/**
 * The enum values for the Autofill.Address.IsEnabled.Change metric.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
// LINT.IfChange(AutofillAddressOptInChange)
export enum AutofillAddressOptInChange {
  OPT_IN = 0,
  OPT_OUT = 1,

  // Must be last.
  COUNT = 2,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAddressOptInChange)

declare global {
  interface HTMLElementEventMap {
    'save-address': CustomEvent<chrome.autofillPrivate.AddressEntry>;
  }
}

// TODO(crbug.com/447113309): This file along with all of its dependencies
// should be moved to .../settings/your_saved_info_page directory after
// full release of the `Your Saved Info` page.

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

const SettingsAutofillSectionElementBase =
    SettingsViewMixin(I18nMixin(PolymerElement));

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
      prefs: Object,

      accountInfo_: {
        type: Object,
        value: null,
      },

      /** An array of saved addresses. */
      addresses: Array,

      /** The model for any address related action menus or dialogs. */
      activeAddress: Object,

      showAddressDialog_: Boolean,
      showAddressRemoveConfirmationDialog_: Boolean,

      isGoogleProfileAddress: {
        type: Boolean,
        computed: 'computeIsGoogleProfileAddress_(activeAddress)',
      },

      isPlusAddressEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('plusAddressEnabled'),
      },

      /**
       * Indicates if this element is used as a Your saved info subpage. Causes
       * slight adjustments like different title, no page shadow, cards being
       * visible.
       */
      isYourSavedInfoSubpage_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableYourSavedInfoSettingsPage'),
      },
    };
  }

  declare prefs: {[key: string]: any};
  declare addresses: chrome.autofillPrivate.AddressEntry[];
  declare activeAddress: chrome.autofillPrivate.AddressEntry|null;
  declare private accountInfo_: chrome.autofillPrivate.AccountInfo|null;
  declare private showAddressDialog_: boolean;
  declare private showAddressRemoveConfirmationDialog_: boolean;
  declare private isGoogleProfileAddress: boolean;
  declare private isPlusAddressEnabled_: boolean;
  declare private isYourSavedInfoSubpage_: boolean;
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
        (addressList, _cardList, _ibans, _payOverTimeIssuerList,
         accountInfo?) => {
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

  private getMultiCardClass_(): string {
    return this.isYourSavedInfoSubpage_ ? 'multi-card' : '';
  }

  private getPageTitleLabel_(): string {
    return this.i18n(
        this.isYourSavedInfoSubpage_ ? 'contactInfoTitle' : 'addressesTitle');
  }

  /**
   * Returns the text for the remove button in the action menu.
   */
  private getMenuRemoveAddressLabel_(
      address: chrome.autofillPrivate.AddressEntry): string {
    const isGoogleProfileAddress = this.isAccountHomeAddress_(address) ||
        this.isAccountWorkAddress_(address) ||
        this.isAccountNameEmailAddress_(address);

    return this.i18n(
        isGoogleProfileAddress ? 'removeFromChrome' : 'removeAddress');
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
    if (this.isAccountHomeAddress_(this.activeAddress!)) {
      this.onAccountHomeAddressClick_();
    } else if (this.isAccountWorkAddress_(this.activeAddress!)) {
      this.onAccountWorkAddressClick_();
    } else if (this.isAccountNameEmailAddress_(this.activeAddress!)) {
      this.onAccountNameEmailAddressClick_();
    } else {
      this.showAddressDialog_ = true;
    }
    this.$.addressSharedMenu.close();
  }

  private onAddressRemoveConfirmationDialogClose_() {
    // Check if the dialog was confirmed before closing it.
    const wasDeletionConfirmed =
        this.shadowRoot!
            .querySelector(
                'settings-address-remove-confirmation-dialog')!.wasConfirmed();
    const isHomeOrWorkAddress =
        this.isAccountHomeAddress_(this.activeAddress!) ||
        this.isAccountWorkAddress_(this.activeAddress!);
    const recordType = this.activeAddress?.metadata?.recordType;
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
        if (this.activeAddress!.guid === this.addresses[lastIndex].guid) {
          focusWithoutInk(this.$.addressList.querySelectorAll<HTMLElement>(
              '.address-menu')[lastIndex - 1]);
        }
      }

      this.autofillManager_.removeAddress(this.activeAddress!.guid as string);
      if (isHomeOrWorkAddress) {
        getAnnouncerInstance().announce(
            loadTimeData.getString('homeAndWorkAddressRemovedMessage'));
      } else if (this.isAccountNameEmailAddress_(this.activeAddress!)) {
        getAnnouncerInstance().announce(
            loadTimeData.getString('nameEmailAddressRemovedMessage'));
      } else {
        getAnnouncerInstance().announce(
            loadTimeData.getString('addressRemovedMessage'));
      }
    }
    if (recordType) {
      this.recordDeletionMetrics_(wasDeletionConfirmed, recordType);
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

  private isAccountHomeAddress_(address: chrome.autofillPrivate.AddressEntry) {
    return address.metadata?.recordType ===
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_HOME;
  }

  private isAccountWorkAddress_(address: chrome.autofillPrivate.AddressEntry) {
    return address.metadata?.recordType ===
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_WORK;
  }

  private isAccountNameEmailAddress_(
      address: chrome.autofillPrivate.AddressEntry) {
    return address.metadata?.recordType ===
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_NAME_EMAIL;
  }

  private computeIsGoogleProfileAddress_(
      address: chrome.autofillPrivate.AddressEntry): boolean {
    if (!address) {
      return false;
    }

    return this.isAccountHomeAddress_(address) ||
        this.isAccountWorkAddress_(address) ||
        this.isAccountNameEmailAddress_(address);
  }

  private onAccountHomeAddressClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        this.i18n('googleAccountHomeAddressUrl'));
  }

  private onAccountWorkAddressClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        this.i18n('googleAccountWorkAddressUrl'));
  }

  private onAccountNameEmailAddressClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        this.i18n('googleAccountNameEmailAddressEditUrl'));
  }

  private isCloudOffVisible_(
      address: chrome.autofillPrivate.AddressEntry,
      accountInfo: chrome.autofillPrivate.AccountInfo|null): boolean {
    if (address.metadata?.recordType ===
            chrome.autofillPrivate.AddressRecordType.ACCOUNT ||
        address.metadata?.recordType ===
            chrome.autofillPrivate.AddressRecordType.ACCOUNT_HOME ||
        address.metadata?.recordType ===
            chrome.autofillPrivate.AddressRecordType.ACCOUNT_WORK ||
        address.metadata?.recordType ===
            chrome.autofillPrivate.AddressRecordType.ACCOUNT_NAME_EMAIL) {
      return false;
    }

    if (!accountInfo) {
      return false;
    }

    if (accountInfo.isSyncEnabledForAutofillProfiles) {
      return false;
    }

    // Local profile of a logged-in user with disabled address sync and
    // enabled feature.
    return true;
  }

  /**
   * Determines if an icon is to be shown for the given address.
   */
  private shouldShowAddressIcon_(
      address: chrome.autofillPrivate.AddressEntry,
      accountInfo: chrome.autofillPrivate.AccountInfo|null): boolean {
    return this.getAddressIcon_(address, accountInfo).length > 0;
  }

  /**
   * Determines which icon to show for a given address.
   *
   * @return The icon string or an empty string.
   */
  private getAddressIcon_(
      address: chrome.autofillPrivate.AddressEntry,
      accountInfo: chrome.autofillPrivate.AccountInfo|null): string {
    if (loadTimeData.getBoolean('enableSupportForHomeAndWork')) {
      if (this.isAccountHomeAddress_(address)) {
        return 'settings20:home';
      }
      if (this.isAccountWorkAddress_(address)) {
        return 'settings20:work';
      }
    }
    if (this.isCloudOffVisible_(address, accountInfo)) {
      return 'cr20:cloud-off';
    }
    return '';
  }

  /**
   * Determines which a11y string to announce for a given address.
   *
   * @return The a11y string or an empty string.
   */
  private getA11yLabelForIcon_(
      address: chrome.autofillPrivate.AddressEntry,
      accountInfo: chrome.autofillPrivate.AccountInfo|null): string {
    if (this.isCloudOffVisible_(address, accountInfo)) {
      return this.i18n('localAddressIconA11yLabel');
    }
    if (this.isAccountHomeAddress_(address)) {
      return this.i18n('homeAddressIconA11yLabel');
    }
    if (this.isAccountWorkAddress_(address)) {
      return this.i18n('workAddressIconA11yLabel');
    }
    return '';
  }

  /**
   * @returns the title for the More Actions button corresponding to the address
   */
  private moreActionsTitle_(address: chrome.autofillPrivate.AddressEntry):
      string {
    const label = address.metadata?.summaryLabel;
    const subLabel = address.metadata?.summarySublabel;
    const fullLabel = label + (subLabel ?? '');

    let messageKey: string;
    if (this.isAccountHomeAddress_(address)) {
      messageKey = 'moreOptionsForHomeAddress';
    } else if (this.isAccountWorkAddress_(address)) {
      messageKey = 'moreOptionsForWorkAddress';
    } else {
      messageKey = 'moreActionsForAddress';
    }

    return this.i18n(messageKey, fullLabel);
  }

  private isAutofillSyncToggleVisible_(accountInfo:
                                           chrome.autofillPrivate.AccountInfo|
                                       null): boolean {
    return !!(accountInfo?.isAutofillSyncToggleAvailable);
  }

  private getRecordTypeSuffix_(
      recordType: chrome.autofillPrivate.AddressRecordType): string {
    switch (recordType) {
      case chrome.autofillPrivate.AddressRecordType.LOCAL_OR_SYNCABLE:
        return 'LocalOrSyncable';
      case chrome.autofillPrivate.AddressRecordType.ACCOUNT:
        return 'Account';
      case chrome.autofillPrivate.AddressRecordType.ACCOUNT_HOME:
        return 'AccountHome';
      case chrome.autofillPrivate.AddressRecordType.ACCOUNT_WORK:
        return 'AccountWork';
      case chrome.autofillPrivate.AddressRecordType.ACCOUNT_NAME_EMAIL:
        return 'AccountNameEmail';
      default:
        assertNotReached();
    }
  }

  private recordDeletionMetrics_(
      wasDeletionConfirmed: boolean,
      recordType: chrome.autofillPrivate.AddressRecordType) {
    const suffix = this.getRecordTypeSuffix_(recordType);

    chrome.metricsPrivate.recordBoolean(
        'Autofill.ProfileDeleted.Settings.Total', wasDeletionConfirmed);
    chrome.metricsPrivate.recordBoolean(
        'Autofill.ProfileDeleted.Any.Total', wasDeletionConfirmed);
    chrome.metricsPrivate.recordBoolean(
        'Autofill.ProfileDeleted.Settings.' + suffix, wasDeletionConfirmed);
    chrome.metricsPrivate.recordBoolean(
        'Autofill.ProfileDeleted.Any.' + suffix, wasDeletionConfirmed);
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

  private onAutofillProfileToggleChanged_() {
    const value = this.$.autofillProfileToggle.checked ?
        AutofillAddressOptInChange.OPT_IN :
        AutofillAddressOptInChange.OPT_OUT;
    chrome.metricsPrivate.recordEnumerationValue(
        'Autofill.Address.IsEnabled.Change', value,
        AutofillAddressOptInChange.COUNT);
  }

  private onPlusAddressClick_() {
    chrome.metricsPrivate.recordUserAction(
        'Settings.ManageOptionOnSettingsSelected');
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('plusAddressManagementUrl'));
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-section': SettingsAutofillSectionElement;
  }
}

customElements.define(
    SettingsAutofillSectionElement.is, SettingsAutofillSectionElement);
