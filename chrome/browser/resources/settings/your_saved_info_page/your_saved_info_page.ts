// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-your-saved-info-page' is the entry point for users to see
 * and manage their saved info.
 */
import './account_card_element.js';
import './category_reference_card.js';
import './collapsible_autofill_settings_card.js';
import '/shared/settings/prefs/prefs.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EntityTypeName} from '../autofill_ai_enums.mojom-webui.js';
import type {AutofillManagerProxy, PersonalDataChangedListener} from '../autofill_page/autofill_manager_proxy.js';
import {AutofillManagerImpl} from '../autofill_page/autofill_manager_proxy.js';
import type {EntityDataManagerProxy, EntityInstancesChangedListener} from '../autofill_page/entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from '../autofill_page/entity_data_manager_proxy.js';
import {PasswordManagerImpl, PasswordManagerPage} from '../autofill_page/password_manager_proxy.js';
import {PaymentsManagerImpl} from '../autofill_page/payments_manager_proxy.js';
import type {PaymentsManagerProxy} from '../autofill_page/payments_manager_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {SavedInfoHandlerImpl} from './saved_info_handler_proxy.js';
import {getTemplate} from './your_saved_info_page.html.js';

/**
 * A complete set of data chips, organized into categories, with presentation
 * order in the UI.
 */
interface DataTypeHierarchy {
  passwordManager: DataChip[];
  payments: DataChip[];
  contactInfo: DataChip[];
  identityDocs: DataChip[];
  travel: DataChip[];
}

/**
 * Represents a single chip for a saved data type, showing a label, icon,
 * and the number of items.
 */
export interface DataChip {
  type: DataType;
  label: string;
  icon: string;
  // A value of 0 indicates a loaded count of no items,
  // while `undefined` indicates a "not yet loaded" state.
  count?: number;
  // An `undefined` availability indicates it has not yet been determined
  isAvailable?: boolean;
}

/**
 * A specific kind of saved user's information.
 */
enum DataType {
  PASSWORD = 'password',
  PASSKEY = 'passkey',
  CREDIT_CARD = 'creditCard',
  IBAN = 'iban',
  PAY_OVER_TIME_ISSUER = 'payOverTimeIssuer',
  LOYALTY_CARD = 'loyaltyCard',
  ADDRESS = 'address',
  DRIVERS_LICENSE = 'driversLicense',
  NATIONAL_ID_CARD = 'nationalIdCard',
  PASSPORT = 'passport',
  FLIGHT_RESERVATION = 'flightReservation',
  TRAVEL_INFO = 'travelInfo',
  VEHICLE = 'vehicle',
}

const SettingsYourSavedInfoPageElementBase =
    WebUiListenerMixin(SettingsViewMixin(PrefsMixin(I18nMixin(PolymerElement))));

