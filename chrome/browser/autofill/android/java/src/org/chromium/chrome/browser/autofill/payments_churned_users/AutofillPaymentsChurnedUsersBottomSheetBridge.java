// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.AutofillEnableResurrectingPaymentsUsersTreatmentArm;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** JNI bridge entry point to show the Payments Churned Users bottom sheet. */
@JNINamespace("autofill")
@NullMarked
public class AutofillPaymentsChurnedUsersBottomSheetBridge {
    private final WindowAndroid mWindowAndroid;
    private @Nullable AutofillPaymentsChurnedUsersBottomSheetCoordinator mCoordinator;

    @CalledByNative
    public AutofillPaymentsChurnedUsersBottomSheetBridge(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    public void requestShowContent(
            @AutofillEnableResurrectingPaymentsUsersTreatmentArm int treatmentArm) {
        Context context = mWindowAndroid.getContext().get();
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(mWindowAndroid);
        if (context == null || bottomSheetController == null) {
            return;
        }

        if (mCoordinator != null) {
            mCoordinator.destroy();
        }

        mCoordinator =
                new AutofillPaymentsChurnedUsersBottomSheetCoordinator(
                        context, bottomSheetController, treatmentArm);
        mCoordinator.requestShowContent();
    }

    @CalledByNative
    public void destroy() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
            mCoordinator = null;
        }
    }

    @Nullable AutofillPaymentsChurnedUsersBottomSheetCoordinator getCoordinatorForTesting() {
        return mCoordinator;
    }
}
