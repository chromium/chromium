// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebAuthnCredential;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.Arrays;

/**
 * This bridge creates and initializes a {@link TouchToFillComponent} on construction and forwards
 * native calls to it.
 */
class TouchToFillBridge implements TouchToFillComponent.Delegate {
    private long mNativeView;
    private final TouchToFillComponent mTouchToFillComponent;

    private TouchToFillBridge(long nativeView, WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController) {
        mNativeView = nativeView;
        mTouchToFillComponent = new TouchToFillCoordinator();
        mTouchToFillComponent.initialize(
                windowAndroid.getContext().get(), bottomSheetController, this);
    }

    @CalledByNative
    private static @Nullable TouchToFillBridge create(
            long nativeView, WindowAndroid windowAndroid) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;
        return new TouchToFillBridge(nativeView, windowAndroid, bottomSheetController);
    }

    @CalledByNative
    private void destroy() {
        mNativeView = 0;
    }

    @CalledByNative
    private static Credential[] createCredentialArray(int size) {
        return new Credential[size];
    }

    @CalledByNative
    private static void insertCredential(Credential[] credentials, int index, String username,
            String password, String formattedUsername, String originUrl,
            boolean isPublicSuffixMatch, boolean isAffiliationBasedMatch,
            long lastUsedMsSinceEpoch) {
        credentials[index] = new Credential(username, password, formattedUsername, originUrl,
                isPublicSuffixMatch, isAffiliationBasedMatch, lastUsedMsSinceEpoch);
    }

    @CalledByNative
    private static WebAuthnCredential[] createWebAuthnCredentialArray(int size) {
        return new WebAuthnCredential[size];
    }

    @CalledByNative
    private static void insertWebAuthnCredential(
            WebAuthnCredential[] credentials, int index, String username, String id) {
        credentials[index] = new WebAuthnCredential(username, id);
    }

    @CalledByNative
    private void showCredentials(GURL url, boolean isOriginSecure,
            WebAuthnCredential[] webAuthnCredentials, Credential[] credentials,
            boolean submitCredential) {
        mTouchToFillComponent.showCredentials(url, isOriginSecure,
                Arrays.asList(webAuthnCredentials), Arrays.asList(credentials), submitCredential);
    }

    @Override
    public void onDismissed() {
        if (mNativeView != 0) TouchToFillBridgeJni.get().onDismiss(mNativeView);
    }

    @Override
    public void onManagePasswordsSelected() {
        if (mNativeView != 0) TouchToFillBridgeJni.get().onManagePasswordsSelected(mNativeView);
    }

    @Override
    public void onCredentialSelected(Credential credential) {
        if (mNativeView != 0) {
            TouchToFillBridgeJni.get().onCredentialSelected(mNativeView, credential);
        }
    }

    @Override
    public void onWebAuthnCredentialSelected(WebAuthnCredential credential) {
        if (mNativeView != 0) {
            TouchToFillBridgeJni.get().onWebAuthnCredentialSelected(mNativeView, credential);
        }
    }

    @NativeMethods
    interface Natives {
        void onCredentialSelected(long nativeTouchToFillViewImpl, Credential credential);
        void onWebAuthnCredentialSelected(
                long nativeTouchToFillViewImpl, WebAuthnCredential credential);
        void onManagePasswordsSelected(long nativeTouchToFillViewImpl);
        void onDismiss(long nativeTouchToFillViewImpl);
    }
}
