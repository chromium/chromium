// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-payments-section' is the section containing saved
 * credit cards for use in autofill and payments APIs.
 */

import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared.css.js';
import '/shared/settings/controls/settings_toggle_button.js';
import './credit_card_edit_dialog.js';
import './iban_edit_dialog.js';
import '../simple_confirmation_dialog.js';
import './passwords_shared.css.js';
import './payments_list.js';
import './virtual_card_unenroll_dialog.js';

import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {AnchorAlignment, CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {SettingsSimpleConfirmationDialogElement} from '../simple_confirmation_dialog.js';

import {PersonalDataChangedListener} from './autofill_manager_proxy.js';
import {DotsIbanMenuClickEvent} from './iban_list_entry.js';
import {SettingsPaymentsListElement} from './payments_list.js';
import {PaymentsManagerImpl, PaymentsManagerProxy} from './payments_manager_proxy.js';
import {getTemplate} from './payments_section.html.js';

type DotsCardMenuiClickEvent = CustomEvent<{
  creditCard: chrome.autofillPrivate.CreditCardEntry,
  anchorElement: HTMLElement,
}>;

declare global {
  interface HTMLElementEventMap {
    'dots-card-menu-click': DotsCardMenuiClickEvent;
  }
}

export interface SettingsPaymentsSectionElement {
  $: {
    autofillCreditCardToggle: SettingsToggleButtonElement,
    canMakePaymentToggle: SettingsToggleButtonElement,
    creditCardSharedMenu: CrActionMenuElement,
    ibanSharedActionMenu: CrLazyRenderElement<CrActionMenuElement>,
    mandatoryAuthToggle: SettingsToggleButtonElement,
    menuClearCreditCard: HTMLElement,
    menuEditCreditCard: HTMLElement,
    menuRemoveCreditCard: HTMLElement,
    menuAddVirtualCard: HTMLElement,
    menuRemoveVirtualCard: HTMLElement,
    migrateCreditCards: HTMLElement,
    paymentsList: SettingsPaymentsListElement,
  };
}

const SettingsPaymentsSectionElementBase = I18nMixin(PolymerElement);

export class SettingsPaymentsSectionElement extends
    SettingsPaymentsSectionElementBase {
  static get is() {
    return 'settings-payments-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      /**
       * An array of all saved credit cards.
       */
      creditCards: {
        type: Array,
        value: () => [],
      },

      /**
       * An array of all saved IBANs.
       */
      ibans: {
        type: Array,
        value: () => [],
      },

      /**
       * An array of all saved UPI IDs.
       */
      upiIds: {
        type: Array,
        value: () => [],
      },

      /**
       * Set to true if user can be verified through FIDO authentication.
       */
      userIsFidoVerifiable_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'fidoAuthenticationAvailableForAutofill');
        },
      },

      /**
       * Whether IBAN is supported in Settings page.
       */
      showIbanSettingsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showIbansSettings');
        },
        readOnly: true,
      },

      /**
       * The model for any credit card-related action menus or dialogs.
       */
      activeCreditCard_: Object,

      /**
       * The model for any IBAN-related action menus or dialogs.
       */
      activeIban_: Object,

      showCreditCardDialog_: Boolean,
      showIbanDialog_: Boolean,
      showLocalCreditCardRemoveConfirmationDialog_: Boolean,
      showLocalIbanRemoveConfirmationDialog_: Boolean,
      showVirtualCardUnenrollDialog_: Boolean,
      migratableCreditCardsInfo_: String,

      /**
       * Whether migration local card on settings page is enabled.
       */
      migrationEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('migrationEnabled');
        },
        readOnly: true,
      },

      /**
       * Whether the removal of Expiration and Type titles on settings page
       * is enabled.
       */
      removeCardExpirationAndTypeTitlesEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('removeCardExpirationAndTypeTitles');
        },
        readOnly: true,
      },

      /**
       * Whether virtual card enroll management on settings page is enabled.
       */
      virtualCardEnrollmentEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('virtualCardEnrollmentEnabled');
        },
        readOnly: true,
      },

      /**
       * Checks if we can use device authentication to authenticate the user.
       */
      // <if expr="is_win or is_macosx">
      deviceAuthAvailable_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('deviceAuthAvailable');
        },
      },
      // </if>
    };
  }

  prefs: {[key: string]: any};
  creditCards: chrome.autofillPrivate.CreditCardEntry[];
  ibans: chrome.autofillPrivate.IbanEntry[];
  upiIds: string[];
  private showIbanSettingsEnabled_: boolean;
  private userIsFidoVerifiable_: boolean;
  private activeCreditCard_: chrome.autofillPrivate.CreditCardEntry|null;
  private activeIban_: chrome.autofillPrivate.IbanEntry|null;
  private showCreditCardDialog_: boolean;
  private showIbanDialog_: boolean;
  private showLocalCreditCardRemoveConfirmationDialog_: boolean;
  private showLocalIbanRemoveConfirmationDialog_: boolean;
  private showVirtualCardUnenrollDialog_: boolean;
  private migratableCreditCardsInfo_: string;
  private migrationEnabled_: boolean;
  private removeCardExpirationAndTypeTitlesEnabled_: boolean;
  private virtualCardEnrollmentEnabled_: boolean;
  private deviceAuthAvailable_: boolean;
  private paymentsManager_: PaymentsManagerProxy =
      PaymentsManagerImpl.getInstance();
  private setPersonalDataListener_: PersonalDataChangedListener|null = null;

  override connectedCallback() {
    super.connectedCallback();

    // Create listener function.
    const setCreditCardsListener =
        (cardList: chrome.autofillPrivate.CreditCardEntry[]) => {
          this.creditCards = cardList;
        };

    // Update |userIsFidoVerifiable_| based on the availability of a platform
    // authenticator.
    this.paymentsManager_.isUserVerifyingPlatformAuthenticatorAvailable().then(
        r => {
          if (r === null) {
            return;
          }

          this.userIsFidoVerifiable_ = this.userIsFidoVerifiable_ && r;
        });

    const setPersonalDataListener: PersonalDataChangedListener =
        (_addressList, cardList, ibanList) => {
          this.creditCards = cardList;
          this.ibans = ibanList;
        };

    const setIbansListener = (ibanList: chrome.autofillPrivate.IbanEntry[]) => {
      this.ibans = ibanList;
    };

    const setUpiIdsListener = (upiIdList: string[]) => {
      this.upiIds = upiIdList;
    };

    // Remember the bound reference in order to detach.
    this.setPersonalDataListener_ = setPersonalDataListener;

    // Request initial data.
    this.paymentsManager_.getCreditCardList().then(setCreditCardsListener);
    this.paymentsManager_.getIbanList().then(setIbansListener);
    this.paymentsManager_.getUpiIdList().then(setUpiIdsListener);

    // Listen for changes.
    this.paymentsManager_.setPersonalDataManagerListener(
        setPersonalDataListener);

    // Record that the user opened the payments settings.
    chrome.metricsPrivate.recordUserAction('AutofillCreditCardsViewed');
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.paymentsManager_.removePersonalDataManagerListener(
        this.setPersonalDataListener_!);
    this.setPersonalDataListener_ = null;
  }

  /**
   * Calculate the class style for `paymentsList` based on flags.
   */
  private computeCssClass_(): string {
    return this.removeCardExpirationAndTypeTitlesEnabled_ ?
        'payment-list-margin-start' :
        '';
  }

  /**
   * Returns true if IBAN should be shown from settings page.
   * TODO(crbug.com/1352606): Add additional check (starter country-list, or
   * the saved-pref-boolean on if the user has submitted an IBAN form).
   */
  private shouldShowIbanSettings_(): boolean {
    return this.showIbanSettingsEnabled_;
  }

  /**
   * Opens the dropdown menu to add a credit/debit card or IBAN.
   */
  private onAddPaymentMethodClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const menu = this.shadowRoot!
                     .querySelector<CrLazyRenderElement<CrActionMenuElement>>(
                         '#paymentMethodsActionMenu')!.get();
    assert(menu);
    menu.showAt(target, {
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }

  /**
   * Opens the credit card action menu.
   */
  private onCreditCardDotsMenuClick_(e: DotsCardMenuiClickEvent) {
    // Copy item so dialog won't update model on cancel.
    this.activeCreditCard_ = e.detail.creditCard;

    this.$.creditCardSharedMenu.showAt(e.detail.anchorElement);
  }

  /**
   * Opens the IBAN action menu.
   */
  private onDotsIbanMenuClick_(e: DotsIbanMenuClickEvent) {
    // Copy item so dialog won't update model on cancel.
    this.activeIban_ = e.detail.iban;

    this.$.ibanSharedActionMenu.get().showAt(e.detail.anchorElement);
  }

  /**
   * Handles clicking on the "Add credit card" button.
   */
  private onAddCreditCardClick_(e: Event) {
    e.preventDefault();
    const date = new Date();  // Default to current month/year.
    const expirationMonth = date.getMonth() + 1;  // Months are 0 based.
    this.activeCreditCard_ = {
      expirationMonth: expirationMonth.toString(),
      expirationYear: date.getFullYear().toString(),
    };
    this.showCreditCardDialog_ = true;
    if (this.showIbanSettingsEnabled_) {
      const menu = this.shadowRoot!
                       .querySelector<CrLazyRenderElement<CrActionMenuElement>>(
                           '#paymentMethodsActionMenu')!.get();
      assert(menu);
      menu.close();
    }
  }

  private onCreditCardDialogClose_() {
    this.showCreditCardDialog_ = false;
    this.activeCreditCard_ = null;
  }

  /**
   * Handles clicking on the add "IBAN" option.
   */
  private onAddIbanClick_(e: Event) {
    e.preventDefault();
    this.showIbanDialog_ = true;
    const menu = this.shadowRoot!
                     .querySelector<CrLazyRenderElement<CrActionMenuElement>>(
                         '#paymentMethodsActionMenu')!.get();
    assert(menu);
    menu.close();
  }

  private onIbanDialogClose_() {
    this.showIbanDialog_ = false;
    this.activeIban_ = null;
  }

  /**
   * Handles clicking on the "Edit" credit card button.
   */
  private async onMenuEditCreditCardClick_(e: Event) {
    e.preventDefault();

    if (this.activeCreditCard_!.metadata!.isLocal) {
      this.showCreditCardDialog_ =
          await this.paymentsManager_.authenticateUserToEditLocalCard();
    } else {
      this.onRemoteEditCreditCardClick_();
    }

    this.$.creditCardSharedMenu.close();
  }

  private onRemoteEditCreditCardClick_() {
    this.paymentsManager_.logServerCardLinkClicked();
    window.open(loadTimeData.getString('manageCreditCardsUrl'));
  }

  private onLocalCreditCardRemoveConfirmationDialogClose_() {
    // Only remove the credit card entry if the user closed the dialog via the
    // confirmation button (instead of cancel or close).
    const confirmationDialog =
        this.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
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
  private onMenuRemoveCreditCardClick_() {
    this.showLocalCreditCardRemoveConfirmationDialog_ = true;
    this.$.creditCardSharedMenu.close();
  }

  /**
   * Handles clicking on the "Edit" IBAN button.
   */
  private onMenuEditIbanClick_(e: Event) {
    e.preventDefault();
    this.showIbanDialog_ = true;
    this.$.ibanSharedActionMenu.get().close();
  }

  private onLocalIbanRemoveConfirmationDialogClose_() {
    // Only remove the IBAN entry if the user closed the dialog via the
    // confirmation button (instead of cancel or close).
    const confirmationDialog =
        this.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
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
  private onMenuRemoveIbanClick_() {
    assert(this.activeIban_);
    this.showLocalIbanRemoveConfirmationDialog_ = true;
    this.$.ibanSharedActionMenu.get().close();
  }

  /**
   * Handles clicking on the "Clear copy" button for cached credit cards.
   */
  private onMenuClearCreditCardClick_() {
    this.paymentsManager_.clearCachedCreditCard(this.activeCreditCard_!.guid!);
    this.$.creditCardSharedMenu.close();
    this.activeCreditCard_ = null;
  }


  private onMenuAddVirtualCardClick_() {
    this.paymentsManager_.addVirtualCard(this.activeCreditCard_!.guid!);
    this.$.creditCardSharedMenu.close();
    this.activeCreditCard_ = null;
  }

  private onMenuRemoveVirtualCardClick_() {
    this.showVirtualCardUnenrollDialog_ = true;
    this.$.creditCardSharedMenu.close();
  }

  private onVirtualCardUnenrollDialogClose_() {
    this.showVirtualCardUnenrollDialog_ = false;
    this.activeCreditCard_ = null;
  }

  /**
   * Handles clicking on the "Migrate" button for migrate local credit
   * cards.
   */
  private onMigrateCreditCardsClick_() {
    this.paymentsManager_.migrateCreditCards();
  }

  /**
   * Records changes made to the "Allow sites to check if you have payment
   * methods saved" setting to a histogram.
   */
  private onCanMakePaymentChange_() {
    MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
        PrivacyElementInteractions.PAYMENT_METHOD);
  }

  /**
   * Listens for the save-credit-card event, and calls the private API.
   */
  private saveCreditCard_(
      event: CustomEvent<chrome.autofillPrivate.CreditCardEntry>) {
    this.paymentsManager_.saveCreditCard(event.detail);
  }

  private onSaveIban_(event: CustomEvent<chrome.autofillPrivate.IbanEntry>) {
    this.paymentsManager_.saveIban(event.detail);
  }

  /**
   * @return Whether the user is verifiable through FIDO authentication.
   */
  private shouldShowFidoToggle_(
      creditCardEnabled: boolean, userIsFidoVerifiable: boolean): boolean {
    return creditCardEnabled && userIsFidoVerifiable;
  }

  /**
   * Listens for the enable-authentication event, and calls the private API.
   */
  private setFidoAuthenticationEnabledState_() {
    this.paymentsManager_.setCreditCardFidoAuthEnabledState(
        this.shadowRoot!
            .querySelector<SettingsToggleButtonElement>(
                '#autofillCreditCardFIDOAuthToggle')!.checked);
  }

  /**
   * @return Whether to show the migration button.
   */
  private checkIfMigratable_(
      creditCards: chrome.autofillPrivate.CreditCardEntry[],
      creditCardEnabled: boolean): boolean {
    // If migration prerequisites are not met, return false.
    if (!this.migrationEnabled_) {
      return false;
    }

    // If credit card enabled pref is false, return false.
    if (!creditCardEnabled) {
      return false;
    }

    const numberOfMigratableCreditCard =
        creditCards.filter(card => card.metadata!.isMigratable).length;
    // Check whether exist at least one local valid card for migration.
    if (numberOfMigratableCreditCard === 0) {
      return false;
    }

    // Update the display text depends on the number of migratable credit
    // cards.
    this.migratableCreditCardsInfo_ = numberOfMigratableCreditCard === 1 ?
        this.i18n('migratableCardsInfoSingle') :
        this.i18n('migratableCardsInfoMultiple');

    return true;
  }

  private getMenuEditCardText_(isLocalCard: boolean): string {
    return this.i18n(isLocalCard ? 'edit' : 'editServerCard');
  }

  private shouldShowAddVirtualCardButton_(): boolean {
    if (!this.virtualCardEnrollmentEnabled_ ||
        this.activeCreditCard_ === null || !this.activeCreditCard_!.metadata) {
      return false;
    }
    return !!this.activeCreditCard_!.metadata!
                 .isVirtualCardEnrollmentEligible &&
        !this.activeCreditCard_!.metadata!.isVirtualCardEnrolled;
  }

  private shouldShowRemoveVirtualCardButton_(): boolean {
    if (!this.virtualCardEnrollmentEnabled_ ||
        this.activeCreditCard_ === null || !this.activeCreditCard_!.metadata) {
      return false;
    }
    return !!this.activeCreditCard_!.metadata!
                 .isVirtualCardEnrollmentEligible &&
        !!this.activeCreditCard_!.metadata!.isVirtualCardEnrolled;
  }

  /**
   * Listens for the unenroll-virtual-card event, and calls the private API.
   */
  private unenrollVirtualCard_(event: CustomEvent<string>) {
    this.paymentsManager_.removeVirtualCard(event.detail);
  }

  /**
   * Checks if we can show the Mandatory reauth toggle.
   * This method checks if pref autofill.credit_card_enabled is true and either
   * there is support for device authentication or the mandatory auth toggle is
   * already enabled.
   */
  private shouldShowMandatoryAuthToggle_(
      deviceAuthAvailable: boolean, creditCardEnabled: boolean,
      mandatoryReauthToggleEnabled: boolean): boolean {
    return creditCardEnabled &&
        (deviceAuthAvailable || mandatoryReauthToggleEnabled);
  }

  private focusHeaderControls_(): void {
    const element =
        this.shadowRoot!.querySelector<HTMLElement>('.header-aligned-button');
    if (element) {
      focusWithoutInk(element);
    }
  }

  /**
   * Checks for user auth before flipping the mandatory auth toggle.
   */
  private onMandatoryAuthToggleChange_(e: Event) {
    const mandatoryAuthToggle = e.target as SettingsToggleButtonElement;
    assert(mandatoryAuthToggle);
    // The toggle is reset to the value when it was clicked.
    // It will be flipped afterwards if the user auth is successful.
    mandatoryAuthToggle.checked = !mandatoryAuthToggle.checked;
    this.paymentsManager_.authenticateUserAndFlipMandatoryAuthToggle();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-payments-section': SettingsPaymentsSectionElement;
  }
}

customElements.define(
    SettingsPaymentsSectionElement.is, SettingsPaymentsSectionElement);
