// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.IntStringCallback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * Production implementation of PasswordManagerHandler, making calls to native C++ code to retrieve
 * the data.
 */
public final class PasswordUIView implements PasswordManagerHandler {
    @CalledByNative
    private static SavedPasswordEntry createSavedPasswordEntry(
            String url, String name, String password) {
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
     * @param PasswordListObserver The only observer.
     */
    public PasswordUIView(PasswordListObserver observer) {
        mNativePasswordUIViewAndroid = PasswordUIViewJni.get().init(PasswordUIView.this);
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

    // Calls native to refresh password and exception lists. The native code calls back into
    // passwordListAvailable and passwordExceptionListAvailable.
    @Override
    public void updatePasswordLists() {
        PasswordUIViewJni.get().updatePasswordLists(
                mNativePasswordUIViewAndroid, PasswordUIView.this);
    }

    @Override
    public SavedPasswordEntry getSavedPasswordEntry(int index) {
        return PasswordUIViewJni.get().getSavedPasswordEntry(
                mNativePasswordUIViewAndroid, PasswordUIView.this, index);
    }

    @Override
    public String getSavedPasswordException(int index) {
        return PasswordUIViewJni.get().getSavedPasswordException(
                mNativePasswordUIViewAndroid, PasswordUIView.this, index);
    }

    @Override
    public void removeSavedPasswordEntry(int index) {
        PasswordUIViewJni.get().handleRemoveSavedPasswordEntry(
                mNativePasswordUIViewAndroid, PasswordUIView.this, index);
    }

    @Override
    public void removeSavedPasswordException(int index) {
        PasswordUIViewJni.get().handleRemoveSavedPasswordException(
                mNativePasswordUIViewAndroid, PasswordUIView.this, index);
    }

    @Override
    public void serializePasswords(
            String targetPath, IntStringCallback successCallback, Callback<String> errorCallback) {
        PasswordUIViewJni.get().handleSerializePasswords(mNativePasswordUIViewAndroid,
                PasswordUIView.this, targetPath, successCallback, errorCallback);
    }

    @Override
    public void showPasswordEntryEditingView(Context context, int index) {
        PasswordUIViewJni.get().handleShowPasswordEntryEditingView(
                mNativePasswordUIViewAndroid, PasswordUIView.this, context, index);
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

    public static boolean hasAccountForLeakCheckRequest() {
        return PasswordUIViewJni.get().hasAccountForLeakCheckRequest();
    }

    /**
     * Destroy the native object.
     */
    public void destroy() {
        if (mNativePasswordUIViewAndroid != 0) {
            PasswordUIViewJni.get().destroy(mNativePasswordUIViewAndroid, PasswordUIView.this);
            mNativePasswordUIViewAndroid = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(PasswordUIView caller);
        void updatePasswordLists(long nativePasswordUIViewAndroid, PasswordUIView caller);
        SavedPasswordEntry getSavedPasswordEntry(
                long nativePasswordUIViewAndroid, PasswordUIView caller, int index);
        String getSavedPasswordException(
                long nativePasswordUIViewAndroid, PasswordUIView caller, int index);
        void handleRemoveSavedPasswordEntry(
                long nativePasswordUIViewAndroid, PasswordUIView caller, int index);
        void handleRemoveSavedPasswordException(
                long nativePasswordUIViewAndroid, PasswordUIView caller, int index);
        String getAccountDashboardURL();
        boolean hasAccountForLeakCheckRequest();
        void destroy(long nativePasswordUIViewAndroid, PasswordUIView caller);
        void handleSerializePasswords(long nativePasswordUIViewAndroid, PasswordUIView caller,
                String targetPath, IntStringCallback successCallback,
                Callback<String> errorCallback);
        void handleShowPasswordEntryEditingView(long nativePasswordUIViewAndroid,
                PasswordUIView caller, Context context, int index);
    }
}
