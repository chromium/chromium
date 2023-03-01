// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.content.Context;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * This component allows to select a credit card to be filled into a form. It acts as a 1-tap
 * surface (bottom sheet) and is meant to be shown while the keyboard is suppressed.
 */
interface TouchToFillCreditCardComponent {
    /**
     * This delegate is called when the TouchToFillCreditCard component is interacted with.
     */
    interface Delegate {
        /**
         * Called whenever the sheet is dismissed (by user or native).
         */
        void onDismissed(boolean dismissedByUser);

        /**
         * Called when user requests to scan a new credit card.
         */
        void scanCreditCard();

        /**
         * Causes navigation to the payment methods settings page.
         */
        void showCreditCardSettings();

        /**
         * Called when the user selects a card.
         * @param uniqueId A backend id of the card.
         * @param isVirtual A boolean to identify if the card is a virtual card.
         */
        void suggestionSelected(String uniqueId, boolean isVirtual);
    }

    /**
     * Initializes the component.
     * @param context A {@link Context} to create views and retrieve resources.
     * @param sheetController A {@link BottomSheetController} used to show/hide the sheet.
     * @param delegate A {@link Delegate} that handles interaction events.
     * @param bottomSheetFocusHelper that restores the focus to the element that was focused before
     *         the bottom sheet.
     */
    void initialize(Context context, BottomSheetController sheetController, Delegate delegate,
            BottomSheetFocusHelper bottomSheetFocusHelper);

    /**
     * Displays a new bottom sheet.
     */
    void showSheet(PersonalDataManager.CreditCard[] cards, boolean shouldShowScanCreditCard);

    /**
     * Hides the bottom sheet if shown.
     */
    void hideSheet();
}
