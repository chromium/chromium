// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsComponent.Delegate;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

import java.util.Arrays;
import java.util.List;

/**
 * Bridge class providing an entry point for facilitated payments client to trigger the bottom
 * sheet.
 */
@JNINamespace("payments::facilitated")
public class FacilitatedPaymentsPaymentMethodsViewBridge {
    private final FacilitatedPaymentsPaymentMethodsComponent mComponent;

    private FacilitatedPaymentsPaymentMethodsViewBridge(
            Context context, BottomSheetController bottomSheetController, Delegate delegate) {
        mComponent = new FacilitatedPaymentsPaymentMethodsCoordinator();
        mComponent.initialize(context, bottomSheetController, delegate);
    }

    @CalledByNative
    @VisibleForTesting
    static @Nullable FacilitatedPaymentsPaymentMethodsViewBridge create(
            Delegate delegate, WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;

        Context context = windowAndroid.getContext().get();
        if (context == null) return null;

        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;

        return new FacilitatedPaymentsPaymentMethodsViewBridge(
                context, bottomSheetController, delegate);
    }

    /**
     * Requests to show the bottom sheet.
     *
     * <p>The bottom sheet may not be shown in some cases. {@see
     * BottomSheetController#requestShowContent}
     *
     * @param bankAccounts User's bank accounts which passed from facilitated payments client.
     * @return True if shown. False if it was suppressed. Content is suppressed if higher priority
     *     content is in the sheet, the sheet is expanded beyond the peeking state, or the browser
     *     is in a mode that does not support showing the sheet.
     */
    @CalledByNative
    public boolean requestShowContent(@JniType("std::vector") Object[] bankAccounts) {
        return mComponent.showSheet((List<BankAccount>) (List<?>) Arrays.asList(bankAccounts));
    }
}
