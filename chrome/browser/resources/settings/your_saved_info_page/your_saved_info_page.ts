// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-your-saved-info-page' is the entry point for users to see
 * and manage their saved info.
 */
import './account_card.js';
import './category_reference_card.js';
import './collapsible_autofill_settings_card.js';
import '/shared/settings/prefs/prefs.js';
import '../settings_page/settings_section.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';

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
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, YourSavedInfoDataCategory, YourSavedInfoDataChip, YourSavedInfoRelatedService} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import type {DataCategoryClickEvent, DataChipClickEvent} from './category_reference_card.js';
import {SavedInfoHandlerImpl} from './saved_info_handler_proxy.js';
import {getTemplate} from './your_saved_info_page.html.js';

type AddressEntry = chrome.autofillPrivate.AddressEntry;
type CreditCardEntry = chrome.autofillPrivate.CreditCardEntry;
type IbanEntry = chrome.autofillPrivate.IbanEntry;
type PayOverTimeIssuerEntry = chrome.autofillPrivate.PayOverTimeIssuerEntry;
type AccountInfo = chrome.autofillPrivate.AccountInfo;
type EntityType = chrome.autofillPrivate.EntityType;
type EntityInstanceWithLabels = chrome.autofillPrivate.EntityInstanceWithLabels;

/**
 * A complete set of data chips, organized into categories, with presentation
 * order in the UI.
 */
interface DataTypeHierarchy {
  passwordManager: DataCategory;
  payments: DataCategory;
  contactInfo: DataCategory;
  identityDocs: DataCategory;
  travel: DataCategory;
}

interface DataCategory {
  id: YourSavedInfoDataCategory;
  chips: DataChip[];
}

/**
 * Represents a single chip for a saved data type, showing a label, icon,
 * and the number of items.
 */
export interface DataChip {
  id: YourSavedInfoDataChip;
  label: string;
  icon: string;
  // A value of 0 indicates a loaded count of no items,
  // while `undefined` indicates a "not yet loaded" state.
  count?: number;
  // A function determining whether the chip is available or not
  computeAvailability: () => boolean;
}

