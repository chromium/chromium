// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-autofill-page-v2' is the entry point for users to see
 * and manage their saved info when SettingsRefresh2026 is enabled.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './account_card.js';
import './collapsible_autofill_settings_card.js';
import '../settings_page/settings_section.js';
import '../icons.html.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';

// </if>

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {EntityTypeName} from '../autofill_ai_enums.mojom-webui.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SuggestionsFromGeminiEntryPoint, YourSavedInfoDataCategory, YourSavedInfoRelatedService} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import type {AutofillManagerProxy, PersonalDataChangedListener} from './autofill_manager_proxy.js';
import {AutofillManagerImpl} from './autofill_manager_proxy.js';
import {getCss} from './autofill_page_v2.css.js';
import {getHtml} from './autofill_page_v2.html.js';
import type {EntityDataManagerProxy, EntityInstancesChangedListener} from './entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';
import {PasswordManagerImpl, PasswordManagerPage} from './passwords/password_manager_proxy.js';
import {PaymentsManagerImpl} from './payments/payments_manager_proxy.js';
import type {PaymentsManagerProxy} from './payments/payments_manager_proxy.js';
import {DataManagementSurvey, SavedInfoHandlerImpl} from './saved_info_handler_proxy.js';

type AddressEntry = chrome.autofillPrivate.AddressEntry;
type CreditCardEntry = chrome.autofillPrivate.CreditCardEntry;
type IbanEntry = chrome.autofillPrivate.IbanEntry;
type PayOverTimeIssuerEntry = chrome.autofillPrivate.PayOverTimeIssuerEntry;
type AccountInfo = chrome.autofillPrivate.AccountInfo;
type EntityInstanceWithLabels = chrome.autofillPrivate.EntityInstanceWithLabels;

const SettingsAutofillPageV2ElementBase =
    SettingsViewMixinLit(WebUiListenerMixinLit(CrLitElement));

