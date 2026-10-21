// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
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
    @JniType("::TouchToFillPasswordManagerViewImpl")
    interface NativeTouchToFillPasswordManagerViewImpl extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeTouchToFillPasswordManagerViewImpl> mNativeView;
    private final TouchToFillComponent mTouchToFillComponent;

    private TouchToFillPasswordManagerBridge(
            JniRawPtr<NativeTouchToFillPasswordManagerViewImpl> nativeView,
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
            JniRawPtr<NativeTouchToFillPasswordManagerViewImpl> nativeView,
            @JniType("Profile*") Profile profile,
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) {
            nativeView.release();
            return null;
        }
        return new TouchToFillPasswordManagerBridge(
                nativeView, profile, windowAndroid, bottomSheetController);
    }

    @CalledByNative
    private void destroy() {
        if (mNativeView != null) {
            mNativeView.release();
            mNativeView = null;
        }
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
        if (mNativeView != null) TouchToFillPasswordManagerBridgeJni.get().onDismiss(mNativeView);
    }

    @Override
    public void onManagePasswordsSelected(boolean passkeysShown) {
        if (mNativeView != null) {
            TouchToFillPasswordManagerBridgeJni.get()
                    .onManagePasswordsSelected(mNativeView, passkeysShown);
        }
    }

    @Override
    public void onHybridSignInSelected() {
        if (mNativeView != null) {
            TouchToFillPasswordManagerBridgeJni.get().onHybridSignInSelected(mNativeView);
        }
    }

    @Override
    public void onCredentialSelected(Credential credential) {
        if (mNativeView != null) {
            TouchToFillPasswordManagerBridgeJni.get().onCredentialSelected(mNativeView, credential);
        }
    }

    @Override
    public void onWebAuthnCredentialSelected(WebauthnCredential credential) {
        if (mNativeView != null) {
            TouchToFillPasswordManagerBridgeJni.get()
                    .onWebAuthnCredentialSelected(mNativeView, credential);
        }
    }

    @Override
    public void onShowMorePasskeysSelected() {
        if (mNativeView == null) return;
        TouchToFillPasswordManagerBridgeJni.get().onShowCredManSelected(mNativeView);
    }

    @NativeMethods
    interface Natives {
        void onCredentialSelected(
                JniPtr<NativeTouchToFillPasswordManagerViewImpl> self, Credential credential);

        void onWebAuthnCredentialSelected(
                JniPtr<NativeTouchToFillPasswordManagerViewImpl> self,
                WebauthnCredential credential);

        void onManagePasswordsSelected(
                JniPtr<NativeTouchToFillPasswordManagerViewImpl> self, boolean passkeysShown);

        void onHybridSignInSelected(JniPtr<NativeTouchToFillPasswordManagerViewImpl> self);

        void onShowCredManSelected(JniPtr<NativeTouchToFillPasswordManagerViewImpl> self);

        void onDismiss(JniPtr<NativeTouchToFillPasswordManagerViewImpl> self);
    }
}
