// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.grouped_affiliations;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

// JNI bridge to display the acknowledgement sheet when filling grouped
// credentials on Android.
@NullMarked
public class AcknowledgeGroupedCredentialSheetBridge {
    @JniType("::AcknowledgeGroupedCredentialSheetBridge")
    interface NativeAcknowledgeGroupedCredentialSheetBridge extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeAcknowledgeGroupedCredentialSheetBridge>
            mNativeAcknowledgeGroupedCredentialSheetBridge;
    private final WindowAndroid mWindowAndroid;
    private @Nullable AcknowledgeGroupedCredentialSheetController mController;

    @CalledByNative
    public AcknowledgeGroupedCredentialSheetBridge(
            JniRawPtr<NativeAcknowledgeGroupedCredentialSheetBridge> nativeAddUsernameDialogBridge,
            WindowAndroid windowAndroid) {
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
        if (mNativeAcknowledgeGroupedCredentialSheetBridge == null) return;
        // The controller is missing if `show` returned early.
        if (mController != null) mController.dismiss();
        mNativeAcknowledgeGroupedCredentialSheetBridge.release();
        mNativeAcknowledgeGroupedCredentialSheetBridge = null;
    }

    public void onDismissed(@DismissReason int dismissReason) {
        if (mNativeAcknowledgeGroupedCredentialSheetBridge == null) return;
        AcknowledgeGroupedCredentialSheetBridgeJni.get()
                .onDismissed(mNativeAcknowledgeGroupedCredentialSheetBridge, dismissReason);
        mNativeAcknowledgeGroupedCredentialSheetBridge.release();
        mNativeAcknowledgeGroupedCredentialSheetBridge = null;
    }

    @NativeMethods
    interface Natives {
        void onDismissed(
                JniPtr<NativeAcknowledgeGroupedCredentialSheetBridge> self,
                @DismissReason int dismissReason);
    }
}
