// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.password_entry_edit.CredentialEditCoordinator.CredentialActionDelegate;
import org.chromium.chrome.browser.password_entry_edit.CredentialEditCoordinator.UiDismissalHandler;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

/**
 * Class mediating the communication between the credential edit UI and the C++ part responsible for
 * saving the changes.
 */
class CredentialEditBridge implements UiDismissalHandler, CredentialActionDelegate {
    private static CredentialEditBridge sCredentialEditBridge;

    private long mNativeCredentialEditBridge;
    private CredentialEditCoordinator mCoordinator;

    static @Nullable CredentialEditBridge get() {
        return sCredentialEditBridge;
    }

    private CredentialEditBridge() {}

    @CalledByNative
    static @Nullable CredentialEditBridge maybeCreate() {
        // There can only be one bridge at a time and it shouldn't be shared.
        if (sCredentialEditBridge != null) return null;
        sCredentialEditBridge = new CredentialEditBridge();
        return sCredentialEditBridge;
    }

    @CalledByNative
    void initAndLaunchUi(
            long nativeCredentialEditBridge,
            Context context,
            boolean isBlockedCredential,
            boolean isFederatedCredential) {
        mNativeCredentialEditBridge = nativeCredentialEditBridge;
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        if (isBlockedCredential) {
            settingsNavigation.startSettings(context, BlockedCredentialFragmentView.class);
            return;
        }
        if (isFederatedCredential) {
            settingsNavigation.startSettings(context, FederatedCredentialFragmentView.class);
            return;
        }
        settingsNavigation.startSettings(context, CredentialEditFragmentView.class);
    }

    public void initialize(CredentialEditCoordinator coordinator) {
        mCoordinator = coordinator;
        // This will result in setCredential being called from native with the required data.
        CredentialEditBridgeJni.get().getCredential(mNativeCredentialEditBridge);

        // This will result in setExistingUsernames being called from native with the required data.
        CredentialEditBridgeJni.get().getExistingUsernames(mNativeCredentialEditBridge);
    }

    @CalledByNative
    void setCredential(
            String displayUrlOrAppName,
            String username,
            String password,
            String displayFederationOrigin,
            boolean isInsecureCredential) {
        mCoordinator.setCredential(
                displayUrlOrAppName,
                username,
                password,
                displayFederationOrigin,
                isInsecureCredential);
    }

    @CalledByNative
    void setExistingUsernames(String[] existingUsernames) {
        mCoordinator.setExistingUsernames(existingUsernames);
    }

    // This can be called either before or after the native counterpart has gone away, depending
    // on where the edit component is being destroyed from.
    @Override
    public void onUiDismissed() {
        if (mNativeCredentialEditBridge != 0) {
            CredentialEditBridgeJni.get().onUIDismissed(mNativeCredentialEditBridge);
        }
        mNativeCredentialEditBridge = 0;
        sCredentialEditBridge = null;
    }

    @Override
    public void saveChanges(String username, String password) {
        if (mNativeCredentialEditBridge == 0) return;
        CredentialEditBridgeJni.get().saveChanges(mNativeCredentialEditBridge, username, password);
    }

    @Override
    public void deleteCredential() {
        CredentialEditBridgeJni.get().deleteCredential(mNativeCredentialEditBridge);
    }

    @CalledByNative
    void destroy() {
        if (mCoordinator != null) mCoordinator.dismiss();
        mNativeCredentialEditBridge = 0;
        sCredentialEditBridge = null;
    }

    @NativeMethods
    interface Natives {
        void getCredential(long nativeCredentialEditBridge);

        void getExistingUsernames(long nativeCredentialEditBridge);

        void saveChanges(long nativeCredentialEditBridge, String username, String password);

        void deleteCredential(long nativeCredentialEditBridge);

        void onUIDismissed(long nativeCredentialEditBridge);
    }
}
