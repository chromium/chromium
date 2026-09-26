// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-payments-page' is the page containing saved
 * credit cards for use in autofill and payments APIs.
 */

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_page/settings_subpage.js';
import '../../simple_confirmation_dialog.js';
import './credit_card_edit_dialog.js';
import './iban_edit_dialog.js';
import './payments_list.js';
import './virtual_card_unenroll_dialog.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {loadTimeData} from '../../i18n_setup.js';
import {CvcDeletionUserAction, MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../../metrics_browser_proxy.js';
import {SettingsViewMixinLit} from '../../settings_page/settings_view_mixin_lit.js';
import type {SettingsSimpleConfirmationDialogElement} from '../../simple_confirmation_dialog.js';
import type {PersonalDataChangedListener} from '../autofill_manager_proxy.js';
import {AutofillPolicyDataCategory, computeEffectiveAutofillPref} from '../policy_utils.js';
import type {TypesBlockedEntry} from '../policy_utils.js';

import type {DotsIbanMenuClickEvent, RemoteIbanMenuClickEvent} from './iban_list_entry.js';
import type {SettingsPaymentsListElement} from './payments_list.js';
import type {PaymentsManagerProxy} from './payments_manager_proxy.js';
import {PaymentsManagerImpl} from './payments_manager_proxy.js';
import {getCss} from './payments_page.css.js';
import {getHtml} from './payments_page.html.js';

type DotsCardMenuClickEvent = CustomEvent<{
  creditCard: chrome.autofillPrivate.CreditCardEntry,
  anchorElement: HTMLElement,
}>;

type RemoteCardMenuClickEvent = CustomEvent<{
  creditCard: chrome.autofillPrivate.CreditCardEntry,
  anchorElement: HTMLElement,
}>;

declare global {
  interface HTMLElementEventMap {
    'dots-card-menu-click': DotsCardMenuClickEvent;
    'remote-card-menu-click': RemoteCardMenuClickEvent;
  }
}

export interface SettingsPaymentsPageElement {
  $: {
    autofillCreditCardToggle: SettingsToggleButtonElement,
    canMakePaymentToggle: SettingsToggleButtonElement,
    creditCardSharedMenu: CrActionMenuElement,
    ibanSharedActionMenu: CrLazyRenderLitElement<CrActionMenuElement>,
    manageLink: HTMLElement,
    menuEditCreditCard: HTMLElement,
    menuRemoveCreditCard: HTMLElement,
    menuAddVirtualCard: HTMLElement,
    menuRemoveVirtualCard: HTMLElement,
    paymentMethodsActionMenu: CrLazyRenderLitElement<CrActionMenuElement>,
    paymentsList: SettingsPaymentsListElement,
  };
}

const SettingsPaymentsPageElementBase = PrefServiceObserverMixinLit(
    SettingsViewMixinLit(I18nMixinLit(CrLitElement)));

export class SettingsPaymentsPageElement extends
    SettingsPaymentsPageElementBase {
  static get is() {
    return 'settings-payments-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      creditCardEnabledSyntheticPref_: {type: Object},

      /**
       * An array of all saved credit cards.
       */
      creditCards: {type: Array},

      /**
       * An array of all saved IBANs.
       */
      ibans: {type: Array},

      /**
       * An array of all saved pay over time issuers.
       */
      payOverTimeIssuers: {type: Array},

      /**
       * Whether IBAN is supported in Settings page.
       */
      showIbanSettingsEnabled_: {type: Boolean},

      /**
       * Whether Google Wallet branding should be used instead of Google Pay
       * branding.
       */
      autofillEnableWalletBrandingEnabled_: {type: Boolean},

      /**
       * The model for any credit card-related action menus or dialogs.
       */
      activeCreditCard_: {type: Object},

      /**
       * The model for any IBAN-related action menus or dialogs.
       */
      activeIban_: {type: Object},

      showCreditCardDialog_: {type: Boolean},
      showIbanDialog_: {type: Boolean},
      showLocalCreditCardRemoveConfirmationDialog_: {type: Boolean},
      showLocalIbanRemoveConfirmationDialog_: {type: Boolean},
      showVirtualCardUnenrollDialog_: {type: Boolean},
      showBulkRemoveCvcConfirmationDialog_: {type: Boolean},

      /**
       * Checks if we can use device authentication to authenticate the user.
       */
      // <if expr="is_win or is_macosx or is_chromeos">
      deviceAuthAvailable_: {type: Boolean},
      // </if>

      /**
       * Checks if CVC storage is available based on the feature flag.
       */
      cvcStorageAvailable_: {type: Boolean},

      /**
       * Checks if a mandatory reauth feature flag is enabled.
       */
      mandatoryReauthFeatureFlagEnabled_: {type: Boolean},

      /**
       * Checks if pay over time should be shown from the settings page.
       */
      shouldShowPayOverTimeSettings_: {type: Boolean},

      prefsInitialized_: {type: Boolean},
    };
  }

  accessor creditCards: chrome.autofillPrivate.CreditCardEntry[] = [];
  accessor ibans: chrome.autofillPrivate.IbanEntry[] = [];
  accessor payOverTimeIssuers: chrome.autofillPrivate.PayOverTimeIssuerEntry[] =
      [];
  private accessor showIbanSettingsEnabled_: boolean =
      loadTimeData.getBoolean('showIbansSettings');
  protected accessor creditCardEnabledSyntheticPref_:
      chrome.settingsPrivate.PrefObject<boolean> = {
    key: 'autofill.credit_card_enabled',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: false,
  };
  private accessor autofillEnableWalletBrandingEnabled_: boolean =
      loadTimeData.getBoolean('autofillEnableWalletBranding');
  protected accessor activeCreditCard_: chrome.autofillPrivate.CreditCardEntry|
      null = null;
  protected accessor activeIban_: chrome.autofillPrivate.IbanEntry|null = null;
  protected accessor showCreditCardDialog_: boolean = false;
  protected accessor showIbanDialog_: boolean = false;
  protected accessor showLocalCreditCardRemoveConfirmationDialog_: boolean =
      false;
  protected accessor showLocalIbanRemoveConfirmationDialog_: boolean = false;
  protected accessor showVirtualCardUnenrollDialog_: boolean = false;
  // <if expr="is_win or is_macosx or is_chromeos">
  private accessor deviceAuthAvailable_: boolean =
      loadTimeData.getBoolean('deviceAuthAvailable');
  // </if>
  protected accessor cvcStorageAvailable_: boolean =
      loadTimeData.getBoolean('cvcStorageAvailable');
  protected accessor showBulkRemoveCvcConfirmationDialog_: boolean = false;
  protected accessor shouldShowPayOverTimeSettings_: boolean =
      loadTimeData.getBoolean('shouldShowPayOverTimeSettings');
  protected accessor mandatoryReauthFeatureFlagEnabled_: boolean =
      loadTimeData.getBoolean('mandatoryReauthFeatureFlagEnabled');
  private accessor prefsInitialized_: boolean = false;

  private paymentsManager_: PaymentsManagerProxy =
      PaymentsManagerImpl.getInstance();
  private setPersonalDataListener_: PersonalDataChangedListener|null = null;

  override connectedCallback() {
    this.addPrefObserver(
        'autofill.credit_card_enabled',
        () => this.updateCreditCardEnabledSyntheticPref_());
    this.addPrefObserver(
        'autofill.types_blocked',
        () => this.updateCreditCardEnabledSyntheticPref_());

    super.connectedCallback();

    PrefService.getInstance().whenInitialized().then(() => {
      this.prefsInitialized_ = true;
    });

    // Create listener function.
    const setCreditCardsListener =
        (cardList: chrome.autofillPrivate.CreditCardEntry[]) => {
          this.setCreditCards_(cardList);
        };

    const setPersonalDataListener: PersonalDataChangedListener =
        (_addressList, cardList, ibanList, payOverTimeIssuerList) => {
          this.setCreditCards_(cardList);
          this.ibans = ibanList;
          if (this.shouldShowPayOverTimeSettings_) {
            this.payOverTimeIssuers = payOverTimeIssuerList;
          }
        };

    const setIbansListener = (ibanList: chrome.autofillPrivate.IbanEntry[]) => {
      this.ibans = ibanList;
    };

    // Remember the bound reference in order to detach.
    this.setPersonalDataListener_ = setPersonalDataListener;

    // Request initial data.
    this.paymentsManager_.getCreditCardList().then(setCreditCardsListener);
    this.paymentsManager_.getIbanList().then(setIbansListener);
    if (this.shouldShowPayOverTimeSettings_) {
      this.paymentsManager_.getPayOverTimeIssuerList().then(
          (issuers: chrome.autofillPrivate.PayOverTimeIssuerEntry[]) => {
            this.payOverTimeIssuers = issuers;
          });
    }

    // Listen for changes.
    this.paymentsManager_.setPersonalDataManagerListener(
        setPersonalDataListener);

    // <if expr="is_win or is_macosx or is_chromeos">
    this.paymentsManager_.checkIfDeviceAuthAvailable().then(
        result => this.deviceAuthAvailable_ = result);
    // </if>

    // Record that the user opened the payments settings.
    chrome.metricsPrivate.recordUserAction('AutofillCreditCardsViewed');

    // Measure clicks on the 'Google Account' link for managing payment methods.
    const manageAccountAnchor = this.$.manageLink.querySelector('a');
    if (manageAccountAnchor !== null) {
      manageAccountAnchor.addEventListener('click', () => {
        MetricsBrowserProxyImpl.getInstance().recordAction(
            'Autofill.PaymentMethodsSettingsPage.ManagePaymentMethodsLinkClicked');
      });
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.setPersonalDataListener_);
    this.paymentsManager_.removePersonalDataManagerListener(
        this.setPersonalDataListener_);
    this.setPersonalDataListener_ = null;
  }

  /**
   * Computes a synthetic preference that overlays the wildcard
   * `autofill.types_blocked` enterprise policy onto the user's
   * `autofill.credit_card_enabled` preference.
   */
  private updateCreditCardEnabledSyntheticPref_() {
    const effectivePref = computeEffectiveAutofillPref(
        PrefService.getInstance().getPref<boolean>(
            'autofill.credit_card_enabled'),
        PrefService.getInstance().getPref<TypesBlockedEntry[]>(
            'autofill.types_blocked'),
        AutofillPolicyDataCategory.PAYMENTS);
    assert(effectivePref);
    this.creditCardEnabledSyntheticPref_ = effectivePref;
  }

  /**
   * Handles user toggle changes on the payments autofill toggle. Since the
   * toggle is one-way bound to `creditCardEnabledSyntheticPref_`, we explicitly
   * update the underlying `autofill.credit_card_enabled` pref, guarding against
   * changes when policy is enforced.
   */
  protected onCreditCardToggleSettingsBooleanControlChange_(event: Event) {
    // If the preference is enforced by enterprise policy, do not allow the user
    // to toggle or mutate the underlying preference value.
    if (this.creditCardEnabledSyntheticPref_.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      return;
    }
    const toggle = event.target as SettingsToggleButtonElement;
    PrefService.getInstance().setPrefValue(
        'autofill.credit_card_enabled', toggle.checked);
  }

  private setCreditCards_(cardList: chrome.autofillPrivate.CreditCardEntry[]) {
    this.creditCards = cardList;

    // To align with Android, only record this histogram when the pref is
    // enabled.
    if (this.prefsInitialized_ &&
        (this.creditCardEnabledSyntheticPref_.value ||
         PrefService.getInstance()
             .getPref<boolean>('autofill.credit_card_enabled')
             .value)) {
      MetricsBrowserProxyImpl.getInstance().recordBooleanHistogram(
          'Autofill.PaymentMethodsSettingsPage.CardsViewedWithoutExistingCards',
          this.creditCards.length === 0);
    }
  }

  /**
   * Returns true if IBAN should be shown from settings page.
   * TODO(crbug.com/40234941): Add additional check (starter country-list, or
   * the saved-pref-boolean on if the user has submitted an IBAN form).
   */
  protected shouldShowIbanSettings_(): boolean {
    return this.showIbanSettingsEnabled_;
  }

  /**
   * Opens the dropdown menu to add a credit/debit card or IBAN.
   */
  protected onAddPaymentMethodClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    this.$.paymentMethodsActionMenu.get().showAt(target, {
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }

  /**
   * Opens the credit card action menu.
   */
  protected onDotsCardMenuClick_(e: DotsCardMenuClickEvent) {
    // Copy item so dialog won't update model on cancel.
    this.activeCreditCard_ = e.detail.creditCard;

    this.$.creditCardSharedMenu.showAt(e.detail.anchorElement);
  }

  /**
   * Opens the IBAN action menu.
   */
  protected onDotsIbanMenuClick_(e: DotsIbanMenuClickEvent) {
    // Copy item so dialog won't update model on cancel.
    this.activeIban_ = e.detail.iban;

    this.$.ibanSharedActionMenu.get().showAt(e.detail.anchorElement);
  }

  /**
   * Handles clicking on the "Add credit card" button.
   */
  protected onAddCreditCardClick_(e: Event) {
    e.preventDefault();

    MetricsBrowserProxyImpl.getInstance().recordBooleanHistogram(
        'Autofill.PaymentMethodsSettingsPage.AddCardClickedWithoutExistingCards2',
        this.creditCards.length === 0);

    const date = new Date();  // Default to current month/year.
    const expirationMonth = date.getMonth() + 1;  // Months are 0 based.
    this.activeCreditCard_ = {
      expirationMonth: expirationMonth.toString(),
      expirationYear: date.getFullYear().toString(),
    };
    this.showCreditCardDialog_ = true;
    if (this.showIbanSettingsEnabled_) {
      this.$.paymentMethodsActionMenu.get().close();
    }
  }

  protected onCreditCardDialogClose_() {
    this.showCreditCardDialog_ = false;
    this.activeCreditCard_ = null;
  }

  /**
   * Handles clicking on the add "IBAN" option.
   */
  protected onAddIbanClick_(e: Event) {
    e.preventDefault();
    this.showIbanDialog_ = true;
    this.$.paymentMethodsActionMenu.get().close();
  }

  protected onIbanDialogClose_() {
    this.showIbanDialog_ = false;
    this.activeIban_ = null;
  }

  /**
   * Handles clicking on the "Edit" credit card button.
   */
  protected async onMenuEditCreditCardClick_(e: Event) {
    e.preventDefault();
    assert(this.activeCreditCard_);
    if (this.activeCreditCard_.metadata!.isLocal) {
      const unmaskedCreditCard = await this.paymentsManager_.getLocalCard(
          this.activeCreditCard_.guid!);
      assert(unmaskedCreditCard);
      this.activeCreditCard_ = unmaskedCreditCard;
      this.showCreditCardDialog_ = true;
    } else {
      this.onRemoteCreditCardUrlClick_();
    }

    this.$.creditCardSharedMenu.close();
  }

  protected onRemoteCardMenuClick_(e: RemoteCardMenuClickEvent) {
    this.activeCreditCard_ = e.detail.creditCard;
    this.onRemoteCreditCardUrlClick_();
  }

  private onRemoteCreditCardUrlClick_() {
    this.paymentsManager_.logServerCardLinkClicked();
    const url = new URL(loadTimeData.getString('managePaymentMethodsUrl'));
    assert(this.activeCreditCard_);
    if (this.activeCreditCard_.instrumentId) {
      url.searchParams.append('id', this.activeCreditCard_.instrumentId);
    }
    OpenWindowProxyImpl.getInstance().openUrl(url.toString());
  }

  protected onRemoteIbanMenuClick_(e: RemoteIbanMenuClickEvent) {
    this.activeIban_ = e.detail.iban;
    this.paymentsManager_.logServerIbanLinkClicked();
    const url = new URL(loadTimeData.getString('managePaymentMethodsUrl'));
    assert(this.activeIban_);
    if (this.activeIban_.instrumentId) {
      url.searchParams.append('id', this.activeIban_.instrumentId);
    }
    OpenWindowProxyImpl.getInstance().openUrl(url.toString());
  }

  protected onLocalCreditCardRemoveConfirmationDialogClose_() {
    // Only remove the credit card entry if the user closed the dialog via the
    // confirmation button (instead of cancel or close).
    const confirmationDialog =
        this.shadowRoot.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#localCardDeleteConfirmDialog');
    assert(confirmationDialog);
    if (confirmationDialog.wasConfirmed()) {
      assert(this.activeCreditCard_);
      assert(this.activeCreditCard_.guid);
      const index = this.creditCards.findIndex(
          (card) => card.guid === this.activeCreditCard_!.guid);
      if (!this.$.paymentsList.updateFocusBeforeCreditCardRemoval(index)) {
        this.focusHeaderControls_();
      }
      this.paymentsManager_.removeCreditCard(this.activeCreditCard_.guid);
      this.activeCreditCard_ = null;
    }

    this.showLocalCreditCardRemoveConfirmationDialog_ = false;
  }

  /**
   * Handles clicking on the "Remove" credit card button.
   */
  protected onMenuRemoveCreditCardClick_() {
    this.showLocalCreditCardRemoveConfirmationDialog_ = true;
    this.$.creditCardSharedMenu.close();
  }

  /**
   * Handles clicking on the "Edit" IBAN button.
   */
  protected onMenuEditIbanClick_(e: Event) {
    e.preventDefault();
    this.showIbanDialog_ = true;
    this.$.ibanSharedActionMenu.get().close();
  }

  protected onLocalIbanRemoveConfirmationDialogClose_() {
    // Only remove the IBAN entry if the user closed the dialog via the
    // confirmation button (instead of cancel or close).
    const confirmationDialog =
        this.shadowRoot.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#localIbanDeleteConfirmationDialog');
    assert(confirmationDialog);
    if (confirmationDialog.wasConfirmed()) {
      assert(this.activeIban_);
      assert(this.activeIban_.guid);
      const index =
          this.ibans.findIndex((iban) => iban.guid === this.activeIban_!.guid);
      if (!this.$.paymentsList.updateFocusBeforeIbanRemoval(index)) {
        this.focusHeaderControls_();
      }
      this.paymentsManager_.removeIban(this.activeIban_.guid);
      this.activeIban_ = null;
    }

    this.showLocalIbanRemoveConfirmationDialog_ = false;
  }

  /**
   * Handles clicking on the "Remove" IBAN button.
   */
  protected onMenuRemoveIbanClick_() {
    assert(this.activeIban_);
    this.showLocalIbanRemoveConfirmationDialog_ = true;
    this.$.ibanSharedActionMenu.get().close();
  }

  protected onMenuAddVirtualCardClick_() {
    this.paymentsManager_.addVirtualCard(this.activeCreditCard_!.guid!);
    this.$.creditCardSharedMenu.close();
    this.activeCreditCard_ = null;
  }

  protected onMenuRemoveVirtualCardClick_() {
    this.showVirtualCardUnenrollDialog_ = true;
    this.$.creditCardSharedMenu.close();
  }

  protected onVirtualCardUnenrollDialogClose_() {
    this.showVirtualCardUnenrollDialog_ = false;
    this.activeCreditCard_ = null;
  }

  /**
   * Records changes made to the "Allow sites to check if you have payment
   * methods saved" setting to a histogram.
   */
  protected onCanMakePaymentSettingsBooleanControlChange_() {
    MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
        PrivacyElementInteractions.PAYMENT_METHOD);
  }

  /**
   * Listens for the save-credit-card event, and calls the private API.
   */
  protected onSaveCreditCard_(
      event: CustomEvent<chrome.autofillPrivate.CreditCardEntry>) {
    this.paymentsManager_.saveCreditCard(event.detail);
  }

  protected onSaveIban_(event: CustomEvent<chrome.autofillPrivate.IbanEntry>) {
    this.paymentsManager_.saveIban(event.detail);
  }

  protected getMenuEditCardText_(): string {
    if (this.activeCreditCard_?.metadata?.isLocal) {
      return this.i18n('edit');
    }
    return this.i18n(
        this.autofillEnableWalletBrandingEnabled_ ? 'editServerCardInWallet' :
                                                    'editServerCard');
  }

  protected shouldShowAddVirtualCardButton_(): boolean {
    if (this.activeCreditCard_ === null || !this.activeCreditCard_.metadata) {
      return false;
    }
    return !!this.activeCreditCard_.metadata.isVirtualCardEnrollmentEligible &&
        !this.activeCreditCard_.metadata.isVirtualCardEnrolled;
  }

  protected shouldShowRemoveVirtualCardButton_(): boolean {
    if (this.activeCreditCard_ === null || !this.activeCreditCard_.metadata) {
      return false;
    }
    return !!this.activeCreditCard_.metadata.isVirtualCardEnrollmentEligible &&
        !!this.activeCreditCard_.metadata.isVirtualCardEnrolled;
  }

  /**
   * Listens for the unenroll-virtual-card event, and calls the private API.
   */
  protected onUnenrollVirtualCard_(event: CustomEvent<string>) {
    this.paymentsManager_.removeVirtualCard(event.detail);
  }

  // <if expr="is_win or is_macosx or is_chromeos">
  /**
   * Checks if we should disable the mandatory reauth toggle.
   * This method checks that one of the following conditions are met:
   * 1) Pref autofill.credit_card_enabled is false
   * 2) There is no support for device authentication
   * Under any of these circumstances, we should display a disabled mandatory
   * re-auth toggle to the user.
   */
  protected shouldDisableAuthToggle_(): boolean {
    if (!this.prefsInitialized_) {
      return true;
    }

    const creditCardEnabled = this.creditCardEnabledSyntheticPref_.value ||
        PrefService.getInstance()
            .getPref<boolean>('autofill.credit_card_enabled')
            .value;
    return !creditCardEnabled || !this.deviceAuthAvailable_;
  }
  // </if>

  private focusHeaderControls_(): void {
    const element =
        this.shadowRoot.querySelector<HTMLElement>('.header-aligned-button');
    if (element) {
      focusWithoutInk(element);
    }
  }

  /**
   * Checks for user auth before flipping the mandatory auth toggle.
   */
  protected onMandatoryAuthToggleSettingsBooleanControlChange_(e: Event) {
    const mandatoryAuthToggle = e.target as SettingsToggleButtonElement;
    assert(mandatoryAuthToggle);
    // The toggle is reset to the value when it was clicked.
    // It will be flipped afterwards if the user auth is successful.
    mandatoryAuthToggle.checked = !mandatoryAuthToggle.checked;
    this.paymentsManager_.authenticateUserAndFlipMandatoryAuthToggle();
  }

  /**
   * Method to handle the clicking of bulk delete all the CVCs.
   */
  protected onCvcStorageSubLabelLinkClicked_() {
    assert(this.cvcStorageAvailable_);
    // Log the metric for user clicking on the bulk delete hyperlink which
    // triggers the dialog window.
    MetricsBrowserProxyImpl.getInstance().recordAction(
        CvcDeletionUserAction.HYPERLINK_CLICKED);
    this.showBulkRemoveCvcConfirmationDialog_ = true;
  }

  /**
   * Method to bulk delete all the CVCs present on the local DB.
   */
  protected onShowBulkRemoveCvcConfirmationDialogClose_() {
    assert(this.cvcStorageAvailable_);
    const confirmationDialog =
        this.shadowRoot.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#bulkDeleteCvcConfirmDialog');
    assert(confirmationDialog);

    // Log the metric for user either clicking on "Delete" or "Cancel" on the
    // bulk delete dialog window.
    MetricsBrowserProxyImpl.getInstance().recordAction(
        confirmationDialog.wasConfirmed() ?
            CvcDeletionUserAction.DIALOG_ACCEPTED :
            CvcDeletionUserAction.DIALOG_CANCELLED);
    if (confirmationDialog.wasConfirmed()) {
      this.paymentsManager_.bulkDeleteAllCvcs();
    }
    this.showBulkRemoveCvcConfirmationDialog_ = false;

    // Focus on the CVC storage toggle, post deletion of CVCs for voice reader
    // correctness.
    const cvcStorageToggle =
        this.shadowRoot.querySelector<SettingsToggleButtonElement>(
            '#cvcStorageToggle');
    assert(cvcStorageToggle);
    cvcStorageToggle.focus();
  }

  /**
   * Method to return the correct sublabel for the cvc storage toggle.
   * If any card from the list has a cvc, the sublabel with bulk delete
   * hyperlink is returned else return the regular sublabel.
   * @returns Cvc storage toggle sublabel string.
   */
  protected getCvcStorageSublabel_(): string {
    const card = this.creditCards.find(cc => !!cc.cvc);
    return loadTimeData.getStringF(
        card === undefined ? 'enableCvcStorageSublabel' :
                             'enableCvcStorageDeleteDataSublabel');
  }

  /**
   * Opens an article to learn about card benefits when the card benefits toggle
   * sublabel link is clicked.
   */
  protected onCardBenefitsSubLabelLinkClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('cardBenefitsToggleLearnMoreUrl'));
  }

  /**
   * Get the CVC storage toggle aria label for a11y voice readers.
   * @returns CVC storage aria label.
   */
  protected getCvcStorageAriaLabel_(): string {
    const card = this.creditCards.find(cc => !!cc.cvc);
    return this.i18n(
        card === undefined ? 'enableCvcStorageAriaLabelForNoCvcSaved' :
                             'enableCvcStorageLabel');
  }

  /**
   * Get the body text for the CVC deletion dialog, depending on whether Google
   * Wallet branding is enabled or not.
   */
  protected getCvcDeletionDialogBodyText_(): string {
    return this.i18n(
        this.autofillEnableWalletBrandingEnabled_ ?
            'bulkRemoveCvcFromWalletConfirmationDescription' :
            'bulkRemoveCvcConfirmationDescription');
  }

  /**
   * Opens an article to learn about pay over time when the pay over time
   * toggle sublabel link is clicked.
   */
  protected onPayOverTimeSubLabelLinkClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('autofillPayOverTimeSettingsLearnMoreUrl'));
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-payments-page': SettingsPaymentsPageElement;
  }
}

customElements.define(
    SettingsPaymentsPageElement.is, SettingsPaymentsPageElement);
