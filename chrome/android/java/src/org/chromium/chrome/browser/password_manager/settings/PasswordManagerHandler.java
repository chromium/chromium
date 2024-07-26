// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.IntStringCallback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Interface for retrieving passwords and password exceptions (websites for which Chrome should not
 * save password) from native code.
 */
public interface PasswordManagerHandler {
    /** Called to insert a password entry into the password store. */
    public void insertPasswordEntryForTesting(String origin, String username, String password);

    /** Called to start fetching password and exception lists. */
    void updatePasswordLists();

    /**
     * Get the saved password entry at index.
     *
     * @param index Index of Password.
     * @return SavedPasswordEntry at index.
     */
    SavedPasswordEntry getSavedPasswordEntry(int index);

    /**
     * Get saved password exception at index.
     *
     * @param index of exception
     * @return Origin of password exception.
     */
    String getSavedPasswordException(int index);

    /**
     * Remove saved password entry at index.
     *
     * @param index of password entry to remove.
     */
    void removeSavedPasswordEntry(int index);

    /**
     * Remove saved exception entry at index.
     *
     * @param index of exception entry.
     */
    void removeSavedPasswordException(int index);

    /**
     * Trigger serializing the saved passwords in the background.
     *
     * @param targetPath is the file to which the serialized passwords should be written.
     * @param successCallback is called on successful completion, with the count of the serialized
     * passwords and the path to the file containing them as argument.
     * @param errorCallback is called on failure, with the error message as argument.
     */
    void serializePasswords(
            String targetPath, IntStringCallback successCallback, Callback<String> errorCallback);

    /**
     * Show the UI that allows to edit saved credentials.
     *
     * @param context the current Activity to launch the edit view from, or an application context
     *     if no Activity is available.
     * @param index the index of the password entry to edit
     * @param isBlockedCredential whether this credential is blocked for saving
     */
    void showPasswordEntryEditingView(Context context, int index, boolean isBlockedCredential);

    /**
     * Calls C++ to trigger the migration warning. The C++ util centralizes the showing logic such
     * that it can ensure that all appropriate checks are done an that the timestamp for showing the
     * warning is uniformly recorded.
     *
     * @param activity the activity used to show the warning and potentially the export flow.
     * @param bottomSheetController the controller displaying the warning bottom sheet.
     */
    void showMigrationWarning(Activity activity, BottomSheetController bottomSheetController);

    /**
     * Calls C++ to check whether the PasswordManagerHandler is still waiting for the passwords to
     * be fetched from the password store.
     *
     * @return Returns true if the request to fetch the passwords is still pending.
     */
    boolean isWaitingForPasswordStore();
}
