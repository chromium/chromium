// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mandatory_reauth;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.autofill.PaymentsBubbleClosedReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java bridge to delegate calls from native MandatoryReauthOptInViewAndroid. Facilitates creating
 * the Mandatory Reauth opt-in bottom sheet.
 */
@JNINamespace("autofill")
class MandatoryReauthOptInBottomSheetViewBridge {
    private final MandatoryReauthOptInBottomSheetComponent mComponent;

    MandatoryReauthOptInBottomSheetViewBridge(MandatoryReauthOptInBottomSheetComponent component) {
        mComponent = component;
    }

    /**
     * Creates an instance of a {@link MandatoryReauthOptInBottomSheetViewBridge}.
     *
     * @param windowAndroid Window to show the bottom sheet.
     */
    @CalledByNative
    private static @Nullable MandatoryReauthOptInBottomSheetViewBridge create(
            WindowAndroid windowAndroid,
            MandatoryReauthOptInBottomSheetComponent.Delegate delegate) {
        if (windowAndroid == null) return null;
        Context context = windowAndroid.getContext().get();
        if (context == null) return null;
        BottomSheetController controller = BottomSheetControllerProvider.from(windowAndroid);
        if (controller == null) return null;

        return new MandatoryReauthOptInBottomSheetViewBridge(
                new MandatoryReauthOptInBottomSheetCoordinator(context, controller, delegate));
    }

    /** Shows the view. */
    @CalledByNative
    boolean show() {
        return mComponent.show();
    }

    /** Closes the view. */
    @CalledByNative
    void close() {
        mComponent.close(PaymentsBubbleClosedReason.NOT_INTERACTED);
    }
}