export class SettingsYourSavedInfoPageElement extends
    SettingsYourSavedInfoPageElementBase {
  static get is() {
    return 'settings-your-saved-info-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      hierarchy_: {
        type: Object,
      },
    };
  }

  declare prefs: {[key: string]: any};
  declare private hierarchy_: DataTypeHierarchy;

  private dataTypeToChip_: Map<DataType, DataChip> = new Map();
  private dataTypeToCategory_: Map<DataType, string> = new Map();

  private paymentsManager_: PaymentsManagerProxy =
      PaymentsManagerImpl.getInstance();
  private autofillManager_: AutofillManagerProxy =
      AutofillManagerImpl.getInstance();
  private autofillAiEntityManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();
  private setPersonalDataListener_: PersonalDataChangedListener|null = null;
  private onAutofillAiEntitiesChangedListener_: EntityInstancesChangedListener|
      null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.initializeDataTypeHierarchy_();
    this.setupDataTypeCounters();
  }

  private initializeDataTypeHierarchy_() {
    this.hierarchy_ = {
      passwordManager: [
        {
          type: DataType.PASSWORD,
          label: this.i18n('passwordsLabel'),
          icon: 'cr20:password',
          isAvailable: true,
        },
        {
          type: DataType.PASSKEY,
          label: this.i18n('passkeysLabel'),
          icon: 'settings20:passkey',
          isAvailable: true,
        },
      ],
      payments: [
        {
          type: DataType.CREDIT_CARD,
          label: this.i18n('creditAndDebitCardTitle'),
          icon: 'settings20:credit-card',
          isAvailable: true,
        },
        {
          type: DataType.IBAN,
          label: this.i18n('ibanTitle'),
          icon: 'settings20:iban',
          isAvailable: loadTimeData.getBoolean('showIbansSettings'),
        },
        {
          type: DataType.PAY_OVER_TIME_ISSUER,
          label: this.i18n('autofillPayOverTimeSettingsLabel'),
          icon: 'settings20:hourglass',
          isAvailable: loadTimeData.getBoolean('shouldShowPayOverTimeSettings'),
        },
        {
          type: DataType.LOYALTY_CARD,
          label: this.i18n('loyaltyCardsTitle'),
          icon: 'settings20:loyalty-programs',
          isAvailable: true,
        },
      ],
      contactInfo: [
        {
          type: DataType.ADDRESS,
          label: this.i18n('addresses'),
          icon: 'settings:email',
          isAvailable: true,
        },
      ],
      identityDocs: [
        {
          type: DataType.DRIVERS_LICENSE,
          label: this.i18n('yourSavedInfoDriverLicenseChip'),
          icon: 'settings20:id-card',
          isAvailable: true,
        },
        {
          type: DataType.NATIONAL_ID_CARD,
          label: this.i18n('yourSavedInfoNationalIdsChip'),
          icon: 'settings20:id-card',
          isAvailable: true,
        },
        {
          type: DataType.PASSPORT,
          label: this.i18n('yourSavedInfoPassportChip'),
          icon: 'settings20:passport',
          isAvailable: true,
        },
      ],
      travel: [
        {
          type: DataType.FLIGHT_RESERVATION,
          label: this.i18n('yourSavedInfoFlightReservationsChip'),
          icon: 'settings20:travel',
          isAvailable: true,
        },
        {
          type: DataType.TRAVEL_INFO,
          label: this.i18n('yourSavedInfoTravelInfoChip'),
          icon: 'privacy20:person-check',
          isAvailable: true,
        },
        {
          type: DataType.VEHICLE,
          label: this.i18n('yourSavedInfoVehiclesChip'),
          icon: 'settings20:directions-car',
          isAvailable: true,
        },
      ],
    };

    for (const [categoryName, category] of Object.entries(this.hierarchy_)) {
      for (const chip of category) {
        this.dataTypeToChip_.set(chip.type, chip);
        this.dataTypeToCategory_.set(chip.type, categoryName);
      }
    }
  }

  private setupDataTypeCounters() {
    // Password and passkey counts.
    const setPasswordCount =
        (count: {passwordCount: number, passkeyCount: number}) => {
          this.setChipCount_(DataType.PASSWORD, count.passwordCount);
          this.setChipCount_(DataType.PASSKEY, count.passkeyCount);
        };
    this.addWebUiListener('password-count-changed', setPasswordCount);
    SavedInfoHandlerImpl.getInstance().getPasswordCount().then(
      setPasswordCount);

    // Addresses: Request initial data.
    const setAddressesListener =
        (addresses: chrome.autofillPrivate.AddressEntry[]) => {
          this.setChipCount_(DataType.ADDRESS, addresses.length);
        };
    this.autofillManager_.getAddressList().then(setAddressesListener);

    // Payments: Request initial data.
    const setCreditCardsListener =
        (creditCards: chrome.autofillPrivate.CreditCardEntry[]) => {
          this.setChipCount_(DataType.CREDIT_CARD, creditCards.length);
        };
    const setIbansListener = (ibans: chrome.autofillPrivate.IbanEntry[]) => {
      this.setChipCount_(DataType.IBAN, ibans.length);
    };
    const setPayOverTimeListener =
        (payOverTimeIssuers:
             chrome.autofillPrivate.PayOverTimeIssuerEntry[]) => {
          this.setChipCount_(
              DataType.PAY_OVER_TIME_ISSUER, payOverTimeIssuers.length);
        };
    this.paymentsManager_.getCreditCardList().then(setCreditCardsListener);
    this.paymentsManager_.getIbanList().then(setIbansListener);
    this.paymentsManager_.getPayOverTimeIssuerList().then(
        setPayOverTimeListener);

    // Addresses and Payments: Listen for changes.
    const setPersonalDataListener: PersonalDataChangedListener =
        (addresses: chrome.autofillPrivate.AddressEntry[],
         creditCards: chrome.autofillPrivate.CreditCardEntry[],
         ibans: chrome.autofillPrivate.IbanEntry[],
         payOverTimeIssuers: chrome.autofillPrivate.PayOverTimeIssuerEntry[],
         _accountInfo?: chrome.autofillPrivate.AccountInfo) => {
          this.setChipCount_(DataType.ADDRESS, addresses.length);
          this.setChipCount_(DataType.CREDIT_CARD, creditCards.length);
          this.setChipCount_(DataType.IBAN, ibans.length);
          this.setChipCount_(
              DataType.PAY_OVER_TIME_ISSUER, payOverTimeIssuers.length);
        };
    this.setPersonalDataListener_ = setPersonalDataListener;
    this.autofillManager_.setPersonalDataManagerListener(
      setPersonalDataListener);

    // Autofill AI entities.
    this.onAutofillAiEntitiesChangedListener_ =
        this.onAutofillAiEntitiesChanged.bind(this);
    this.autofillAiEntityManager_.addEntityInstancesChangedListener(
        this.onAutofillAiEntitiesChangedListener_);
    this.autofillAiEntityManager_.loadEntityInstances().then(
        this.onAutofillAiEntitiesChangedListener_);

    // Wallet: Loyalty cards count.
    const setLoyaltyCardsCount = (loyaltyCardsCount?: number) => {
      this.setChipCount_(DataType.LOYALTY_CARD, loyaltyCardsCount);
    };
    this.addWebUiListener('loyalty-cards-count-changed', setLoyaltyCardsCount);
    SavedInfoHandlerImpl.getInstance().getLoyaltyCardsCount().then(
        setLoyaltyCardsCount);
  }

  private onAutofillAiEntitiesChanged(
      entities: chrome.autofillPrivate.EntityInstanceWithLabels[]) {
    const entityCounts = new Map<EntityTypeName, number>();
    for (const entity of entities) {
      const newCount = (entityCounts.get(entity.type.typeName) || 0) + 1;
      entityCounts.set(entity.type.typeName, newCount);
    }
    this.setChipCount_(
        DataType.PASSPORT, entityCounts.get(EntityTypeName.kPassport) ?? 0);
    this.setChipCount_(
        DataType.DRIVERS_LICENSE,
        entityCounts.get(EntityTypeName.kDriversLicense) ?? 0);
    this.setChipCount_(
        DataType.VEHICLE, entityCounts.get(EntityTypeName.kVehicle) ?? 0);
    this.setChipCount_(
        DataType.NATIONAL_ID_CARD,
        entityCounts.get(EntityTypeName.kNationalIdCard) ?? 0);
    this.setChipCount_(
        DataType.TRAVEL_INFO,
        (entityCounts.get(EntityTypeName.kKnownTravelerNumber) ?? 0) +
            (entityCounts.get(EntityTypeName.kRedressNumber) ?? 0));
    this.setChipCount_(
        DataType.FLIGHT_RESERVATION,
        entityCounts.get(EntityTypeName.kFlightReservation) ?? 0);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.setPersonalDataListener_) {
      this.autofillManager_.removePersonalDataManagerListener(
          this.setPersonalDataListener_);
      this.setPersonalDataListener_ = null;
    }

    if (this.onAutofillAiEntitiesChangedListener_) {
      this.autofillAiEntityManager_.removeEntityInstancesChangedListener(
          this.onAutofillAiEntitiesChangedListener_);
      this.onAutofillAiEntitiesChangedListener_ = null;
    }
  }

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    const map = new Map();
    if (routes.PAYMENTS) {
      map.set(routes.PAYMENTS.path, '#paymentManagerButton');
    }
    if (routes.ADDRESSES) {
      map.set(routes.ADDRESSES.path, '#addressesManagerButton');
    }

    return map;
  }

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    const ids = [
      'addresses',
      // <if expr="is_win or is_macosx">
      'passkeys',
      // </if>
      'payments',
    ];
    assert(ids.includes(childViewId));

    let triggerId: string|null = null;
    switch (childViewId) {
      case 'addresses':
        triggerId = 'addressesManagerButton';
        break;
      // <if expr="is_win or is_macosx">
      case 'passkeys':
        triggerId = 'passwordManagerButton';
        break;
      // </if>
      case 'payments':
        triggerId = 'paymentManagerButton';
        break;
      default:
        break;
    }

    assert(triggerId);

    const control =
        this.shadowRoot!.querySelector<HTMLElement>(`#${triggerId}`);
    assert(control);
    return control;
  }

  private setChipCount_(dataType: DataType, count?: number) {
    const chip: DataChip = this.dataTypeToChip_.get(dataType)!;
    const categoryName = this.dataTypeToCategory_.get(dataType)!;
    chip.count = count;
    this.notifyPath(`hierarchy_.${categoryName}`);
  }

  private getVisibleChips_(chips: DataChip[]): DataChip[] {
    return chips.filter(chip => chip.isAvailable).map(chip => ({...chip}));
  }

  /**
   * Shows the manage payment methods sub page.
   */
  private onPaymentManagerClick_() {
    Router.getInstance().navigateTo(routes.PAYMENTS);
  }

  /**
   * Shows the manage addresses sub page.
   */
  private onAddressesManagerClick_() {
    Router.getInstance().navigateTo(routes.ADDRESSES);
  }

  /**
   * Shows the manage identity sub page.
   */
  private onIdentityManagerClick_() {
    Router.getInstance().navigateTo(routes.YOUR_SAVED_INFO_IDENTITY_DOCS);
  }

  /**
   * Shows the manage travel sub page.
   */
  private onTravelManagerClick_() {
    Router.getInstance().navigateTo(routes.YOUR_SAVED_INFO_TRAVEL);
  }

  /**
   * Shows Password Manager page.
   */
  private onPasswordManagerExternalLinkClick_() {
    PasswordManagerImpl.getInstance().recordPasswordsPageAccessInSettings();
    PasswordManagerImpl.getInstance().showPasswordManager(
        PasswordManagerPage.PASSWORDS);
  }

  /**
   * Opens Wallet page in a new tab.
   */
  private onGoogleWalletExternalLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('googleWalletUrl'));
  }

  /**
   * Opens Google Account page in a new tab.
   */
  private onGoogleAccountExternalLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('googleAccountUrl'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-your-saved-info-page': SettingsYourSavedInfoPageElement;
  }
}

customElements.define(
    SettingsYourSavedInfoPageElement.is, SettingsYourSavedInfoPageElement);
