// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge.PasswordStoreObserver;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.password_manager.settings.DialogManager;

/**
 * An alert dialog that offers to delete passwords which were exported.
 */
public class ExportDeletionDialogFragment extends DialogFragment implements PasswordStoreObserver {
    /** The delay after which the progress bar will be displayed. */
    private static final int PROGRESS_BAR_DELAY_MS = 500;

    private PasswordStoreBridge mPasswordStoreBridge;
    private Runnable mHideProgressBarCallback;
    private Dialog mDialog;
    private DialogManager mProgressBarManager;
    private FragmentManager mFragmentManager;

    public ExportDeletionDialogFragment() {}

    public void initialize(FragmentManager fragmentManager, Runnable hideProgressBarCallback,
            PasswordStoreBridge passwordStoreBridge) {
        mFragmentManager = fragmentManager;
        mHideProgressBarCallback = hideProgressBarCallback;
        mPasswordStoreBridge = passwordStoreBridge;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPasswordStoreBridge.addObserver(this, true);
        mProgressBarManager = new DialogManager(null);
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        mDialog =
                new AlertDialog
                        .Builder(getActivity(),
                                R.style.ThemeOverlay_BrowserUI_AlertDialog_NoActionBar)
                        .setPositiveButton(R.string.exported_passwords_delete_button,
                                this::onDeleteButtonClicked)
                        .setNegativeButton(
                                R.string.cancel, (DialogInterface unused, int unusedButton) -> {})
                        .setMessage(
                                getActivity()
                                        .getResources()
                                        .getString(R.string.exported_passwords_deletion_dialog_text)
                                        .replace("%1$s",
                                                PasswordMigrationWarningUtil.getChannelString(
                                                        getActivity().getApplicationContext())))
                        .setTitle(getActivity().getResources().getString(
                                R.string.exported_passwords_deletion_dialog_title))
                        .create();
        return mDialog;
    }

    private void onDeleteButtonClicked(DialogInterface unused, int unusedButton) {
        // Tapping the delete button should show a progress bar and start the deletion.
        showProgressBar();
        mPasswordStoreBridge.clearAllPasswords();
    }

    @Override
    public void onSavedPasswordsChanged(int count) {
        if (count == 0) {
            hideProgressBar();
        }
    }

    @Override
    public void onEdit(PasswordStoreCredential credential) {
        // Won't be used. It's overridden to implement {@link PasswordStoreObserver}.
    }

    @Override
    public void onStart() {
        super.onStart();
        mDialog.findViewById(android.R.id.button1)
                .setAccessibilityTraversalAfter(android.R.id.message);
        mDialog.findViewById(android.R.id.button2)
                .setAccessibilityTraversalAfter(android.R.id.button1);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mPasswordStoreBridge.removeObserver(this);
    }

    private void showProgressBar() {
        NonCancelableProgressBar progressBarDialogFragment = new NonCancelableProgressBar(
                R.string.exported_passwords_deletion_in_progress_title);
        mProgressBarManager.showWithDelay(
                progressBarDialogFragment, mFragmentManager, PROGRESS_BAR_DELAY_MS);
    }

    private void hideProgressBar() {
        mProgressBarManager.hide(mHideProgressBarCallback::run);
    }
}