export class SettingsAutofillPageV2Element extends
    SettingsAutofillPageV2ElementBase {
  static get is() {
    return 'settings-autofill-page-v2';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      passwordManagerCount_: {type: Number},
      paymentsCount_: {type: Number},
      contactInfoCount_: {type: Number},
      identityDocsCount_: {type: Number},
      travelCount_: {type: Number},
      shoppingCount_: {type: Number},
      isShoppingEnabled_: {type: Boolean},
      isOffersAndLoyaltyCardsEnabled_: {type: Boolean},
      showSuggestionsFromGeminiSettings_: {type: Boolean},
      spark_: {type: String},
    };
  }

  protected accessor passwordManagerCount_: number|undefined;
  protected accessor paymentsCount_: number|undefined;
  protected accessor contactInfoCount_: number|undefined;
  protected accessor identityDocsCount_: number|undefined;
  protected accessor travelCount_: number|undefined;
  protected accessor shoppingCount_: number|undefined;
  protected accessor isShoppingEnabled_: boolean =
      loadTimeData.getBoolean('shoppingIntegrationEnabled');
  protected accessor isOffersAndLoyaltyCardsEnabled_: boolean =
      loadTimeData.getBoolean('offersAndLoyaltyCardsSettingsEnabled');
  protected accessor showSuggestionsFromGeminiSettings_: boolean =
      loadTimeData.getBoolean('showSuggestionsFromGeminiSettings');
  // <if expr="not _google_chrome">
  protected accessor spark_: string = 'settings20:lightbulb';
  // </if>
  // <if expr="_google_chrome">
  protected accessor spark_: string =
      loadTimeData.getBoolean('glicAssetsV2Enabled') ?
      'settings-internal:sparkv2' :
      'settings-internal:spark';
  // </if>

  private creditCardCount_?: number;
  private ibanCount_?: number;
  private payOverTimeCount_?: number;
  private loyaltyCardsCount_?: number;

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
    this.setupDataTypeCounters_();
  }

  private setupDataTypeCounters_() {
    // Password and passkey counts.
    const setPasswordCount =
        (count: {passwordCount: number, passkeyCount: number}) => {
          this.passwordManagerCount_ = count.passwordCount + count.passkeyCount;
        };
    this.addWebUiListener('password-count-changed', setPasswordCount);
    SavedInfoHandlerImpl.getInstance().getPasswordCount().then(
        setPasswordCount);

    // Addresses: Request initial data.
    const setAddressesListener = (addresses: AddressEntry[]) => {
      this.contactInfoCount_ = addresses.length;
    };
    this.autofillManager_.getAddressList().then(setAddressesListener);

    // Payments: Request initial data.
    const setCreditCardsListener = (creditCards: CreditCardEntry[]) => {
      this.creditCardCount_ = creditCards.length;
      this.updatePaymentsCount_();
    };
    const setIbansListener = (ibans: IbanEntry[]) => {
      this.ibanCount_ = ibans.length;
      this.updatePaymentsCount_();
    };
    const setPayOverTimeListener =
        (payOverTimeIssuers: PayOverTimeIssuerEntry[]) => {
          this.payOverTimeCount_ = payOverTimeIssuers.length;
          this.updatePaymentsCount_();
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
          this.contactInfoCount_ = addresses.length;
          this.creditCardCount_ = creditCards.length;
          this.ibanCount_ = ibans.length;
          this.payOverTimeCount_ = payOverTimeIssuers.length;
          this.updatePaymentsCount_();
        };

    this.setPersonalDataListener_ = setPersonalDataListener;
    this.autofillManager_.setPersonalDataManagerListener(
        setPersonalDataListener);

    // Autofill AI entities.
    this.onAutofillAiEntitiesChangedListener_ =
        this.onAutofillAiEntitiesChanged_.bind(this);
    this.autofillAiEntityManager_.addEntityInstancesChangedListener(
        this.onAutofillAiEntitiesChangedListener_);
    this.autofillAiEntityManager_.loadEntityInstances().then(
        this.onAutofillAiEntitiesChangedListener_);

    // Wallet: Loyalty cards count.
    const setLoyaltyCardsCount = (loyaltyCardsCount?: number) => {
      this.loyaltyCardsCount_ = loyaltyCardsCount;
      this.updatePaymentsCount_();
    };
    this.addWebUiListener('loyalty-cards-count-changed', setLoyaltyCardsCount);
    SavedInfoHandlerImpl.getInstance().getLoyaltyCardsCount().then(
        setLoyaltyCardsCount);
  }

  private updatePaymentsCount_() {
    if (this.creditCardCount_ === undefined && this.ibanCount_ === undefined &&
        this.payOverTimeCount_ === undefined &&
        this.loyaltyCardsCount_ === undefined) {
      this.paymentsCount_ = undefined;
      return;
    }

    this.paymentsCount_ = (this.creditCardCount_ ?? 0) +
        (this.ibanCount_ ?? 0) + (this.payOverTimeCount_ ?? 0) +
        (this.loyaltyCardsCount_ ?? 0);
  }

  private onAutofillAiEntitiesChanged_(entities: EntityInstanceWithLabels[]) {
    const entityCounts = new Map<EntityTypeName, number>();
    for (const entity of entities) {
      const newCount = (entityCounts.get(entity.type.typeName) || 0) + 1;
      entityCounts.set(entity.type.typeName, newCount);
    }
    this.identityDocsCount_ =
        (entityCounts.get(EntityTypeName.kPassport) ?? 0) +
        (entityCounts.get(EntityTypeName.kDriversLicense) ?? 0) +
        (entityCounts.get(EntityTypeName.kNationalIdCard) ?? 0);
    this.travelCount_ =
        (entityCounts.get(EntityTypeName.kFlightReservation) ?? 0) +
        (entityCounts.get(EntityTypeName.kKnownTravelerNumber) ?? 0) +
        (entityCounts.get(EntityTypeName.kRedressNumber) ?? 0) +
        (entityCounts.get(EntityTypeName.kVehicle) ?? 0);
    this.shoppingCount_ = (entityCounts.get(EntityTypeName.kOrder) ?? 0) +
        (entityCounts.get(EntityTypeName.kShipment) ?? 0);
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
    if (routes.CONTACT_INFO) {
      map.set(routes.CONTACT_INFO.path, '#addressesManagerButton');
    }
    if (routes.IDENTITY_DOCS) {
      map.set(routes.IDENTITY_DOCS.path, '#identityManagerButton');
    }
    if (routes.TRAVEL) {
      map.set(routes.TRAVEL.path, '#travelManagerButton');
    }
    if (routes.OFFERS_AND_LOYALTY_CARDS) {
      map.set(
          routes.OFFERS_AND_LOYALTY_CARDS.path,
          '#offersAndLoyaltyCardsManagerButton');
    }
    if (routes.SHOPPING) {
      map.set(routes.SHOPPING.path, '#shoppingManagerButton');
    }
    if (routes.SUGGESTIONS_FROM_GEMINI) {
      map.set(
          routes.SUGGESTIONS_FROM_GEMINI.path, '#suggestionsFromGeminiLinkRow');
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
      case 'offersAndLoyaltyCards':
        triggerId = 'offersAndLoyaltyCardsManagerButton';
        break;
      case 'shopping':
        triggerId = 'shoppingManagerButton';
        break;
      case 'suggestionsFromGemini':
        triggerId = 'suggestionsFromGeminiLinkRow';
        break;
      default:
        assertNotReached(`Unrecognized child view ID: ${childViewId}`);
    }
    const control = this.shadowRoot.querySelector<HTMLElement>(`#${triggerId}`);
    assert(
        control,
        `Failed to find associated control for child '${childViewId}'`);
    return control;
  }

  protected onPasswordManagerClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoCategoryClick(
        YourSavedInfoDataCategory.PASSWORD_MANAGER);
    PasswordManagerImpl.getInstance().recordPasswordsPageAccessInSettings();
    PasswordManagerImpl.getInstance().showPasswordManager(
        PasswordManagerPage.PASSWORDS);
    SavedInfoHandlerImpl.getInstance().requestDataManagementSurvey(
        DataManagementSurvey.PASSWORDS, true);
  }

  protected onPaymentsClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoCategoryClick(
        YourSavedInfoDataCategory.PAYMENTS);
    Router.getInstance().navigateTo(routes.PAYMENTS);
  }

  protected onContactInfoClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoCategoryClick(
        YourSavedInfoDataCategory.CONTACT_INFO);
    Router.getInstance().navigateTo(routes.CONTACT_INFO);
  }

  protected onIdentityDocsClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoCategoryClick(
        YourSavedInfoDataCategory.IDENTITY_DOCS);
    Router.getInstance().navigateTo(routes.IDENTITY_DOCS);
  }

  protected onTravelClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoCategoryClick(
        YourSavedInfoDataCategory.TRAVEL);
    Router.getInstance().navigateTo(routes.TRAVEL);
  }

  protected onOffersAndLoyaltyCardsClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoCategoryClick(
        YourSavedInfoDataCategory.OFFERS_AND_LOYALTY_CARDS);
    Router.getInstance().navigateTo(routes.OFFERS_AND_LOYALTY_CARDS);
  }

  protected onShoppingClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoCategoryClick(
        YourSavedInfoDataCategory.SHOPPING);
    Router.getInstance().navigateTo(routes.SHOPPING);
  }

  protected onSuggestionsFromGeminiClick_() {
    this.metricsBrowserProxy_.recordSuggestionsFromGeminiEntryPointClick(
        SuggestionsFromGeminiEntryPoint.YOUR_SAVED_INFO);
    Router.getInstance().navigateTo(routes.SUGGESTIONS_FROM_GEMINI);
  }

  /**
   * Opens Password Manager page on clicking a related service link.
   */
  protected onPasswordManagerRelatedServiceClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoRelatedServiceClick(
        YourSavedInfoRelatedService.GOOGLE_PASSWORD_MANAGER);
    PasswordManagerImpl.getInstance().recordPasswordsPageAccessInSettings();
    PasswordManagerImpl.getInstance().showPasswordManager(
        PasswordManagerPage.PASSWORDS);
    SavedInfoHandlerImpl.getInstance().requestDataManagementSurvey(
        DataManagementSurvey.PASSWORDS, true);
  }

  /**
   * Opens Wallet page in a new tab.
   */
  protected onGoogleWalletRelatedServiceClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoRelatedServiceClick(
        YourSavedInfoRelatedService.GOOGLE_WALLET);
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('googleWalletUrl'));
  }

  /**
   * Opens Google Account page in a new tab.
   */
  protected onGoogleAccountRelatedServiceClick_() {
    this.metricsBrowserProxy_.recordYourSavedInfoRelatedServiceClick(
        YourSavedInfoRelatedService.GOOGLE_ACCOUNT);
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('googleAccountUrl'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-page-v2': SettingsAutofillPageV2Element;
  }
}

customElements.define(
    SettingsAutofillPageV2Element.is, SettingsAutofillPageV2Element);
