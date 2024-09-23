// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import static org.chromium.chrome.browser.password_entry_edit.CredentialEditMediator.CredentialEditError.DUPLICATE_USERNAME;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditMediator.CredentialEditError.EMPTY_PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditMediator.CredentialEditError.ERROR_COUNT;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditMediator.CredentialEntryAction.ACTION_COUNT;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditMediator.CredentialEntryAction.COPIED_PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditMediator.CredentialEntryAction.COPIED_USERNAME;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditMediator.CredentialEntryAction.DELETED;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditMediator.CredentialEntryAction.EDITED_PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditMediator.CredentialEntryAction.EDITED_USERNAME;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditMediator.CredentialEntryAction.EDITED_USERNAME_AND_PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditMediator.CredentialEntryAction.MASKED_PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditMediator.CredentialEntryAction.UNMASKED_PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.DUPLICATE_USERNAME_ERROR;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.EMPTY_PASSWORD_ERROR;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.FEDERATION_ORIGIN;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD_VISIBLE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.UI_DISMISSED_BY_NATIVE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.URL_OR_APP;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.USERNAME;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.password_entry_edit.CredentialEditCoordinator.CredentialActionDelegate;
import org.chromium.chrome.browser.password_entry_edit.CredentialEntryFragmentViewBase.UiActionHandler;
import org.chromium.chrome.browser.password_manager.ConfirmationDialogHelper;
import org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper;
import org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper.ReauthReason;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Contains the logic for the edit component. It  updates the model when needed and reacts to UI
 * events (e.g. button clicks).
 */
public class CredentialEditMediator implements UiActionHandler {
    static final String SAVED_PASSWORD_ACTION_HISTOGRAM =
            "PasswordManager.CredentialEntryActions.SavedPassword";
    static final String FEDERATED_CREDENTIAL_ACTION_HISTOGRAM =
            "PasswordManager.CredentialEntryActions.FederatedCredential";
    static final String BLOCKED_CREDENTIAL_ACTION_HISTOGRAM =
            "PasswordManager.CredentialEntryActions.BlockedCredential";
    private final PasswordAccessReauthenticationHelper mReauthenticationHelper;
    private final ConfirmationDialogHelper mDeleteDialogHelper;
    private final CredentialActionDelegate mCredentialActionDelegate;
    private final Runnable mHelpLauncher;
    private final boolean mIsBlockedCredential;
    private PropertyModel mModel;
    private String mOriginalUsername;
    private String mOriginalPassword;
    private boolean mIsInsecureCredential;
    private Set<String> mExistingUsernames;

