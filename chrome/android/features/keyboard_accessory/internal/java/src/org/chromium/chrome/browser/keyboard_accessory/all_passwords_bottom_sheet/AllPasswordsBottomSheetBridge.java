// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * This bridge creates and initializes a {@link AllPasswordsBottomSheetCoordinator} on construction
 * and forwards native calls to it.
 */
class AllPasswordsBottomSheetBridge implements AllPasswordsBottomSheetCoordinator.Delegate {
    private long mNativeView;
    private Credential[] mCredentials;
    private final AllPasswordsBottomSheetCoordinator mAllPasswordsBottomSheetCoordinator;

    private AllPasswordsBottomSheetBridge(
            long nativeView, WindowAndroid windowAndroid, String origin) {
        mNativeView = nativeView;
        assert (mNativeView != 0);
        assert (windowAndroid.getActivity().get() != null);
        mAllPasswordsBottomSheetCoordinator = new AllPasswordsBottomSheetCoordinator();
        mAllPasswordsBottomSheetCoordinator.initialize(
                windowAndroid.getActivity().get(),
                BottomSheetControllerProvider.from(windowAndroid),
                this,
                origin);
    }

    @CalledByNative
    private static AllPasswordsBottomSheetBridge create(
            long nativeView, WindowAndroid windowAndroid, String origin) {
        return new AllPasswordsBottomSheetBridge(nativeView, windowAndroid, origin);
    }

    @CalledByNative
    private void destroy() {
        mNativeView = 0;
    }

    @CalledByNative
    private void createCredentialArray(int size) {
        mCredentials = new Credential[size];
    }

    @CalledByNative
    private void insertCredential(
            int index,
            String username,
            String password,
            String formattedUsername,
            String originUrl,
            boolean isAndroidCredential,
            String appDisplayName) {
        mCredentials[index] =
                new Credential(
                        username,
                        password,
                        formattedUsername,
                        originUrl,
                        isAndroidCredential,
                        appDisplayName);
    }

    @CalledByNative
    private void showCredentials(boolean isPasswordField) {
        mAllPasswordsBottomSheetCoordinator.showCredentials(mCredentials, isPasswordField);
    }

    @Override
    public void onCredentialSelected(CredentialFillRequest credentialFillRequest) {
        assert mNativeView != 0 : "The native side is already dismissed";
        AllPasswordsBottomSheetBridgeJni.get()
                .onCredentialSelected(
                        mNativeView,
                        credentialFillRequest.getCredential().getUsername(),
                        credentialFillRequest.getCredential().getPassword(),
                        credentialFillRequest.getRequestsToFillPassword());
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
                String username,
                String password,
                boolean requestsToFillPassword);

        void onDismiss(long nativeAllPasswordsBottomSheetViewImpl);
    }
}
