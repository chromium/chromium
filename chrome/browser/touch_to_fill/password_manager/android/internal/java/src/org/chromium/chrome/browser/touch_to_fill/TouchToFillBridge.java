// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.password_manager.GetLoginMatchType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.CredentialBase;
import org.chromium.chrome.browser.touch_to_fill.data.WebauthnCredential;
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

    private TouchToFillBridge(
            long nativeView,
            Profile profile,
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController) {
        mNativeView = nativeView;
        mTouchToFillComponent = new TouchToFillCoordinator();
        mTouchToFillComponent.initialize(
                windowAndroid.getContext().get(),
                profile,
                bottomSheetController,
                this,
                new BottomSheetFocusHelper(bottomSheetController, windowAndroid));
    }

    @CalledByNative
    private static @Nullable TouchToFillBridge create(
            long nativeView, Profile profile, WindowAndroid windowAndroid) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;
        return new TouchToFillBridge(nativeView, profile, windowAndroid, bottomSheetController);
    }

    @CalledByNative
    private void destroy() {
        mNativeView = 0;
        mTouchToFillComponent.cleanUp();
    }

    @CalledByNative
    private static CredentialBase[] createCredentialArray(int size) {
        return new CredentialBase[size];
    }

    @CalledByNative
    private static void insertCredential(
            CredentialBase[] credentials,
            int index,
            String username,
            String password,
            String formattedUsername,
            String originUrl,
            String displayName,
            @GetLoginMatchType int mMatchType,
            long lastUsedMsSinceEpoch,
            boolean isShared,
            String senderName,
            GURL senderProfileImageUrl,
            boolean sharingNotificationDisplayed,
            boolean isBackupCredential) {
        credentials[index] =
                new Credential.Builder()
                        .setUsername(username)
                        .setPassword(password)
                        .setFormattedUsername(formattedUsername)
                        .setOriginUrl(originUrl)
                        .setDisplayName(displayName)
                        .setMatchType(mMatchType)
                        .setLastUsedMsSinceEpoch(lastUsedMsSinceEpoch)
                        .setIsShared(isShared)
                        .setSenderName(senderName)
                        .setSenderProfileImageUrl(senderProfileImageUrl)
                        .setSharingNotificationDisplayed(sharingNotificationDisplayed)
                        .setIsBackupCredential(isBackupCredential)
                        .build();
    }

    @CalledByNative
    private static void insertWebAuthnCredential(
            CredentialBase[] credentials,
            int index,
            String rpId,
            byte[] credentialId,
            byte[] userId,
            String username) {
        credentials[index] = new WebauthnCredential(rpId, credentialId, userId, username);
    }

    @CalledByNative
    private void showCredentials(
            GURL url,
            boolean isOriginSecure,
            CredentialBase[] credentials,
            boolean submitCredential,
            boolean showHybridPasskeyOption,
            boolean showCredManEntry) {
        mTouchToFillComponent.showCredentials(
                url,
                isOriginSecure,
                Arrays.asList(credentials),
                submitCredential,
                showHybridPasskeyOption,
                showCredManEntry);
    }

    @Override
    public void onDismissed() {
        if (mNativeView != 0) TouchToFillBridgeJni.get().onDismiss(mNativeView);
    }

    @Override
    public void onManagePasswordsSelected(boolean passkeysShown) {
        if (mNativeView != 0) {
            TouchToFillBridgeJni.get().onManagePasswordsSelected(mNativeView, passkeysShown);
        }
    }

    @Override
    public void onHybridSignInSelected() {
        if (mNativeView != 0) {
            TouchToFillBridgeJni.get().onHybridSignInSelected(mNativeView);
        }
    }

    @Override
    public void onCredentialSelected(Credential credential) {
        if (mNativeView != 0) {
            TouchToFillBridgeJni.get().onCredentialSelected(mNativeView, credential);
        }
    }

    @Override
    public void onWebAuthnCredentialSelected(WebauthnCredential credential) {
        if (mNativeView != 0) {
            TouchToFillBridgeJni.get().onWebAuthnCredentialSelected(mNativeView, credential);
        }
    }

    @Override
    public void onShowMorePasskeysSelected() {
        if (mNativeView == 0) return;
        TouchToFillBridgeJni.get().onShowCredManSelected(mNativeView);
    }

    @NativeMethods
    interface Natives {
        void onCredentialSelected(long nativeTouchToFillViewImpl, Credential credential);

        void onWebAuthnCredentialSelected(
                long nativeTouchToFillViewImpl, WebauthnCredential credential);

        void onManagePasswordsSelected(long nativeTouchToFillViewImpl, boolean passkeysShown);

        void onHybridSignInSelected(long nativeTouchToFillViewImpl);

        void onShowCredManSelected(long nativeTouchToFillViewImpl);

        void onDismiss(long nativeTouchToFillViewImpl);
    }
}
