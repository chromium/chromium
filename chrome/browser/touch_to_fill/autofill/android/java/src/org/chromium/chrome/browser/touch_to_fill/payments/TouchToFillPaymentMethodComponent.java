// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.LoyaltyCard;
import org.chromium.components.autofill.payments.BnplIssuerContext;
import org.chromium.components.autofill.payments.BnplIssuerTosDetail;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;

/**
 * This component allows to select a payment method to be filled into a form. It acts as a 1-tap
 * surface (bottom sheet) and is meant to be shown while the keyboard is suppressed.
 */
@NullMarked
interface TouchToFillPaymentMethodComponent {
    /** This delegate is called when the TouchToFillPaymentMethod component is interacted with. */
    interface Delegate {
        /** Called whenever the sheet is dismissed (by user or native). */
        void onDismissed(boolean dismissedByUser, boolean shouldReshow);

        /** Called when user requests to scan a new credit card. */
        void scanCreditCard();

        /** Causes navigation to the payment methods settings page. */
        void showPaymentMethodSettings();

        /**
         * Causes navigation to the payment methods settings page and scrolls to the loyalty card
         * settings preference.
         */
        void showGoogleWalletSettings();

        /**
         * Called when the user selects a card.
         *
         * @param uniqueId A backend id of the card.
         * @param isVirtual A boolean to identify if the card is a virtual card.
         */
        void creditCardSuggestionSelected(String uniqueId, boolean isVirtual);

        /**
         * Called when the user selects the BNPL suggestion. If the extractedAmount is available, we
         * show the issuer selection screen. Otherwise, the progress screen is displayed until
         * amount extraction is complete.
         *
         * @param extractedAmount The amount associated with the BNPL suggestion, extracted from the
         *     page.
         */
        void bnplSuggestionSelected(@Nullable Long extractedAmount);

        /**
         * Called when the user selects a local IBAN.
         *
         * @param guid The selected local IBAN.
         */
        void localIbanSuggestionSelected(String guid);

        /**
         * Called when the user selects a server IBAN.
         *
         * @param instrumentId The selected server IBAN.
         */
        void serverIbanSuggestionSelected(long instrumentId);

        /**
         * Called when the user selects a loyalty card.
         *
         * @param loyaltyCard the selected loyalty card.
         */
        void loyaltyCardSuggestionSelected(LoyaltyCard loyaltyCard);

        /** Called when the user clicks the "Manage loyalty cards" button. */
        void openPassesManagementUi();

        /**
         * Called when the user selects a BNPL issuer.
         *
         * @param issuerId The selected BNPL issuer Id.
         */
        void onBnplIssuerSuggestionSelected(String issuerId);

        /** Called when the user clicks the "Continue" button on the BNPL ToS screen. */
        void onBnplTosAccepted();
    }

    /**
     * Initializes the component.
     *
     * @param context A {@link Context} to create views and retrieve resources.
     * @param imageFetcher A {@link AutofillImageFetcher} associated with the profile.
     * @param sheetController A {@link BottomSheetController} used to show/hide the sheet.
     * @param delegate A {@link Delegate} that handles interaction events.
     * @param bottomSheetFocusHelper that restores the focus to the element that was focused before
     *     the bottom sheet.
     */
    void initialize(
            Context context,
            AutofillImageFetcher imageFetcher,
            BottomSheetController sheetController,
            Delegate delegate,
            BottomSheetFocusHelper bottomSheetFocusHelper);

    /**
     * Displays a new payment methods bottom sheet.
     *
     * @param suggestions A list of {@link AutofillSuggestion}, each generated from a corresponding
     *     credit card. It includes a boolean that denotes if the card is acceptable for the given
     *     merchant. If not acceptable, the card suggestion is grayed out.
     * @param shouldShowScanCreditCard A boolean that conveys whether 'ScanCreditCard' should be
     *     shown.
     */
    void showPaymentMethods(List<AutofillSuggestion> suggestions, boolean shouldShowScanCreditCard);

    /** Displays a new IBAN bottom sheet. */
    void showIbans(List<PersonalDataManager.Iban> ibans);

    /**
     * Displays a new loyalty card bottom sheet.
     *
     * @param affiliatedLoyaltyCards The list of loyalty cards directly linked to the current
     *     domain, that are displayed on the first screen in the bottom sheet.
     * @param allLoyaltyCards The list of all the user's loyalty cards that are displayed on the
     *     second screen in the bottom sheet.
     * @param firstTimeUsage A boolean indicating whether the first time usage message should be
     *     shown to the user.
     */
    void showLoyaltyCards(
            List<LoyaltyCard> affiliatedLoyaltyCards,
            List<LoyaltyCard> allLoyaltyCards,
            boolean firstTimeUsage);

    /**
     * Updates BNPL suggestions or BNPL screen on the bottom sheet based on the results of amount
     * extraction.
     *
     * @param bnplIssuerContexts A list of {@link BnplIssuerContext} objects, each representing a
     *     BNPL issuer context, to be displayed on the bottom sheet for the user to select from.
     * @param extractedAmount The amount extracted from the checkout page, or {@code null} if
     *     extraction failed or timed out.
     * @param isAmountSupportedByAnyIssuer Whether the {@code extractedAmount} is supported by at
     *     least one BNPL issuer. This is only relevant if {@code extractedAmount} is not {@code
     *     null}.
     */
    void onPurchaseAmountExtracted(
            List<BnplIssuerContext> bnplIssuerContexts,
            @Nullable Long extractedAmount,
            boolean isAmountSupportedByAnyIssuer);

    /** Displays a progress screen bottom sheet. */
    void showProgressScreen();

    /**
     * Displays a new BNPL issuers bottom sheet.
     *
     * @param bnplIssuerContexts A list of {@link BnplIssuerContext} objects, each representing a
     *     BNPL issuer context, to be displayed on the bottom sheet for the user to select from.
     */
    void showBnplIssuers(List<BnplIssuerContext> bnplIssuerContexts);

    /**
     * Displays an error screen bottom sheet.
     *
     * @param title The title to be displayed on the error screen.
     * @param description The description to be displayed on the error screen.
     */
    void showErrorScreen(String title, String description);

    /**
     * Displays a new BNPL issuer ToS bottom sheet.
     *
     * @param bnplIssuerTosDetail The struct that holds info for showing the ToS screen.
     */
    void showBnplIssuerTos(BnplIssuerTosDetail bnplIssuerTosDetail);

    /** Hides the bottom sheet if shown. */
    void hideSheet();

    /**
     * Sets the bottom sheet visibility.
     *
     * @param visible The value to set the bottom sheet visibility to.
     */
    void setVisible(boolean visible);
}
