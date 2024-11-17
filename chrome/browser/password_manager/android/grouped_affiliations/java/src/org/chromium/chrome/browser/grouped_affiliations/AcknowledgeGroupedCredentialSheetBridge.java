// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.grouped_affiliations;

import android.content.Context;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

// JNI bridge to display the acknowledgement sheet when filling grouped
// credentials on Android.
public class AcknowledgeGroupedCredentialSheetBridge {
    private long mNativeAcknowledgeGroupedCredentialSheetBridge;
    private final WindowAndroid mWindowAndroid;
    private AcknowledgeGroupedCredentialSheetController mController;

    @CalledByNative
    public AcknowledgeGroupedCredentialSheetBridge(
            long nativeAddUsernameDialogBridge, @NonNull WindowAndroid windowAndroid) {
        mNativeAcknowledgeGroupedCredentialSheetBridge = nativeAddUsernameDialogBridge;
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    public void show(
            @JniType("std::string") String currentOrigin,
            @JniType("std::string") String credentialOrigin) {
        Context context = mWindowAndroid.getContext().get();
        if (context == null) return;

        mController =
                new AcknowledgeGroupedCredentialSheetController(
                        context,
                        BottomSheetControllerProvider.from(mWindowAndroid),
                        this::onDismissed);
        mController.show(currentOrigin, credentialOrigin);
    }

    @CalledByNative
    public void dismiss() {
        if (mNativeAcknowledgeGroupedCredentialSheetBridge == 0) return;
        mController.dismiss();
        mNativeAcknowledgeGroupedCredentialSheetBridge = 0;
    }

    public void onDismissed(boolean accepted) {
        if (mNativeAcknowledgeGroupedCredentialSheetBridge == 0) return;
        AcknowledgeGroupedCredentialSheetBridgeJni.get()
                .onDismissed(mNativeAcknowledgeGroupedCredentialSheetBridge, accepted);
        mNativeAcknowledgeGroupedCredentialSheetBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeAcknowledgeGroupedCredentialSheetBridge, boolean accepted);
    }
}
