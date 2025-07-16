// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.IntStringCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Production implementation of PasswordManagerHandler, making calls to native C++ code to retrieve
 * the data.
 */
@NullMarked
public final class PasswordUiView implements PasswordManagerHandler {
    @CalledByNative
    private static SavedPasswordEntry createSavedPasswordEntry(
            @JniType("std::string") String url,
            @JniType("std::u16string") String name,
            @JniType("std::u16string") String password) {
        return new SavedPasswordEntry(url, name, password);
    }

    // Pointer to native implementation, set to 0 in destroy().
    private long mNativePasswordUiViewAndroid;

    // This class has exactly one observer, set on construction and expected to last at least as
    // long as this object (a good candidate is the owner of this object).
    private final PasswordListObserver mObserver;

    /**
     * Constructor creates the native object as well. Callers should call destroy() after usage.
     *
     * @param observer The only observer.
     * @param profile The {@link Profile} associated with these passwords.
     */
    public PasswordUiView(PasswordListObserver observer, Profile profile) {
        mNativePasswordUiViewAndroid = PasswordUiViewJni.get().init(this, profile);
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
        PasswordUiViewJni.get()
                .insertPasswordEntryForTesting(
                        mNativePasswordUiViewAndroid, origin, username, password);
    }

    // Calls native to refresh password and exception lists. The native code calls back into
    // passwordListAvailable and passwordExceptionListAvailable.
    @Override
    public void updatePasswordLists() {
        PasswordUiViewJni.get().updatePasswordLists(mNativePasswordUiViewAndroid);
    }

    @Override
    public SavedPasswordEntry getSavedPasswordEntry(int index) {
        return PasswordUiViewJni.get().getSavedPasswordEntry(mNativePasswordUiViewAndroid, index);
    }

    @Override
    public String getSavedPasswordException(int index) {
        return PasswordUiViewJni.get()
                .getSavedPasswordException(mNativePasswordUiViewAndroid, index);
    }

    @Override
    public void removeSavedPasswordEntry(int index) {
        PasswordUiViewJni.get().handleRemoveSavedPasswordEntry(mNativePasswordUiViewAndroid, index);
    }

    @Override
    public void removeSavedPasswordException(int index) {
        PasswordUiViewJni.get()
                .handleRemoveSavedPasswordException(mNativePasswordUiViewAndroid, index);
    }

    @Override
    public void serializePasswords(
            String targetPath, IntStringCallback successCallback, Callback<String> errorCallback) {
        PasswordUiViewJni.get()
                .handleSerializePasswords(
                        mNativePasswordUiViewAndroid, targetPath, successCallback, errorCallback);
    }

    @Override
    public void showPasswordEntryEditingView(
            Context context, int index, boolean isBlockedCredential) {
        if (isBlockedCredential) {
            PasswordUiViewJni.get()
                    .handleShowBlockedCredentialView(mNativePasswordUiViewAndroid, context, index);
            return;
        }
        PasswordUiViewJni.get()
                .handleShowPasswordEntryEditingView(mNativePasswordUiViewAndroid, context, index);
    }

    /**
     * Returns the URL for the website for managing one's passwords without the need to use Chrome
     * with the user's profile signed in.
     *
     * @return The string with the URL.
     */
    public static String getAccountDashboardURL() {
        return PasswordUiViewJni.get().getAccountDashboardURL();
    }

    /**
     * Returns the URL of the help center article about trusted vault encryption.
     *
     * @return The string with the URL.
     */
    public static String getTrustedVaultLearnMoreURL() {
        return PasswordUiViewJni.get().getTrustedVaultLearnMoreURL();
    }

    @Override
    public boolean isWaitingForPasswordStore() {
        return PasswordUiViewJni.get().isWaitingForPasswordStore(mNativePasswordUiViewAndroid);
    }

    /** Destroy the native object. */
    public void destroy() {
        if (mNativePasswordUiViewAndroid != 0) {
            PasswordUiViewJni.get().destroy(mNativePasswordUiViewAndroid);
            mNativePasswordUiViewAndroid = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(PasswordUiView self, @JniType("Profile*") Profile profile);

        void insertPasswordEntryForTesting(
                long nativePasswordUiViewAndroid,
                @JniType("std::u16string") String origin,
                @JniType("std::u16string") String username,
                @JniType("std::u16string") String password);

        void updatePasswordLists(long nativePasswordUiViewAndroid);

        SavedPasswordEntry getSavedPasswordEntry(long nativePasswordUiViewAndroid, int index);

        @JniType("std::string")
        String getSavedPasswordException(long nativePasswordUiViewAndroid, int index);

        void handleRemoveSavedPasswordEntry(long nativePasswordUiViewAndroid, int index);

        void handleRemoveSavedPasswordException(long nativePasswordUiViewAndroid, int index);

        @JniType("std::string")
        String getAccountDashboardURL();

        @JniType("std::string")
        String getTrustedVaultLearnMoreURL();

        boolean isWaitingForPasswordStore(long nativePasswordUiViewAndroid);

        void destroy(long nativePasswordUiViewAndroid);

        void handleSerializePasswords(
                long nativePasswordUiViewAndroid,
                @JniType("std::string") String targetPath,
                IntStringCallback successCallback,
                Callback<String> errorCallback);

        void handleShowPasswordEntryEditingView(
                long nativePasswordUiViewAndroid, Context context, int index);

        void handleShowBlockedCredentialView(
                long nativePasswordUiViewAndroid, Context context, int index);
    }
}
