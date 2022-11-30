// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.IntStringCallback;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

import java.util.ArrayList;

/**
 * Fake implementation for the PasswordManagerHandler.
 */
final class FakePasswordManagerHandler implements PasswordManagerHandler {
    // This class has exactly one observer, set on construction and expected to last at least as
    // long as this object (a good candidate is the owner of this object).
    private final PasswordListObserver mObserver;

    // The faked contents of the password store to be displayed.
    private ArrayList<SavedPasswordEntry> mSavedPasswords = new ArrayList<SavedPasswordEntry>();

    // The faked contents of the saves password exceptions to be displayed.
    private ArrayList<String> mSavedPasswordExeptions = new ArrayList<>();

    // The following three data members are set once {@link #serializePasswords()} is called.
    @Nullable
    private IntStringCallback mExportSuccessCallback;

    @Nullable
    private Callback<String> mExportErrorCallback;

    @Nullable
    private String mExportTargetPath;

    void setSavedPasswords(ArrayList<SavedPasswordEntry> savedPasswords) {
        mSavedPasswords = savedPasswords;
    }

    void setSavedPasswordExceptions(ArrayList<String> savedPasswordExceptions) {
        mSavedPasswordExeptions = savedPasswordExceptions;
    }

    IntStringCallback getExportSuccessCallback() {
        return mExportSuccessCallback;
    }

    Callback<String> getExportErrorCallback() {
        return mExportErrorCallback;
    }

    String getExportTargetPath() {
        return mExportTargetPath;
    }

    /**
     * Constructor.
     * @param observer The only observer.
     */
    FakePasswordManagerHandler(PasswordListObserver observer) {
        mObserver = observer;
    }

    @Override
    public void insertPasswordEntryForTesting(String origin, String username, String password) {
        mSavedPasswords.add(new SavedPasswordEntry(origin, username, password));
    }

    // Pretends that the updated lists are |mSavedPasswords| for the saved passwords and an
    // empty list for exceptions and immediately calls the observer.
    @Override
    public void updatePasswordLists() {
        mObserver.passwordListAvailable(mSavedPasswords.size());
        mObserver.passwordExceptionListAvailable(mSavedPasswordExeptions.size());
    }

    @Override
    public SavedPasswordEntry getSavedPasswordEntry(int index) {
        return mSavedPasswords.get(index);
    }

    @Override
    public String getSavedPasswordException(int index) {
        return mSavedPasswordExeptions.get(index);
    }

    @Override
    public void removeSavedPasswordEntry(int index) {
        assert false : "Define this method before starting to use it in tests.";
    }

    @Override
    public void removeSavedPasswordException(int index) {
        assert false : "Define this method before starting to use it in tests.";
    }

    @Override
    public void serializePasswords(
            String targetPath, IntStringCallback successCallback, Callback<String> errorCallback) {
        mExportSuccessCallback = successCallback;
        mExportErrorCallback = errorCallback;
        mExportTargetPath = targetPath;
    }

    @Override
    public void showPasswordEntryEditingView(
            Context context, SettingsLauncher launcher, int index, boolean isBlockedCredential) {
        assert false : "Define this method before starting to use it in tests.";
    }
}
