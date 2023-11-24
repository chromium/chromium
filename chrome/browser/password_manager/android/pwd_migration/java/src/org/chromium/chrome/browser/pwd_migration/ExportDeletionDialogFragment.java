// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

/** An alert dialog that offers to delete passwords which were exported. */
public class ExportDeletionDialogFragment extends DialogFragment {
    private Runnable mOnDeletionAcceptedCallback;
    private Dialog mDialog;

    public ExportDeletionDialogFragment() {}

    public void initialize(Runnable onDeletionAcceptedCallback) {
        mOnDeletionAcceptedCallback = onDeletionAcceptedCallback;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        mDialog =
                new AlertDialog.Builder(
                                getActivity(),
                                R.style.ThemeOverlay_BrowserUI_AlertDialog_NoActionBar)
                        .setPositiveButton(
                                R.string.exported_passwords_delete_button,
                                this::onDeleteButtonClicked)
                        .setNegativeButton(
                                R.string.cancel, (DialogInterface unused, int unusedButton) -> {})
                        .setMessage(
                                getActivity()
                                        .getResources()
                                        .getString(R.string.exported_passwords_deletion_dialog_text)
                                        .replace(
                                                "%1$s",
                                                PasswordMigrationWarningUtil.getChannelString(
                                                        getActivity().getApplicationContext())))
                        .setTitle(
                                getActivity()
                                        .getResources()
                                        .getString(
                                                R.string.exported_passwords_deletion_dialog_title))
                        .create();
        return mDialog;
    }

    private void onDeleteButtonClicked(DialogInterface unused, int unusedButton) {
        mOnDeletionAcceptedCallback.run();
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
    }
}
