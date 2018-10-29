// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import org.chromium.base.Callback;
import org.chromium.base.IntStringCallback;
import org.chromium.base.annotations.CalledByNative;

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
     * @param PasswordListObserver The only observer.
     */
    public PasswordUIView(PasswordListObserver observer) {
        mNativePasswordUIViewAndroid = nativeInit();
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
        nativeUpdatePasswordLists(mNativePasswordUIViewAndroid);
    }

    @Override
    public SavedPasswordEntry getSavedPasswordEntry(int index) {
        return nativeGetSavedPasswordEntry(mNativePasswordUIViewAndroid, index);
    }

    @Override
    public String getSavedPasswordException(int index) {
        return nativeGetSavedPasswordException(mNativePasswordUIViewAndroid, index);
    }

    @Override
    public void removeSavedPasswordEntry(int index) {
        nativeHandleRemoveSavedPasswordEntry(mNativePasswordUIViewAndroid, index);
    }

    @Override
    public void removeSavedPasswordException(int index) {
        nativeHandleRemoveSavedPasswordException(mNativePasswordUIViewAndroid, index);
    }

    @Override
    public void serializePasswords(
            String targetPath, IntStringCallback successCallback, Callback<String> errorCallback) {
        nativeHandleSerializePasswords(
                mNativePasswordUIViewAndroid, targetPath, successCallback, errorCallback);
    }

    /**
     * Returns the URL for the website for managing one's passwords without the need to use Chrome
     * with the user's profile signed in.
     * @return The string with the URL.
     */
    public static String getAccountDashboardURL() {
        return nativeGetAccountDashboardURL();
    }

    /**
     * Destroy the native object.
     */
    public void destroy() {
        if (mNativePasswordUIViewAndroid != 0) {
            nativeDestroy(mNativePasswordUIViewAndroid);
            mNativePasswordUIViewAndroid = 0;
        }
    }

    private native long nativeInit();

    private native void nativeUpdatePasswordLists(long nativePasswordUIViewAndroid);

    private native SavedPasswordEntry nativeGetSavedPasswordEntry(
            long nativePasswordUIViewAndroid, int index);

    private native String nativeGetSavedPasswordException(
            long nativePasswordUIViewAndroid, int index);

    private native void nativeHandleRemoveSavedPasswordEntry(
            long nativePasswordUIViewAndroid, int index);

    private native void nativeHandleRemoveSavedPasswordException(
            long nativePasswordUIViewAndroid, int index);

    private static native String nativeGetAccountDashboardURL();

    private native void nativeDestroy(long nativePasswordUIViewAndroid);

    private native void nativeHandleSerializePasswords(long nativePasswordUIViewAndroid,
            String targetPath, IntStringCallback successCallback, Callback<String> errorCallback);
}
