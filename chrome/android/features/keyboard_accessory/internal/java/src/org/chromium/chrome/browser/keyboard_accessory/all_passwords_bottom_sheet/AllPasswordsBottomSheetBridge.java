// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * This bridge creates and initializes a {@link AllPasswordsBottomSheetCoordinator} on construction
 * and forwards native calls to it.
 */
class AllPasswordsBottomSheetBridge implements AllPasswordsBottomSheetCoordinator.Delegate {
    private long mNativeView;
    private final AllPasswordsBottomSheetCoordinator mAllPasswordsBottomSheetCoordinator;

    private AllPasswordsBottomSheetBridge(
            long nativeView,
            Profile profile,
            Context context,
            String origin,
            BottomSheetController bottomSheetController) {
        assert nativeView != 0;
        mNativeView = nativeView;
        mAllPasswordsBottomSheetCoordinator = new AllPasswordsBottomSheetCoordinator();
        mAllPasswordsBottomSheetCoordinator.initialize(
                context, profile, bottomSheetController, this, origin);
    }

    @CalledByNative
    private static @Nullable AllPasswordsBottomSheetBridge create(
            long nativeView,
            Profile profile,
            @Nullable WindowAndroid windowAndroid,
            @JniType("std::string") String origin) {
        if (windowAndroid == null) {
            return null;
        }
        Context context = windowAndroid.getActivity().get();
        if (context == null) {
            return null;
        }
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) {
            return null;
        }
        return new AllPasswordsBottomSheetBridge(
                nativeView, profile, context, origin, bottomSheetController);
    }

    @CalledByNative
    private void destroy() {
        mNativeView = 0;
    }

    @CalledByNative
    private void showCredentials(
            @JniType("std::vector") List<Credential> credentials, boolean isPasswordField) {
        mAllPasswordsBottomSheetCoordinator.showCredentials(credentials, isPasswordField);
    }

    @Override
    public void onCredentialSelected(CredentialFillRequest credentialFillRequest) {
        if (mNativeView != 0) { // The native side is already dismissed.
            AllPasswordsBottomSheetBridgeJni.get()
                    .onCredentialSelected(
                            mNativeView,
                            credentialFillRequest.getCredential().getUsername(),
                            credentialFillRequest.getCredential().getPassword(),
                            credentialFillRequest.getRequestsToFillPassword());
        }
    }

    @Override
    public void onDismissed() {
        if (mNativeView != 0) {
            AllPasswordsBottomSheetBridgeJni.get().onDismiss(mNativeView);
        }
    }

    @NativeMethods
    interface Natives {
        void onCredentialSelected(
                long nativeAllPasswordsBottomSheetViewImpl,
                @JniType("std::u16string") String username,
                @JniType("std::u16string") String password,
                boolean requestsToFillPassword);

        void onDismiss(long nativeAllPasswordsBottomSheetViewImpl);
    }
}