    /**
     * The action that the user takes within the credential entry UI.
     *
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused.
     */
    @IntDef({
        DELETED,
        COPIED_USERNAME,
        UNMASKED_PASSWORD,
        MASKED_PASSWORD,
        COPIED_PASSWORD,
        EDITED_USERNAME,
        EDITED_PASSWORD,
        EDITED_USERNAME_AND_PASSWORD,
        ACTION_COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CredentialEntryAction {
        /**
         * The credential entry was deleted. Recorded after the user confirms it, when a
         * confirmation dialog is prompted.
         */
        int DELETED = 0;

        /** The username was copied. */
        int COPIED_USERNAME = 1;

        /** The password was unmasked. Recorded after successful reauth is one was performed. */
        int UNMASKED_PASSWORD = 2;

        /** The password was masked. */
        int MASKED_PASSWORD = 3;

        /** The password was copied. Recorded after successful reauth is one was performed. */
        int COPIED_PASSWORD = 4;

        /** The username was edited. Recorded after the user presses the save button". */
        int EDITED_USERNAME = 5;

        /** The password was edited. Recorded after the user presses the save button". */
        int EDITED_PASSWORD = 6;

        /**
         * Both username and password were edited. Recorded after the user presses the save button".
         */
        int EDITED_USERNAME_AND_PASSWORD = 7;

        int ACTION_COUNT = 8;
    }

    /**
     *  The error displayed in the UI while the user is editing a credential.
     *
     *  These values are persisted to logs. Entries should not be renumbered and
     *  numeric values should never be reused.
     */
    @IntDef({EMPTY_PASSWORD, DUPLICATE_USERNAME, ERROR_COUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CredentialEditError {
        /** The password field is empty. */
        int EMPTY_PASSWORD = 0;

        /** The username in the username field is already saved for this site/app. */
        int DUPLICATE_USERNAME = 1;

        int ERROR_COUNT = 2;
    }

    CredentialEditMediator(
            PasswordAccessReauthenticationHelper reauthenticationHelper,
            ConfirmationDialogHelper deleteDialogHelper,
            CredentialActionDelegate credentialActionDelegate,
            Runnable helpLauncher,
            boolean isBlockedCredential) {
        mReauthenticationHelper = reauthenticationHelper;
        mDeleteDialogHelper = deleteDialogHelper;
        mCredentialActionDelegate = credentialActionDelegate;
        mHelpLauncher = helpLauncher;
        mIsBlockedCredential = isBlockedCredential;
    }

    void initialize(PropertyModel model) {
        mModel = model;
    }

    void setCredential(String username, String password, boolean isInsecureCredential) {
        mOriginalUsername = username;
        mOriginalPassword = password;
        mIsInsecureCredential = isInsecureCredential;

        mModel.set(USERNAME, username);
        mModel.set(PASSWORD_VISIBLE, false);
        mModel.set(PASSWORD, password);
    }

    void setExistingUsernames(String[] existingUsernames) {
        mExistingUsernames = new HashSet<>(Arrays.asList(existingUsernames));
    }

    void dismiss() {
        mModel.set(UI_DISMISSED_BY_NATIVE, true);
    }

    @Override
    public void onMaskOrUnmaskPassword() {
        if (mModel.get(PASSWORD_VISIBLE)) {
            RecordHistogram.recordEnumeratedHistogram(
                    SAVED_PASSWORD_ACTION_HISTOGRAM, MASKED_PASSWORD, ACTION_COUNT);
            mModel.set(PASSWORD_VISIBLE, false);
            return;
        }
        reauthenticateUser(
                ReauthReason.VIEW_PASSWORD,
                (reauthSucceeded) -> {
                    if (!reauthSucceeded) return;
                    RecordHistogram.recordEnumeratedHistogram(
                            SAVED_PASSWORD_ACTION_HISTOGRAM, UNMASKED_PASSWORD, ACTION_COUNT);
                    mModel.set(PASSWORD_VISIBLE, true);
                });
    }

    @Override
    public void onSave() {
        recordSavedEdit();
        mCredentialActionDelegate.saveChanges(mModel.get(USERNAME), mModel.get(PASSWORD));
    }

    @Override
    public void onUsernameTextChanged(String username) {
        mModel.set(USERNAME, username);
        boolean hasError =
                !mOriginalUsername.equals(username) && mExistingUsernames.contains(username);
        mModel.set(DUPLICATE_USERNAME_ERROR, hasError);
    }

    @Override
    public void onPasswordTextChanged(String password) {
        mModel.set(PASSWORD, password);
        mModel.set(EMPTY_PASSWORD_ERROR, password.isEmpty());
    }

    @Override
    public void onCopyUsername(Context context) {
        recordUsernameCopied();
        Clipboard.getInstance().setText("username", mModel.get(USERNAME));
        Toast.makeText(
                        context,
                        R.string.password_entry_viewer_username_copied_into_clipboard,
                        Toast.LENGTH_SHORT)
                .show();
    }

    @Override
    public void onDelete() {
        if (mIsBlockedCredential) {
            recordDeleted();
            mCredentialActionDelegate.deleteCredential();
            return;
        }
        Resources resources = mDeleteDialogHelper.getResources();
        if (resources == null) return;
        String title =
                resources.getString(R.string.password_entry_edit_delete_credential_dialog_title);
        String message =
                resources.getString(
                        mIsInsecureCredential
                                ? R.string.password_check_delete_credential_dialog_body
                                : R.string.password_entry_edit_deletion_dialog_body,
                        mModel.get(URL_OR_APP));
        mDeleteDialogHelper.showConfirmation(
                title,
                message,
                R.string.password_entry_edit_delete_credential_dialog_confirm,
                () -> {
                    recordDeleted();
                    mCredentialActionDelegate.deleteCredential();
                });
    }

    @Override
    public void handleHelp() {
        mHelpLauncher.run();
    }

    @Override
    public void onCopyPassword(Context context) {
        reauthenticateUser(
                ReauthReason.COPY_PASSWORD,
                (reauthSucceeded) -> {
                    if (!reauthSucceeded) return;
                    RecordHistogram.recordEnumeratedHistogram(
                            SAVED_PASSWORD_ACTION_HISTOGRAM, COPIED_PASSWORD, ACTION_COUNT);
                    Clipboard.getInstance().setPassword(mModel.get(PASSWORD));
                    Toast.makeText(
                                    context,
                                    R.string.password_entry_viewer_password_copied_into_clipboard,
                                    Toast.LENGTH_SHORT)
                            .show();
                });
    }

    private void reauthenticateUser(@ReauthReason int reason, Callback<Boolean> action) {
        if (!mReauthenticationHelper.canReauthenticate()) {
            mReauthenticationHelper.showScreenLockToast(reason);
            return;
        }
        mReauthenticationHelper.reauthenticate(reason, action);
    }

    private void recordUsernameCopied() {
        String histogram =
                mModel.get(FEDERATION_ORIGIN).isEmpty()
                        ? SAVED_PASSWORD_ACTION_HISTOGRAM
                        : FEDERATED_CREDENTIAL_ACTION_HISTOGRAM;
        RecordHistogram.recordEnumeratedHistogram(histogram, COPIED_USERNAME, ACTION_COUNT);
    }

    private void recordDeleted() {
        String histogram = SAVED_PASSWORD_ACTION_HISTOGRAM;
        if (mIsBlockedCredential) {
            histogram = BLOCKED_CREDENTIAL_ACTION_HISTOGRAM;
        } else if (!mModel.get(FEDERATION_ORIGIN).isEmpty()) {
            histogram = FEDERATED_CREDENTIAL_ACTION_HISTOGRAM;
        }
        RecordHistogram.recordEnumeratedHistogram(histogram, DELETED, ACTION_COUNT);
    }

    private void recordSavedEdit() {
        boolean changedUsername = !mModel.get(USERNAME).equals(mOriginalUsername);
        boolean changedPassword = !mModel.get(PASSWORD).equals(mOriginalPassword);
        if (changedUsername && changedPassword) {
            RecordHistogram.recordEnumeratedHistogram(
                    SAVED_PASSWORD_ACTION_HISTOGRAM, EDITED_USERNAME_AND_PASSWORD, ACTION_COUNT);
            return;
        }

        if (changedUsername) {
            RecordHistogram.recordEnumeratedHistogram(
                    SAVED_PASSWORD_ACTION_HISTOGRAM, EDITED_USERNAME, ACTION_COUNT);
            return;
        }

        if (changedPassword) {
            RecordHistogram.recordEnumeratedHistogram(
                    SAVED_PASSWORD_ACTION_HISTOGRAM, EDITED_PASSWORD, ACTION_COUNT);
        }
    }
}
