// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

/**
 * A class to display the dialogs the user may encounter when switching to/from or signing into/out
 * of a managed account.
 */
public class ConfirmManagedSyncDataDialog extends DialogFragment {
    /**
     * A listener to allow the Dialog to report on the action taken. Either
     * {@link Listener#onConfirm} or {@link Listener#onCancel} will be called once.
     */
    interface Listener {
        /**
         * The user has accepted the dialog.
         */
        void onConfirm();

        /**
         * The user has cancelled the dialog either through a negative response or by dismissing it.
         */
        void onCancel();
    }

    private static final String KEY_DOMAIN = "domain";

    private Listener mListener;

    /**
     * Creates {@link ConfirmManagedSyncDataDialog} when signing in to a managed account
     * (either through sign in or when switching accounts).
     * @param listener Callback for result.
     * @param domain The domain of the managed account.
     */
    static ConfirmManagedSyncDataDialog create(Listener listener, String domain) {
        ConfirmManagedSyncDataDialog dialog = new ConfirmManagedSyncDataDialog();
        Bundle args = new Bundle();
        args.putString(KEY_DOMAIN, domain);
        dialog.setArguments(args);
        dialog.setListener(listener);
        return dialog;
    }

    private void setListener(Listener listener) {
        assert listener != null;
        mListener = listener;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        if (mListener == null) {
            dismiss();
        }
        String description = getString(
                R.string.sign_in_managed_account_description, getArguments().getString(KEY_DOMAIN));
        return new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                .setTitle(R.string.sign_in_managed_account)
                .setMessage(description)
                .setPositiveButton(
                        R.string.policy_dialog_proceed, (dialog, which) -> mListener.onConfirm())
                .setNegativeButton(R.string.cancel, (dialog, which) -> mListener.onCancel())
                .create();
    }

    @Override
    public void onCancel(DialogInterface dialog) {
        super.onCancel(dialog);
        mListener.onCancel();
    }
}
