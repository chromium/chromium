// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.chrome.browser.password_manager.R;

/**
 * Shows the dialog that gives the user some tips for how to treat the exported passwords securely.
 */
public class ExportWarningDialogFragment extends DialogFragment {
    /**
     * This interface combines handling the clicks on the buttons and the general dismissal of the
     * dialog.
     */
    public interface Handler extends DialogInterface.OnClickListener {
        /** Handle the dismissal of the dialog.*/
        void onDismiss();
    }

    // This handler is used to answer the user actions on the dialog.
    private Handler mHandler;

    public void setExportWarningHandler(Handler handler) {
        mHandler = handler;
    }

    /**
     * Opens the dialog with the warning and sets the button listener to a fragment identified by ID
     * passed in arguments.
     */
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        return new AlertDialog
                .Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_AlertDialog_NoActionBar)
                .setPositiveButton(R.string.password_settings_export_action_title, mHandler)
                .setNegativeButton(R.string.cancel, mHandler)
                .setMessage(
                        getActivity()
                                .getResources()
                                .getString(R.string.settings_passwords_export_description))
                .create();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // If there is savedInstanceState, then the dialog is being recreated by Android and will
        // lack the necessary callbacks. Dismiss immediately as the settings page will need to be
        // recreated anyway.
        if (savedInstanceState != null) {
            dismiss();
            return;
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        // Attempt to inform |mHandler| that this fragment is being dismissed, so that the passwords
        // settings page knows that the user aborted the export flow. However, not all dismissals
        // are caused by the user aborting the process: e.g., when Chrome is killed while in
        // background and then the user returns, this fragment dismisses itself (see onCreate
        // above). In those cases, |mHandler| is not set and the settings page does not expect the
        // notification about user's cancellation (there was no cancellation). Hence, not calling
        // the handler if it does not exist is expected and correct.
        if (mHandler != null) mHandler.onDismiss();
    }
}
