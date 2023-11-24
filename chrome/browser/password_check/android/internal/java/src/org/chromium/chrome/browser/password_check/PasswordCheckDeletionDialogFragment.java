// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.app.Dialog;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;

/** Shows the dialog that confirms the user really wants to delete a credential. */
public class PasswordCheckDeletionDialogFragment extends PasswordCheckDialogFragment {
    // This handler is used to answer the user actions on the dialog.
    private final String mOrigin;

    PasswordCheckDeletionDialogFragment(Handler handler, String origin) {
        super(handler);
        mOrigin = origin;
    }

    /**
     * Opens the dialog with the confirmation and sets the button listener to a fragment identified
     * by ID passed in arguments.
     */
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        return new AlertDialog.Builder(
                        getActivity(), R.style.ThemeOverlay_BrowserUI_AlertDialog_NoActionBar)
                .setTitle(R.string.password_entry_edit_delete_credential_dialog_title)
                .setPositiveButton(
                        R.string.password_entry_edit_delete_credential_dialog_confirm, mHandler)
                .setNegativeButton(R.string.password_check_credential_dialog_cancel, mHandler)
                .setMessage(
                        getString(R.string.password_check_delete_credential_dialog_body, mOrigin))
                .create();
    }
}
