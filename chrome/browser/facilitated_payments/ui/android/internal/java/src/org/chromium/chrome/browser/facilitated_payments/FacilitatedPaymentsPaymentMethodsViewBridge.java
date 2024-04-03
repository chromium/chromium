// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Bridge class providing an entry point for facilitated payments client to trigger the bottom
 * sheet.
 */
@JNINamespace("payments::facilitated")
public class FacilitatedPaymentsPaymentMethodsViewBridge {
    private final FacilitatedPaymentsPaymentMethodsComponent mComponent;

    private FacilitatedPaymentsPaymentMethodsViewBridge(
            Context context, BottomSheetController bottomSheetController) {
        mComponent = new FacilitatedPaymentsPaymentMethodsCoordinator();
        mComponent.initialize(context, bottomSheetController);
    }

    @CalledByNative
    @VisibleForTesting
    static @Nullable FacilitatedPaymentsPaymentMethodsViewBridge create(
            WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;

        Context context = windowAndroid.getContext().get();
        if (context == null) return null;

        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;

        return new FacilitatedPaymentsPaymentMethodsViewBridge(context, bottomSheetController);
    }

    /**
     * Requests to show the bottom sheet.
     *
     * <p>The bottom sheet may not be shown in some cases. {@see
     * BottomSheetController#requestShowContent}
     *
     * @return True if shown. False if it was suppressed. Content is suppressed if higher priority
     *     content is in the sheet, the sheet is expanded beyond the peeking state, or the browser
     *     is in a mode that does not support showing the sheet.
     */
    @CalledByNative
    public void requestShowContent() {
        mComponent.showSheet();
    }
}
