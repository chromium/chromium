// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
@NullMarked
class TouchToFillPasswordManagerBridge implements TouchToFillComponent.Delegate {
    private long mNativeView;
    private final TouchToFillComponent mTouchToFillComponent;

    private TouchToFillPasswordManagerBridge(
            long nativeView,
            Profile profile,
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController) {
        mNativeView = nativeView;
        Context context = windowAndroid.getContext().get();
        assert context != null;
        mTouchToFillComponent = new TouchToFillPasswordManagerCoordinator();
        mTouchToFillComponent.initialize(
                context,
                profile,
                bottomSheetController,
                this,
                new BottomSheetFocusHelper(bottomSheetController, windowAndroid));
    }

    @CalledByNative
    private static @Nullable TouchToFillPasswordManagerBridge create(
            long nativeView,
            @JniType("Profile*") Profile profile,
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;
        return new TouchToFillPasswordManagerBridge(
                nativeView, profile, windowAndroid, bottomSheetController);
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
            @JniType("std::u16string") String username,
            @JniType("std::u16string") String password,
            @JniType("std::u16string") String formattedUsername,
            @JniType("std::string") String originUrl,
            @JniType("std::string") String displayName,
            @GetLoginMatchType int matchType,
            long lastUsedMsSinceEpoch,
            boolean isShared,
            @JniType("std::u16string") String senderName,
            @JniType("GURL") GURL senderProfileImageUrl,
            boolean sharingNotificationDisplayed,
            boolean isBackupCredential) {
        credentials[index] =
                new Credential.Builder()
                        .setUsername(username)
                        .setPassword(password)
                        .setFormattedUsername(formattedUsername)
                        .setOriginUrl(originUrl)
                        .setDisplayName(displayName)
                        .setMatchType(matchType)
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
            @JniType("std::string") String rpId,
            @JniType("std::vector<uint8_t>") byte[] credentialId,
            @JniType("std::vector<uint8_t>") byte[] userId,
            @JniType("std::u16string") String username) {
        credentials[index] = new WebauthnCredential(rpId, credentialId, userId, username);
    }

    @CalledByNative
    private void showCredentials(
            @JniType("GURL") GURL url,
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
        if (mNativeView != 0) TouchToFillPasswordManagerBridgeJni.get().onDismiss(mNativeView);
    }

    @Override
    public void onManagePasswordsSelected(boolean passkeysShown) {
        if (mNativeView != 0) {
            TouchToFillPasswordManagerBridgeJni.get()
                    .onManagePasswordsSelected(mNativeView, passkeysShown);
        }
    }

    @Override
    public void onHybridSignInSelected() {
        if (mNativeView != 0) {
            TouchToFillPasswordManagerBridgeJni.get().onHybridSignInSelected(mNativeView);
        }
    }

    @Override
    public void onCredentialSelected(Credential credential) {
        if (mNativeView != 0) {
            TouchToFillPasswordManagerBridgeJni.get().onCredentialSelected(mNativeView, credential);
        }
    }

    @Override
    public void onWebAuthnCredentialSelected(WebauthnCredential credential) {
        if (mNativeView != 0) {
            TouchToFillPasswordManagerBridgeJni.get()
                    .onWebAuthnCredentialSelected(mNativeView, credential);
        }
    }

    @Override
    public void onShowMorePasskeysSelected() {
        if (mNativeView == 0) return;
        TouchToFillPasswordManagerBridgeJni.get().onShowCredManSelected(mNativeView);
    }

    @NativeMethods
    interface Natives {
        void onCredentialSelected(
                long nativeTouchToFillPasswordManagerViewImpl, Credential credential);

        void onWebAuthnCredentialSelected(
                long nativeTouchToFillPasswordManagerViewImpl, WebauthnCredential credential);

        void onManagePasswordsSelected(
                long nativeTouchToFillPasswordManagerViewImpl, boolean passkeysShown);

        void onHybridSignInSelected(long nativeTouchToFillPasswordManagerViewImpl);

        void onShowCredManSelected(long nativeTouchToFillPasswordManagerViewImpl);

        void onDismiss(long nativeTouchToFillPasswordManagerViewImpl);
    }
}
