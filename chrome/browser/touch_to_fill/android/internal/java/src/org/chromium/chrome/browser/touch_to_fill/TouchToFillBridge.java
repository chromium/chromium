// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.graphics.Bitmap;

import androidx.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.ui.base.WindowAndroid;

import java.util.Arrays;

/**
 * This bridge creates and initializes a {@link TouchToFillComponent} on construction and forwards
 * native calls to it.
 */
class TouchToFillBridge implements TouchToFillComponent.Delegate {
    private long mNativeView;
    private final TouchToFillComponent mTouchToFillComponent;

    private TouchToFillBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mTouchToFillComponent = new TouchToFillCoordinator();
        mTouchToFillComponent.initialize(activity, activity.getBottomSheetController(), this);
    }

    @CalledByNative
    private static TouchToFillBridge create(long nativeView, WindowAndroid windowAndroid) {
        return new TouchToFillBridge(nativeView, windowAndroid);
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
            boolean isPublicSuffixMatch) {
        credentials[index] = new Credential(
                username, password, formattedUsername, originUrl, isPublicSuffixMatch);
    }

    @CalledByNative
    private void showCredentials(String url, boolean isOriginSecure, Credential[] credentials) {
        mTouchToFillComponent.showCredentials(url, isOriginSecure, Arrays.asList(credentials));
    }

    @Override
    public void onDismissed() {
        if (mNativeView != 0) TouchToFillBridgeJni.get().onDismiss(mNativeView);
    }

    @Override
    public void onManagePasswordsSelected() {
        assert mNativeView != 0 : "The native side is already dismissed";
        TouchToFillBridgeJni.get().onManagePasswordsSelected(mNativeView);
    }

    @Override
    public void onCredentialSelected(Credential credential) {
        assert mNativeView != 0 : "The native side is already dismissed";
        TouchToFillBridgeJni.get().onCredentialSelected(mNativeView, credential);
    }

    @Override
    public void fetchFavicon(String credentialOrigin, String frameOrigin, @Px int desiredSize,
            Callback<Bitmap> callback) {
        assert mNativeView != 0 : "Favicon was requested after the bridge was destroyed!";
        TouchToFillBridgeJni.get().fetchFavicon(
                mNativeView, credentialOrigin, frameOrigin, desiredSize, callback);
    }

    @NativeMethods
    interface Natives {
        void onCredentialSelected(long nativeTouchToFillViewImpl, Credential credential);
        void onManagePasswordsSelected(long nativeTouchToFillViewImpl);
        void onDismiss(long nativeTouchToFillViewImpl);
        void fetchFavicon(long nativeTouchToFillViewImpl, String credentialOrigin,
                String fallbackOrigin, int desiredSizeInPx, Callback<Bitmap> callback);
    }
}
