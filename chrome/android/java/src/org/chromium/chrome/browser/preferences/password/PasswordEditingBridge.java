// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;

/**
 * This is a bridge between PasswordEntryEditor and the C++ code. The bridge is in charge of
 * launching the PasswordEntryEditor and handling the password changes that happen through the
 * PasswordEntryEditor.
 */
public class PasswordEditingBridge implements PasswordEditingDelegate {
    private long mNativePasswordEditingBridge;

    public PasswordEditingBridge(long nativePasswordEditingBridge) {
        mNativePasswordEditingBridge = nativePasswordEditingBridge;
        PasswordEditingDelegateProvider.getInstance().setPasswordEditingDelegate(this);
    }

    /**
     * The method edits a password form saved in the password store according to changes performed
     * in PasswordEntryEditor. The delegate holds all the information about the password form that
     * was loaded in the PasswordEntryEditor, so there's no need to pass the site, the old username
     * or the old password to this method. Sometimes the form can have no username (for PSL-matched
     * credentials), but it has to always have a password.
     *
     * @param newUsername that will replace the old one if it's given.
     * @param newPassword that will replace the old one.
     */
    @Override
    public void editSavedPasswordEntry(String newUsername, String newPassword) {
        PasswordEditingBridgeJni.get().handleEditSavedPasswordEntry(
                mNativePasswordEditingBridge, PasswordEditingBridge.this, newUsername, newPassword);
    }

    @CalledByNative
    private static PasswordEditingBridge create(long nativePasswordEditingBridge) {
        return new PasswordEditingBridge(nativePasswordEditingBridge);
    }

    @CalledByNative
    private void showEditingUI(Context context, String site, String username, String password) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_URL, site);
        fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_NAME, username);
        fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_PASSWORD, password);
        PreferencesLauncher.launchSettingsPage(context, PasswordEntryEditor.class, fragmentArgs);
    }

    /**
     * Destroy the native object.
     */
    @Override
    public void destroy() {
        PasswordEditingDelegateProvider.getInstance().setPasswordEditingDelegate(null);
        assert mNativePasswordEditingBridge != 0;
        PasswordEditingBridgeJni.get().destroy(
                mNativePasswordEditingBridge, PasswordEditingBridge.this);
        mNativePasswordEditingBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void destroy(long nativePasswordEditingBridge, PasswordEditingBridge caller);
        void handleEditSavedPasswordEntry(long nativePasswordEditingBridge,
                PasswordEditingBridge caller, String newUsername, String newPassword);
    }
}
