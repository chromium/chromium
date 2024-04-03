// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.content.Context;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * This component allows to select a facilitated payments method. It acts as a 1-tap surface (bottom
 * sheet).
 */
interface FacilitatedPaymentsPaymentMethodsComponent {
    interface Delegate {}

    /** Initializes the component. */
    void initialize(Context context, BottomSheetController bottomSheetController);

    /** Displays a new bottom sheet. */
    void showSheet();
}
