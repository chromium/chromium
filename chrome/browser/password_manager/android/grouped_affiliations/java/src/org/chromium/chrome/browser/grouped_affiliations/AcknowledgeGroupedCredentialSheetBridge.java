// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.grouped_affiliations;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

// JNI bridge to display the acknowledgement sheet when filling grouped
// credentials on Android.
@NullMarked
public class AcknowledgeGroupedCredentialSheetBridge {
    private long mNativeAcknowledgeGroupedCredentialSheetBridge;
    private final WindowAndroid mWindowAndroid;
    private @Nullable AcknowledgeGroupedCredentialSheetController mController;

    @CalledByNative
    public AcknowledgeGroupedCredentialSheetBridge(
            long nativeAddUsernameDialogBridge, WindowAndroid windowAndroid) {
        mNativeAcknowledgeGroupedCredentialSheetBridge = nativeAddUsernameDialogBridge;
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    public void show(
            @JniType("std::string") String currentHostname,
            @JniType("std::string") String credentialHostname) {
        Context context = mWindowAndroid.getContext().get();
        if (context == null) return;

        mController =
                new AcknowledgeGroupedCredentialSheetController(
                        context,
                        assumeNonNull(BottomSheetControllerProvider.from(mWindowAndroid)),
                        this::onDismissed);
        mController.show(currentHostname, credentialHostname);
    }

    @CalledByNative
    public void dismiss() {
        if (mNativeAcknowledgeGroupedCredentialSheetBridge == 0) return;
        assumeNonNull(mController);
        mController.dismiss();
        mNativeAcknowledgeGroupedCredentialSheetBridge = 0;
    }

    public void onDismissed(@DismissReason int dismissReason) {
        if (mNativeAcknowledgeGroupedCredentialSheetBridge == 0) return;
        AcknowledgeGroupedCredentialSheetBridgeJni.get()
                .onDismissed(mNativeAcknowledgeGroupedCredentialSheetBridge, dismissReason);
        mNativeAcknowledgeGroupedCredentialSheetBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onDismissed(
                long nativeAcknowledgeGroupedCredentialSheetBridge,
                @DismissReason int dismissReason);
    }
}