const SettingsYourSavedInfoPageElementBase = WebUiListenerMixin(
    SettingsViewMixin(PrefsMixin(I18nMixin(PolymerElement))));

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

  private dataChipIdToChip_: Map<YourSavedInfoDataChip, DataChip> = new Map();
  private dataChipIdToCategory_: Map<YourSavedInfoDataChip, DataCategory> =
      new Map();
  private dataChipIdToCategoryName_: Map<YourSavedInfoDataChip, string> =
      new Map();
  private availableAutofillAiTypes_: Set<EntityTypeName> = new Set();

  private paymentsManager_: PaymentsManagerProxy =
      PaymentsManagerImpl.getInstance();
  private autofillManager_: AutofillManagerProxy =
      AutofillManagerImpl.getInstance();
  private autofillAiEntityManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
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
      passwordManager: {
        id: YourSavedInfoDataCategory.PASSWORD_MANAGER,
        chips: [
          {
            id: YourSavedInfoDataChip.PASSWORDS,
            label: this.i18n('passwordsLabel'),
            icon: 'cr20:password',
            computeAvailability: () => true,
          },
          {
            id: YourSavedInfoDataChip.PASSKEYS,
            label: this.i18n('passkeysLabel'),
            icon: 'settings20:passkey',
            computeAvailability: () => true,
          },
        ],
      },
      payments: {
        id: YourSavedInfoDataCategory.PAYMENTS,
        chips: [
          {
            id: YourSavedInfoDataChip.CREDIT_CARDS,
            label: this.i18n('creditAndDebitCardTitle'),
            icon: 'settings20:credit-card',
            computeAvailability: () => true,
          },
          {
            id: YourSavedInfoDataChip.IBANS,
            label: this.i18n('ibanTitle'),
            icon: 'settings20:iban',
            computeAvailability: () =>
                loadTimeData.getBoolean('showIbansSettings'),
          },
          {
            id: YourSavedInfoDataChip.PAY_OVER_TIME,
            label: this.i18n('autofillPayOverTimeSettingsLabel'),
            icon: 'settings20:hourglass',
            computeAvailability: () =>
                loadTimeData.getBoolean('shouldShowPayOverTimeSettings'),
          },
          {
            id: YourSavedInfoDataChip.LOYALTY_CARDS,
            label: this.i18n('loyaltyCardsTitle'),
            icon: 'settings20:loyalty-programs',
            computeAvailability: () =>
                loadTimeData.getBoolean('enableLoyaltyCardsFilling'),
          },
        ],
      },
      contactInfo: {
        id: YourSavedInfoDataCategory.CONTACT_INFO,
        chips: [
          {
            id: YourSavedInfoDataChip.ADDRESSES,
            label: this.i18n('addresses'),
            icon: 'settings:email',
            computeAvailability: () => true,
          },
        ],
      },
      identityDocs: {
        id: YourSavedInfoDataCategory.IDENTITY_DOCS,
        chips: [
          {
            id: YourSavedInfoDataChip.DRIVERS_LICENSES,
            label: this.i18n('yourSavedInfoDriverLicenseChip'),
            icon: 'settings20:id-card',
            computeAvailability: () => this.availableAutofillAiTypes_.has(
                EntityTypeName.kDriversLicense),
          },
          {
            id: YourSavedInfoDataChip.NATIONAL_ID_CARDS,
            label: this.i18n('yourSavedInfoNationalIdsChip'),
            icon: 'settings20:id-card',
            computeAvailability: () => this.availableAutofillAiTypes_.has(
                EntityTypeName.kNationalIdCard),
          },
          {
            id: YourSavedInfoDataChip.PASSPORTS,
            label: this.i18n('yourSavedInfoPassportChip'),
            icon: 'settings20:passport',
            computeAvailability: () =>
                this.availableAutofillAiTypes_.has(EntityTypeName.kPassport),
          },
        ],
      },
      travel: {
        id: YourSavedInfoDataCategory.TRAVEL,
        chips: [
          {
            id: YourSavedInfoDataChip.FLIGHT_RESERVATIONS,
            label: this.i18n('yourSavedInfoFlightReservationsChip'),
            icon: 'settings20:travel',
            computeAvailability: () => this.availableAutofillAiTypes_.has(
                EntityTypeName.kFlightReservation),
          },
          {
            id: YourSavedInfoDataChip.TRAVEL_INFO,
            label: this.i18n('yourSavedInfoTravelInfoChip'),
            icon: 'privacy20:person-check',
            computeAvailability: () =>
                this.availableAutofillAiTypes_.has(
                    EntityTypeName.kKnownTravelerNumber) ||
                this.availableAutofillAiTypes_.has(
                    EntityTypeName.kRedressNumber),
          },
          {
            id: YourSavedInfoDataChip.VEHICLES,
            label: this.i18n('yourSavedInfoVehiclesChip'),
            icon: 'settings20:directions-car',
            computeAvailability: () =>
                this.availableAutofillAiTypes_.has(EntityTypeName.kVehicle),
          },
        ],
      },
    };

    for (const [categoryName, category] of Object.entries(this.hierarchy_)) {
      for (const chip of category.chips) {
        this.dataChipIdToChip_.set(chip.id, chip);
        this.dataChipIdToCategory_.set(chip.id, category);
        this.dataChipIdToCategoryName_.set(chip.id, categoryName);
      }
    }
  }

  private setupDataTypeCounters() {
    // Password and passkey counts.
    const setPasswordCount =
        (count: {passwordCount: number, passkeyCount: number}) => {
          this.setChipCount_(
              YourSavedInfoDataChip.PASSWORDS, count.passwordCount);
          this.setChipCount_(
              YourSavedInfoDataChip.PASSKEYS, count.passkeyCount);
        };
    this.addWebUiListener('password-count-changed', setPasswordCount);
    SavedInfoHandlerImpl.getInstance().getPasswordCount().then(
        setPasswordCount);

    // Addresses: Request initial data.
    const setAddressesListener = (addresses: AddressEntry[]) => {
      this.setChipCount_(YourSavedInfoDataChip.ADDRESSES, addresses.length);
    };
    this.autofillManager_.getAddressList().then(setAddressesListener);

    // Payments: Request initial data.
    const setCreditCardsListener = (creditCards: CreditCardEntry[]) => {
      this.setChipCount_(
          YourSavedInfoDataChip.CREDIT_CARDS, creditCards.length);
    };
    const setIbansListener = (ibans: IbanEntry[]) => {
      this.setChipCount_(YourSavedInfoDataChip.IBANS, ibans.length);
    };
    const setPayOverTimeListener =
        (payOverTimeIssuers: PayOverTimeIssuerEntry[]) => {
          this.setChipCount_(
              YourSavedInfoDataChip.PAY_OVER_TIME, payOverTimeIssuers.length);
        };
    this.paymentsManager_.getCreditCardList().then(setCreditCardsListener);
    this.paymentsManager_.getIbanList().then(setIbansListener);
    this.paymentsManager_.getPayOverTimeIssuerList().then(
        setPayOverTimeListener);

    // Addresses and Payments: Listen for changes.
    const setPersonalDataListener: PersonalDataChangedListener =
        (addresses: AddressEntry[], creditCards: CreditCardEntry[],
         ibans: IbanEntry[], payOverTimeIssuers: PayOverTimeIssuerEntry[],
         _accountInfo?: AccountInfo) => {
          this.setChipCount_(YourSavedInfoDataChip.ADDRESSES, addresses.length);
          this.setChipCount_(
              YourSavedInfoDataChip.CREDIT_CARDS, creditCards.length);
          this.setChipCount_(YourSavedInfoDataChip.IBANS, ibans.length);
          this.setChipCount_(
              YourSavedInfoDataChip.PAY_OVER_TIME, payOverTimeIssuers.length);
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

    this.autofillAiEntityManager_.getWritableEntityTypes().then(
        (entityTypes: EntityType[]) => {
          for (const entityType of entityTypes) {
            this.availableAutofillAiTypes_.add(entityType.typeName);
          }
          this.notifyPath('hierarchy_.identityDocs.chips');
          this.notifyPath('hierarchy_.travel.chips');
        });

    // Wallet: Loyalty cards count.
    const setLoyaltyCardsCount = (loyaltyCardsCount?: number) => {
      this.setChipCount_(
          YourSavedInfoDataChip.LOYALTY_CARDS, loyaltyCardsCount);
    };
    this.addWebUiListener('loyalty-cards-count-changed', setLoyaltyCardsCount);
    SavedInfoHandlerImpl.getInstance().getLoyaltyCardsCount().then(
        setLoyaltyCardsCount);
  }

  private onAutofillAiEntitiesChanged(entities: EntityInstanceWithLabels[]) {
    const entityCounts = new Map<EntityTypeName, number>();
    for (const entity of entities) {
      const newCount = (entityCounts.get(entity.type.typeName) || 0) + 1;
      entityCounts.set(entity.type.typeName, newCount);
    }
    this.setChipCount_(
        YourSavedInfoDataChip.PASSPORTS,
        entityCounts.get(EntityTypeName.kPassport) ?? 0);
    this.setChipCount_(
        YourSavedInfoDataChip.DRIVERS_LICENSES,
        entityCounts.get(EntityTypeName.kDriversLicense) ?? 0);
    this.setChipCount_(
        YourSavedInfoDataChip.VEHICLES,
        entityCounts.get(EntityTypeName.kVehicle) ?? 0);
    this.setChipCount_(
        YourSavedInfoDataChip.NATIONAL_ID_CARDS,
        entityCounts.get(EntityTypeName.kNationalIdCard) ?? 0);
    this.setChipCount_(
        YourSavedInfoDataChip.TRAVEL_INFO,
        (entityCounts.get(EntityTypeName.kKnownTravelerNumber) ?? 0) +
            (entityCounts.get(EntityTypeName.kRedressNumber) ?? 0));
    this.setChipCount_(
        YourSavedInfoDataChip.FLIGHT_RESERVATIONS,
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
    if (routes.YOUR_SAVED_INFO_CONTACT_INFO) {
      map.set(
          routes.YOUR_SAVED_INFO_CONTACT_INFO.path, '#addressesManagerButton');
    }
    if (routes.YOUR_SAVED_INFO_IDENTITY_DOCS) {
      map.set(
          routes.YOUR_SAVED_INFO_IDENTITY_DOCS.path, '#identityManagerButton');
    }
    if (routes.YOUR_SAVED_INFO_TRAVEL) {
      map.set(routes.YOUR_SAVED_INFO_TRAVEL.path, '#travelManagerButton');
    }
    return map;
  }

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    let triggerId: string;
    switch (childViewId) {
      case 'contactInfo':
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
      case 'identityDocs':
        triggerId = 'identityManagerButton';
        break;
      case 'travel':
        triggerId = 'travelManagerButton';
        break;
      default:
        throw new Error(`Unrecognized child view ID: ${childViewId}`);
    }
    const control =
        this.shadowRoot!.querySelector<HTMLElement>(`#${triggerId}`);
    assert(control);
    return control;
  }

  private setChipCount_(chipId: YourSavedInfoDataChip, count?: number) {
    const chip: DataChip = this.dataChipIdToChip_.get(chipId)!;
    const categoryName = this.dataChipIdToCategoryName_.get(chipId)!;
    chip.count = count;
    this.notifyPath(`hierarchy_.${categoryName}.chips`);
  }

  private getVisibleChips_(chips: DataChip[]): DataChip[] {
    return chips.filter(chip => chip.computeAvailability() || !!chip.count)
        .map(chip => ({...chip}));
  }

  private hasVisibleChips_(chips: DataChip[]): boolean {
    return chips.some(chip => chip.computeAvailability() || !!chip.count);
  }

  private onDataCategoryClick_(e: DataCategoryClickEvent) {
    const categoryId: YourSavedInfoDataCategory = e.detail.categoryId;
    this.metricsBrowserProxy_.recordYourSavedInfoCategoryClick(categoryId);
    this.navigateToLeafPage_(categoryId);
  }

  private onDataChipClick_(e: DataChipClickEvent) {
    const chipId: YourSavedInfoDataChip = e.detail.chipId;
    const category: DataCategory = this.dataChipIdToCategory_.get(chipId)!;
    this.metricsBrowserProxy_.recordYourSavedInfoDataChipClick(chipId);
    this.navigateToLeafPage_(category.id);
  }

  /**
   * Navigate to the settings sub page corresponding to a data category.
   */
  private navigateToLeafPage_(categoryId: YourSavedInfoDataCategory) {
    switch (categoryId) {
      case YourSavedInfoDataCategory.PASSWORD_MANAGER:
        PasswordManagerImpl.getInstance().recordPasswordsPageAccessInSettings();
        PasswordManagerImpl.getInstance().showPasswordManager(
            PasswordManagerPage.PASSWORDS);
        break;
      case YourSavedInfoDataCategory.PAYMENTS:
        Router.getInstance().navigateTo(routes.PAYMENTS);
        break;
      case YourSavedInfoDataCategory.CONTACT_INFO:
        Router.getInstance().navigateTo(routes.YOUR_SAVED_INFO_CONTACT_INFO);
        break;
      case YourSavedInfoDataCategory.IDENTITY_DOCS:
        Router.getInstance().navigateTo(routes.YOUR_SAVED_INFO_IDENTITY_DOCS);
        break;
      case YourSavedInfoDataCategory.TRAVEL:
        Router.getInstance().navigateTo(routes.YOUR_SAVED_INFO_TRAVEL);
        break;
    }
  }

  /**
   * Opens Password Manager page on clicking a related service link.
   */
  private onPasswordManagerRelatedServiceClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoRelatedServiceClick(
        YourSavedInfoRelatedService.GOOGLE_PASSWORD_MANAGER);
    PasswordManagerImpl.getInstance().recordPasswordsPageAccessInSettings();
    PasswordManagerImpl.getInstance().showPasswordManager(
        PasswordManagerPage.PASSWORDS);
  }

  /**
   * Opens Wallet page in a new tab.
   */
  private onGoogleWalletRelatedServiceClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoRelatedServiceClick(
        YourSavedInfoRelatedService.GOOGLE_WALLET);
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('googleWalletUrl'));
  }

  /**
   * Opens Google Account page in a new tab.
   */
  private onGoogleAccountRelatedServiceClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoRelatedServiceClick(
        YourSavedInfoRelatedService.GOOGLE_ACCOUNT);
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
