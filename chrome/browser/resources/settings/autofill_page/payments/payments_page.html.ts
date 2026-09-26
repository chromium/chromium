// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsPaymentsPageElement} from './payments_page.js';

export function getHtml(this: SettingsPaymentsPageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{paymentsTitle}"
    learn-more-url="$i18n{addressesAndPaymentMethodsLearnMoreURL}"
    class="multi-card">

<div class="card settings-card">

<!-- Toggle to enable/disable overall payments autofill. If disabled, almost all
     below toggles are effectively disabled. -->
<settings-toggle-button id="autofillCreditCardToggle"
    no-extension-indicator label="$i18n{enableCreditCardsLabel}"
    sub-label="$i18n{enableCreditCardsSublabel}"
    .pref="${this.creditCardEnabledSyntheticPref_}"
    @settings-boolean-control-change="${
        this.onCreditCardToggleSettingsBooleanControlChange_}">
</settings-toggle-button>

<!-- Extension-override indicator for payments autofill enable/disable; this
     MUST come immediately after the autofillCreditCardToggle toggle. -->
${this.creditCardEnabledSyntheticPref_.extensionId ? html`
  <div class="cr-row continuation">
    <extension-controlled-indicator class="flex"
        id="autofillExtensionIndicator"
        .extensionId="${this.creditCardEnabledSyntheticPref_.extensionId}"
        .extensionName="${
            this.creditCardEnabledSyntheticPref_.controlledByName || ''}"
        .extensionCanBeDisabled="${
            !!this.creditCardEnabledSyntheticPref_.extensionCanBeDisabled}">
    </extension-controlled-indicator>
  </div>
` : ''}

<!-- Toggle to enable/disable mandatory re-authentication when user attempts to
     view or autofill payment methods. -->
<if expr="is_win or is_macosx or is_chromeos">
${this.mandatoryReauthFeatureFlagEnabled_ ? html`
  <settings-toggle-button id="mandatoryAuthToggle"
      no-extension-indicator label="$i18n{enableMandatoryAuthToggleLabel}"
      sub-label="$i18n{enableMandatoryAuthToggleSublabel}"
      pref-key="autofill.payment_methods_mandatory_reauth" no-set-pref
      ?disabled="${this.shouldDisableAuthToggle_()}"
      @settings-boolean-control-change="${
          this.onMandatoryAuthToggleSettingsBooleanControlChange_}">
  </settings-toggle-button>
` : ''}
</if>

<!-- Toggle to enable/disable CVC storage. -->
${this.cvcStorageAvailable_ ? html`
  <settings-toggle-button id="cvcStorageToggle"
      no-extension-indicator label="$i18n{enableCvcStorageLabel}"
      .ariaLabel="${this.getCvcStorageAriaLabel_()}"
      sub-label-with-link="${this.getCvcStorageSublabel_()}"
      ?disabled="${!this.creditCardEnabledSyntheticPref_.value}"
      @sub-label-link-clicked="${this.onCvcStorageSubLabelLinkClicked_}"
      pref-key="autofill.payment_cvc_storage">
  </settings-toggle-button>
` : ''}

<!-- Toggle to enable/disable showing card benefits/rewards in autofill. -->
<settings-toggle-button id="cardBenefitsToggle"
    no-extension-indicator label="$i18n{cardBenefitsLabel}"
    sub-label-with-link="$i18n{cardBenefitsToggleSublabel}"
    @sub-label-link-clicked="${this.onCardBenefitsSubLabelLinkClicked_}"
    ?disabled="${!this.creditCardEnabledSyntheticPref_.value}"
    pref-key="autofill.payment_card_benefits">
</settings-toggle-button>

<!-- Toggle to enable/disable Buy Now, Pay Later (BNPL) in autofill. -->
${this.shouldShowPayOverTimeSettings_ ? html`
  <settings-toggle-button id="payOverTimeToggle"
      no-extension-indicator label="$i18n{autofillPayOverTimeSettingsLabel}"
      sub-label-with-link="$i18n{autofillPayOverTimeSettingsSublabel}"
      @sub-label-link-clicked="${this.onPayOverTimeSubLabelLinkClicked_}"
      ?disabled="${!this.creditCardEnabledSyntheticPref_.value}"
      pref-key="autofill.bnpl_enabled">
  </settings-toggle-button>
` : ''}

<!-- Toggle to allow/disallow certain behavior for the Payment Request API. Note
     that this setting does *NOT* relate to payments autofill, but lives in
     chrome://settings/payments for legacy reasons (and lack of a better place
     to put it). -->
<settings-toggle-button id="canMakePaymentToggle"
    aria-label="$i18n{canMakePaymentToggleLabel}"
    label="$i18n{canMakePaymentToggleLabel}"
    pref-key="payments.can_make_payment_enabled"
    @settings-boolean-control-change="${
        this.onCanMakePaymentSettingsBooleanControlChange_}">
</settings-toggle-button>

<div id="manageLink" class="cr-row">
  <!-- This span lays out the link correctly, relative to the text. -->
  <div class="cr-padded-text">$i18nRaw{manageCreditCardsLabel}</div>
</div>

</div>
<div class="card">

