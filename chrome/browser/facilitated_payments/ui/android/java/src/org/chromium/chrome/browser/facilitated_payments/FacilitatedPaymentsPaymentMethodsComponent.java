// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.content.Context;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;

/**
 * This component allows to select a facilitated payments method. It acts as a 1-tap surface (bottom
 * sheet).
 */
interface FacilitatedPaymentsPaymentMethodsComponent {
    /**
     * This delegate is called when the FacilitatedPaymentsPaymentMethods component is interacted
     * with.
     */
    interface Delegate {
        /** Called whenever the sheet is dismissed. */
        void onDismissed();

        /** Called whenever a bank account is selected. */
        void onBankAccountSelected(long instrumentId);

        /** Called whenever the payment settings text is clicked on the bottom sheet. */
        boolean showFinancialAccountsManagementSettings(Context context);

        /** Called whenever the manage payment methods footer is tapped on the bottom sheet. */
        boolean showManagePaymentMethodsSettings(Context context);
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

    /** Displays a FOP selector in a bottom sheet. */
    boolean showSheet(List<BankAccount> bankAccounts);

    /** Displays a progress screen in a bottom sheet. */
    void showProgressScreen();

    /** Displays an error screen in a bottom sheet. */
    void showErrorScreen();

    /** Close the bottom sheet. */
    void dismiss();
}
