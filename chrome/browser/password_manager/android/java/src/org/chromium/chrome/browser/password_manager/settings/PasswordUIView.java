// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.app.Activity;
import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.IntStringCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Production implementation of PasswordManagerHandler, making calls to native C++ code to retrieve
 * the data.
 */
public final class PasswordUIView implements PasswordManagerHandler {
    @CalledByNative
    private static SavedPasswordEntry createSavedPasswordEntry(
            @JniType("std::string") String url,
            @JniType("std::u16string") String name,
            @JniType("std::u16string") String password) {
        return new SavedPasswordEntry(url, name, password);
    }

    // Pointer to native implementation, set to 0 in destroy().
    private long mNativePasswordUIViewAndroid;

    // This class has exactly one observer, set on construction and expected to last at least as
    // long as this object (a good candidate is the owner of this object).
    private final PasswordListObserver mObserver;

    /**
     * Constructor creates the native object as well. Callers should call destroy() after usage.
     *
     * @param observer The only observer.
     * @param profile The {@link Profile} associated with these passwords.
     */
    public PasswordUIView(PasswordListObserver observer, Profile profile) {
        mNativePasswordUIViewAndroid = PasswordUIViewJni.get().init(PasswordUIView.this, profile);
        mObserver = observer;
    }

    @CalledByNative
    private void passwordListAvailable(int count) {
        mObserver.passwordListAvailable(count);
    }

    @CalledByNative
    private void passwordExceptionListAvailable(int count) {
        mObserver.passwordExceptionListAvailable(count);
    }

    @Override
    public void insertPasswordEntryForTesting(String origin, String username, String password) {
        PasswordUIViewJni.get()
                .insertPasswordEntryForTesting(
                        mNativePasswordUIViewAndroid, origin, username, password);
    }

    // Calls native to refresh password and exception lists. The native code calls back into
    // passwordListAvailable and passwordExceptionListAvailable.
    @Override
    public void updatePasswordLists() {
        PasswordUIViewJni.get()
                .updatePasswordLists(mNativePasswordUIViewAndroid, PasswordUIView.this);
    }

    @Override
    public SavedPasswordEntry getSavedPasswordEntry(int index) {
        return PasswordUIViewJni.get()
                .getSavedPasswordEntry(mNativePasswordUIViewAndroid, PasswordUIView.this, index);
    }

    @Override
    public String getSavedPasswordException(int index) {
        return PasswordUIViewJni.get()
                .getSavedPasswordException(
                        mNativePasswordUIViewAndroid, PasswordUIView.this, index);
    }

    @Override
    public void removeSavedPasswordEntry(int index) {
        PasswordUIViewJni.get()
                .handleRemoveSavedPasswordEntry(
                        mNativePasswordUIViewAndroid, PasswordUIView.this, index);
    }

    @Override
    public void removeSavedPasswordException(int index) {
        PasswordUIViewJni.get()
                .handleRemoveSavedPasswordException(
                        mNativePasswordUIViewAndroid, PasswordUIView.this, index);
    }

    @Override
    public void serializePasswords(
            String targetPath, IntStringCallback successCallback, Callback<String> errorCallback) {
        PasswordUIViewJni.get()
                .handleSerializePasswords(
                        mNativePasswordUIViewAndroid,
                        PasswordUIView.this,
                        targetPath,
                        successCallback,
                        errorCallback);
    }

    @Override
    public void showPasswordEntryEditingView(
            Context context, int index, boolean isBlockedCredential) {
        if (isBlockedCredential) {
            PasswordUIViewJni.get()
                    .handleShowBlockedCredentialView(
                            mNativePasswordUIViewAndroid, context, index, PasswordUIView.this);
            return;
        }
        PasswordUIViewJni.get()
                .handleShowPasswordEntryEditingView(
                        mNativePasswordUIViewAndroid, context, index, PasswordUIView.this);
    }

    @Override
    public void showMigrationWarning(
            Activity activity, BottomSheetController bottomSheetController) {
        if (mNativePasswordUIViewAndroid == 0) return;
        PasswordUIViewJni.get()
                .showMigrationWarning(
                        mNativePasswordUIViewAndroid, activity, bottomSheetController);
    }

    /**
     * Returns the URL for the website for managing one's passwords without the need to use Chrome
     * with the user's profile signed in.
     *
     * @return The string with the URL.
     */
    public static String getAccountDashboardURL() {
        return PasswordUIViewJni.get().getAccountDashboardURL();
    }

    /**
     * Returns the URL of the help center article about trusted vault encryption.
     *
     * @return The string with the URL.
     */
    public static String getTrustedVaultLearnMoreURL() {
        return PasswordUIViewJni.get().getTrustedVaultLearnMoreURL();
    }

    @Override
    public boolean isWaitingForPasswordStore() {
        return PasswordUIViewJni.get()
                .isWaitingForPasswordStore(mNativePasswordUIViewAndroid, PasswordUIView.this);
    }

    /** Destroy the native object. */
    public void destroy() {
        if (mNativePasswordUIViewAndroid != 0) {
            PasswordUIViewJni.get().destroy(mNativePasswordUIViewAndroid, PasswordUIView.this);
            mNativePasswordUIViewAndroid = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(PasswordUIView caller, @JniType("Profile*") Profile profile);

        void insertPasswordEntryForTesting(
                long nativePasswordUIViewAndroid,
                @JniType("std::u16string") String origin,
                @JniType("std::u16string") String username,
                @JniType("std::u16string") String password);

        void updatePasswordLists(long nativePasswordUIViewAndroid, PasswordUIView caller);

        SavedPasswordEntry getSavedPasswordEntry(
                long nativePasswordUIViewAndroid, PasswordUIView caller, int index);

        @JniType("std::string")
        String getSavedPasswordException(
                long nativePasswordUIViewAndroid, PasswordUIView caller, int index);

        void handleRemoveSavedPasswordEntry(
                long nativePasswordUIViewAndroid, PasswordUIView caller, int index);

        void handleRemoveSavedPasswordException(
                long nativePasswordUIViewAndroid, PasswordUIView caller, int index);

        @JniType("std::string")
        String getAccountDashboardURL();

        @JniType("std::string")
        String getTrustedVaultLearnMoreURL();

        boolean isWaitingForPasswordStore(long nativePasswordUIViewAndroid, PasswordUIView caller);

        void destroy(long nativePasswordUIViewAndroid, PasswordUIView caller);

        void handleSerializePasswords(
                long nativePasswordUIViewAndroid,
                PasswordUIView caller,
                @JniType("std::string") String targetPath,
                IntStringCallback successCallback,
                Callback<String> errorCallback);

        void handleShowPasswordEntryEditingView(
                long nativePasswordUIViewAndroid,
                Context context,
                int index,
                PasswordUIView caller);

        void handleShowBlockedCredentialView(
                long nativePasswordUIViewAndroid,
                Context context,
                int index,
                PasswordUIView caller);

        void showMigrationWarning(
                long nativePasswordUIViewAndroid,
                Activity activity,
                BottomSheetController bottomSheetController);
    }
}
