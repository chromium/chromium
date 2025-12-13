// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.content.Context;
import android.content.pm.ResolveInfo;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.facilitated_payments.core.ui_utils.UiEvent;

import java.util.List;

/**
 * This component allows to select a facilitated payments method. It acts as a 1-tap surface (bottom
 * sheet).
 */
@NullMarked
interface FacilitatedPaymentsPaymentMethodsComponent {
    /**
     * This delegate is called when the FacilitatedPaymentsPaymentMethods component is interacted
     * with.
     */
    interface Delegate {
        /**
         * Called whenever a UI event takes place.
         *
         * @param uiEvent The type of UI event.
         */
        void onUiEvent(@UiEvent int uiEvent);

        /**
         * Called whenever a bank account is selected.
         *
         * @param instrumentId The instrument ID of the selected bank account.
         */
        void onBankAccountSelected(long instrumentId);

        /**
         * Called whenever an eWallet is selected.
         *
         * @param instrumentId The instrument ID of the selected eWallet.
         */
        void onEwalletSelected(long instrumentId);

        /**
         * Called whenever a payment app is selected.
         *
         * @param packageName The package name of the selected payment app.
         * @param activityName The activity name of the selected payment app.
         */
        void onPaymentAppSelected(String packageName, String activityName);

        /** Called whenever the Pix account linking prompt is accepted. */
        void onPixAccountLinkingPromptAccepted();

        /** Called whenever the Pix account linking prompt is declined. */
        void onPixAccountLinkingPromptDeclined();
    }

    /** Initializes the component. */
    void initialize(
            Context context,
            BottomSheetController bottomSheetController,
            Delegate delegate,
            Profile profile);

    /**
     * @return True if the device is being used in the landscape mode.
     */
    boolean isInLandscapeMode();

    /** Displays a Pix FOP selector in a bottom sheet. */
    void showSheetForPix(List<BankAccount> bankAccounts);

    /**
     * Displays a payment link FOP (Form of Payment) selector in a bottom sheet.
     *
     * @param eWallets A list of eWallets.
     * @param apps A list of {@link ResolveInfo} objects representing the payment apps.
     */
    void showSheetForPaymentLink(List<Ewallet> eWallets, List<ResolveInfo> apps);

    /** Displays a progress screen in a bottom sheet. */
    void showProgressScreen();

    /** Displays an error screen in a bottom sheet. */
    void showErrorScreen();

    /** Close the bottom sheet. */
    void dismiss();

    /** Show the Pix account linking prompt in a bottom sheet. */
    void showPixAccountLinkingPrompt();
}
