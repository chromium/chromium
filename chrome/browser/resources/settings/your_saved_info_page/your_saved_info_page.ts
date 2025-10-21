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

import type {ChipData} from './category_reference_card.js';
import {SavedInfoHandlerImpl} from './saved_info_handler_proxy.js';
import {getTemplate} from './your_saved_info_page.html.js';

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

      passwordsCardData_: {
        type: Array,
        computed: 'computePasswordsCardData_(passwordsCount, passkeysCount)',
      },
      paymentsCardData_: {
        type: Array,
        computed:
            'computePaymentsCardData_(creditCardsCount, ibansCount, payOverTimeIssuersCount, loyaltyCardsCount, enableIbans_, enablePayOverTime_)',
      },
      addressesCardData_: {
        type: Array,
        computed: 'computeAddressesCardData_(addressesCount)',
      },
      identityCardData_: {
        type: Array,
        computed: 'computeIdentityCardData_(driversLicensesCount, nationalIdCardsCount, passportsCount)',
      },
      travelCardData_: {
        type: Array,
        computed:
            'computeTravelCardData_(flightReservationsCount, travelInfoCount, vehiclesCount)',
      },

      enableIbans_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showIbansSettings');
        },
      },

      enablePayOverTime_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('shouldShowPayOverTimeSettings');
        },
      },

      passwordsCount: Number,
      passkeysCount: Number,
      addressesCount: Number,
      creditCardsCount: Number,
      ibansCount: Number,
      payOverTimeIssuersCount: Number,
      passportsCount: Number,
      driversLicensesCount: Number,
      vehiclesCount: Number,
      nationalIdCardsCount: Number,
      travelInfoCount: Number,
      flightReservationsCount: Number,
      loyaltyCardsCount: Number,
    };
  }

  declare prefs: {[key: string]: any};
  // The counts are initialized to undefined to indicate that the data is not
  // yet loaded. 0 means that the user has no items of that type, while
  // undefined means that we don't know yet.
  declare passwordsCount: number|undefined;
  declare passkeysCount: number|undefined;
  declare addressesCount: number|undefined;
  declare creditCardsCount: number|undefined;
  declare ibansCount: number|undefined;
  declare payOverTimeIssuersCount: number|undefined;
  declare passportsCount: number|undefined;
  declare driversLicensesCount: number|undefined;
  declare vehiclesCount: number|undefined;
  declare nationalIdCardsCount: number|undefined;
  declare travelInfoCount: number|undefined;
  declare flightReservationsCount: number|undefined;
  declare loyaltyCardsCount: number|undefined;

  declare private passwordsCardData_: ChipData[];
  declare private paymentsCardData_: ChipData[];
  declare private addressesCardData_: ChipData[];
  declare private identityCardData_: ChipData[];
  declare private travelCardData_: ChipData[];

  declare private enableIbans_: boolean;
  declare private enablePayOverTime_: boolean;

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
    this.setupDataTypeCounters();
  }

  private setupDataTypeCounters() {
    // Password and passkey counts.
    const setPasswordCount =
        (count: {passwordCount: number, passkeyCount: number}) => {
          this.passwordsCount = count.passwordCount;
          this.passkeysCount = count.passkeyCount;
        };
    this.addWebUiListener('password-count-changed', setPasswordCount);
    SavedInfoHandlerImpl.getInstance().getPasswordCount().then(
      setPasswordCount);

    // Addresses: Request initial data.
    const setAddressesListener =
        (addresses: chrome.autofillPrivate.AddressEntry[]) => {
          this.addressesCount = addresses.length;
        };
    this.autofillManager_.getAddressList().then(setAddressesListener);

    // Payments: Request initial data.
    const setCreditCardsListener =
        (creditCards: chrome.autofillPrivate.CreditCardEntry[]) => {
          this.creditCardsCount = creditCards.length;
        };
    const setIbansListener = (ibans: chrome.autofillPrivate.IbanEntry[]) => {
      this.ibansCount = ibans.length;
    };
    const setPayOverTimeListener =
        (payOverTimeIssuers:
             chrome.autofillPrivate.PayOverTimeIssuerEntry[]) => {
          this.payOverTimeIssuersCount = payOverTimeIssuers.length;
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
          this.addressesCount = addresses.length;
          this.creditCardsCount = creditCards.length;
          this.ibansCount = ibans.length;
          this.payOverTimeIssuersCount = payOverTimeIssuers.length;
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
      this.loyaltyCardsCount = loyaltyCardsCount;
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
    this.passportsCount = entityCounts.get(EntityTypeName.kPassport) ?? 0;
    this.driversLicensesCount =
        entityCounts.get(EntityTypeName.kDriversLicense) ?? 0;
    this.vehiclesCount = entityCounts.get(EntityTypeName.kVehicle) ?? 0;
    this.nationalIdCardsCount =
        entityCounts.get(EntityTypeName.kNationalIdCard) ?? 0;
    this.travelInfoCount =
        (entityCounts.get(EntityTypeName.kKnownTravelerNumber) ?? 0) +
        (entityCounts.get(EntityTypeName.kRedressNumber) ?? 0);
    this.flightReservationsCount =
        entityCounts.get(EntityTypeName.kFlightReservation) ?? 0;
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

  private computePasswordsCardData_(): ChipData[] {
    return [
      {
        label: this.i18n('passwordsLabel'),
        icon: 'cr20:password',
        counter: this.passwordsCount,
      },
      {
        label: this.i18n('passkeysLabel'),
        icon: 'settings20:passkey',
        counter: this.passkeysCount,
      },
    ];
  }

  private computePaymentsCardData_(): ChipData[] {
    const cardData: ChipData[] = [
      {
        label: this.i18n('creditAndDebitCardTitle'),
        icon: 'settings20:credit-card',
        counter: this.creditCardsCount,
      },
    ];

    if (this.enableIbans_) {
      cardData.push({
        label: this.i18n('ibanTitle'),
        icon: 'settings20:iban',
        counter: this.ibansCount,
      });
    }
    if (this.enablePayOverTime_) {
      cardData.push({
        label: this.i18n('autofillPayOverTimeSettingsLabel'),
        icon: 'settings20:hourglass',
        counter: this.payOverTimeIssuersCount,
      });
    }

    cardData.push({
      label: this.i18n('loyaltyCardsTitle'),
      icon: 'settings20:loyalty-programs',
      counter: this.loyaltyCardsCount,
    });
    return cardData;
  }

  private computeAddressesCardData_(): ChipData[] {
    return [
      {
        label: this.i18n('addresses'),
        icon: 'settings:email',
        counter: this.addressesCount,
      },
    ];
  }

  private computeIdentityCardData_(): ChipData[] {
    return [{
      label: this.i18n('yourSavedInfoDriverLicenseChip'),
      icon: 'settings20:id-card',
      counter: this.driversLicensesCount,
    },
    {
      label: this.i18n('yourSavedInfoNationalIdChip'),
      icon: 'settings20:id-card',
      counter: this.nationalIdCardsCount,
    },
    {
      label: this.i18n('yourSavedInfoPassportChip'),
      icon: 'settings20:passport',
      counter: this.passportsCount,
    }];
  }

  private computeTravelCardData_(): ChipData[] {
    return [{
      label: this.i18n('yourSavedInfoFlightReservationsChip'),
      icon: 'firstLevelTopics20:travel',
      counter: this.flightReservationsCount,
    },
    {
      label: this.i18n('yourSavedInfoTravelInfoChip'),
      icon: 'privacy20:person-check',
      counter: this.travelInfoCount,
    },
    {
      label: this.i18n('yourSavedInfoVehiclesChip'),
      icon: 'firstLevelTopics20:directions-car',
      counter: this.vehiclesCount,
    }];
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
    // TODO(crbug.com/438666322): Update routing once the Identity docs subpage is created.
    Router.getInstance().navigateTo(routes.BASIC);
  }

  /**
   * Shows the manage travel sub page.
   */
  private onTravelManagerClick_() {
    // TODO(crbug.com/438666322): Update routing once the Travel subpage is
    // created.
    Router.getInstance().navigateTo(routes.BASIC);
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