<div class="cr-row continuation">
  <h2 class="flex">$i18n{creditCards}</h2>
  ${!this.shouldShowIbanSettings_() ? html`
    <cr-button id="addCreditCard" class="header-aligned-button"
        @click="${this.onAddCreditCardClick_}"
        aria-label="$i18n{addCreditCardTitle}"
        ?disabled="${!this.creditCardEnabledSyntheticPref_.value}">
      $i18n{add}
    </cr-button>
  ` : html`
    <cr-button class="header-aligned-button"
        id="addPaymentMethods" @click="${this.onAddPaymentMethodClick_}"
        aria-label="$i18n{addPaymentMethods}"
        ?disabled="${!this.creditCardEnabledSyntheticPref_.value}">
      $i18n{add}
      <cr-icon icon="cr:arrow-drop-down" class="arrow-icon-down"></cr-icon>
    </cr-button>
    <cr-lazy-render-lit id="paymentMethodsActionMenu"
        .template="${() => html`
          <cr-action-menu role-description="$i18n{menu}">
            <button id="addCreditCard" class="dropdown-item"
                @click="${this.onAddCreditCardClick_}">
              $i18n{addPaymentMethodCreditOrDebitCard}
            </button>
            <button id="addIban" class="dropdown-item"
                @click="${this.onAddIbanClick_}">
              $i18n{addPaymentMethodIban}
            </button>
          </cr-action-menu>
        `}">
    </cr-lazy-render-lit>
  `}
</div>
<settings-payments-list id="paymentsList"
    class="list-frame payment-list-margin-start"
    .creditCards="${this.creditCards}"
    .ibans="${this.ibans}"
    .payOverTimeIssuers="${this.payOverTimeIssuers}"
    @dots-iban-menu-click="${this.onDotsIbanMenuClick_}"
    @remote-iban-menu-click="${this.onRemoteIbanMenuClick_}"
    @dots-card-menu-click="${this.onDotsCardMenuClick_}"
    @remote-card-menu-click="${this.onRemoteCardMenuClick_}"
    aria-label="$i18n{paymentsMethodsTableAriaLabel}">
</settings-payments-list>

<cr-action-menu id="creditCardSharedMenu" role-description="$i18n{menu}">
  <button id="menuEditCreditCard" class="dropdown-item"
      @click="${this.onMenuEditCreditCardClick_}">
    ${this.getMenuEditCardText_()}
  </button>

  <button id="menuRemoveCreditCard" class="dropdown-item"
      ?hidden="${!this.activeCreditCard_?.metadata?.isLocal}"
      @click="${this.onMenuRemoveCreditCardClick_}">$i18n{delete}</button>

  <button id="menuAddVirtualCard" class="dropdown-item"
      @click="${this.onMenuAddVirtualCardClick_}"
      ?hidden="${!this.shouldShowAddVirtualCardButton_()}">
    $i18n{addVirtualCard}
  </button>
  <button id="menuRemoveVirtualCard" class="dropdown-item"
      @click="${this.onMenuRemoveVirtualCardClick_}"
      ?hidden="${!this.shouldShowRemoveVirtualCardButton_()}">
    $i18n{removeVirtualCard}
  </button>
</cr-action-menu>

<cr-lazy-render-lit id="ibanSharedActionMenu"
    .template="${() => html`
      <cr-action-menu id="ibanSharedMenu" role-description="$i18n{menu}">
        <button id="menuEditIban" class="dropdown-item"
            @click="${this.onMenuEditIbanClick_}">
          $i18n{editIban}
        </button>
        <button id="menuRemoveIban" class="dropdown-item"
            @click="${this.onMenuRemoveIbanClick_}">
          $i18n{delete}
        </button>
      </cr-action-menu>
    `}">
</cr-lazy-render-lit>

${this.showCreditCardDialog_ ? html`
  <settings-credit-card-edit-dialog .creditCard="${this.activeCreditCard_!}"
      @close="${this.onCreditCardDialogClose_}"
      @save-credit-card="${this.onSaveCreditCard_}">
  </settings-credit-card-edit-dialog>
` : ''}
${this.showIbanDialog_ ? html`
  <settings-iban-edit-dialog .iban="${this.activeIban_}"
      @close="${this.onIbanDialogClose_}" @save-iban="${this.onSaveIban_}">
  </settings-iban-edit-dialog>
` : ''}

${this.showVirtualCardUnenrollDialog_ ? html`
  <settings-virtual-card-unenroll-dialog
      .creditCard="${this.activeCreditCard_!}"
      @close="${this.onVirtualCardUnenrollDialogClose_}"
      @unenroll-virtual-card="${this.onUnenrollVirtualCard_}">
  </settings-virtual-card-unenroll-dialog>
` : ''}

${this.showLocalCreditCardRemoveConfirmationDialog_ ? html`
  <settings-simple-confirmation-dialog id="localCardDeleteConfirmDialog"
      title-text="$i18n{removeLocalCreditCardConfirmationTitle}"
      body-text="$i18n{removeLocalPaymentMethodConfirmationDescription}"
      confirm-text="$i18n{delete}"
      @close="${this.onLocalCreditCardRemoveConfirmationDialogClose_}">
  </settings-simple-confirmation-dialog>
` : ''}

${this.showLocalIbanRemoveConfirmationDialog_ ? html`
  <settings-simple-confirmation-dialog id="localIbanDeleteConfirmationDialog"
      title-text="$i18n{removeLocalIbanConfirmationTitle}"
      body-text="$i18n{removeLocalPaymentMethodConfirmationDescription}"
      confirm-text="$i18n{delete}"
      @close="${this.onLocalIbanRemoveConfirmationDialogClose_}">
  </settings-simple-confirmation-dialog>
` : ''}

${this.showBulkRemoveCvcConfirmationDialog_ ? html`
  <settings-simple-confirmation-dialog id="bulkDeleteCvcConfirmDialog"
      title-text="$i18n{bulkRemoveCvcConfirmationTitle}"
      body-text="${this.getCvcDeletionDialogBodyText_()}"
      confirm-text="$i18n{delete}"
      @close="${this.onShowBulkRemoveCvcConfirmationDialogClose_}">
  </settings-simple-confirmation-dialog>
` : ''}

</div>

</settings-subpage>
<!--_html_template_end_-->`;
}
