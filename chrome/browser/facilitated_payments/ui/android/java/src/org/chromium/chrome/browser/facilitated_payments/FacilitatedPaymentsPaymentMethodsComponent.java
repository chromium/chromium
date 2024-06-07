// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.content.Context;

import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;

/**
 * This component allows to select a facilitated payments method. It acts as a 1-tap surface (bottom
 * sheet).
 */
interface FacilitatedPaymentsPaymentMethodsComponent {
    interface Delegate {}

    /** Initializes the component. */
    void initialize(
            Context context, BottomSheetController bottomSheetController, Delegate delegate);

    /** Displays a new bottom sheet. */
    boolean showSheet(List<BankAccount> bankAccounts);
}
