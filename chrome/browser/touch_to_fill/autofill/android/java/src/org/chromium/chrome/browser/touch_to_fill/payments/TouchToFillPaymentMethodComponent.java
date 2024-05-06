// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.content.Context;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;

/**
 * This component allows to select a payment method to be filled into a form. It acts as a 1-tap
 * surface (bottom sheet) and is meant to be shown while the keyboard is suppressed.
 */
interface TouchToFillPaymentMethodComponent {
    /** This delegate is called when the TouchToFillPaymentMethod component is interacted with. */
    interface Delegate {
        /** Called whenever the sheet is dismissed (by user or native). */
        void onDismissed(boolean dismissedByUser);

        /** Called when user requests to scan a new credit card. */
        void scanCreditCard();

        /** Causes navigation to the payment methods settings page. */
        void showPaymentMethodSettings();

        /**
         * Called when the user selects a card.
         *
         * @param uniqueId A backend id of the card.
         * @param isVirtual A boolean to identify if the card is a virtual card.
         */
        void creditCardSuggestionSelected(String uniqueId, boolean isVirtual);

        /**
         * Called when the user selects an IBAN.
         *
         * @param guid GUID of the IBAN.
         */
        void ibanSuggestionSelected(String guid);
    }

    /**
     * Initializes the component.
     *
     * @param context A {@link Context} to create views and retrieve resources.
     * @param personalDataManager A {@link PersonalDataManager} associated with the payment method
     *     data.
     * @param sheetController A {@link BottomSheetController} used to show/hide the sheet.
     * @param delegate A {@link Delegate} that handles interaction events.
     * @param bottomSheetFocusHelper that restores the focus to the element that was focused before
     *     the bottom sheet.
     */
    void initialize(
            Context context,
            PersonalDataManager personalDataManager,
            BottomSheetController sheetController,
            Delegate delegate,
            BottomSheetFocusHelper bottomSheetFocusHelper);

    /** Displays a new credit card bottom sheet. */
    void showSheet(List<PersonalDataManager.CreditCard> cards, boolean shouldShowScanCreditCard);

    /** Displays a new IBAN bottom sheet. */
    void showSheet(List<PersonalDataManager.Iban> ibans);

    /** Hides the bottom sheet if shown. */
    void hideSheet();
}
