// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.chrome.R;

/**
 * Shows the dialog that explains to the user the error which just happened during exporting and
 * optionally helps them to take actions to fix that (learning more, retrying export).
 */
public class ExportErrorDialogFragment extends DialogFragment {
    /** Parameters to fill in the strings in the dialog. Pass them through {@link #initialize()}.*/
    public static class ErrorDialogParams {
        /** The string resource ID for the label of the positive button. Required. */
        public int positiveButtonLabelId;

        /** The main description of the error. Required. */
        public String description;

        /**
         * An optional detailed description. Will be prefixed with "Details:" and displayed below
         * the main one.
         */
        @Nullable
        public String detailedDescription;
    }

    // This handler is used to answer the user actions on the dialog.
    private DialogInterface.OnClickListener mHandler;

    /** Defines the strings to be shown. Set in {@link #initialize()}. */
    private ErrorDialogParams mParams;

    /**
     * Sets the click handler for the dialog buttons.
     * @param mHandler The handler to use.
     */
    public void setExportErrorHandler(DialogInterface.OnClickListener handler) {
        mHandler = handler;
    }

    /**
     * Opens the dialog with the warning and sets the button listener to a fragment identified by ID
     * passed in arguments.
     */
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        assert mParams != null;
        final View dialog =
                getActivity().getLayoutInflater().inflate(R.layout.passwords_error_dialog, null);
        final TextView mainDescription =
                (TextView) dialog.findViewById(R.id.passwords_error_main_description);
        mainDescription.setText(mParams.description);
        final TextView detailedDescription =
                (TextView) dialog.findViewById(R.id.passwords_error_detailed_description);
        if (mParams.detailedDescription != null) {
            detailedDescription.setText(mParams.detailedDescription);
        } else {
            detailedDescription.setVisibility(View.GONE);
        }
        return new AlertDialog
                .Builder(getActivity(), R.style.Theme_Chromium_AlertDialog_NoActionBar)
                .setView(dialog)
                .setTitle(R.string.password_settings_export_error_title)
                .setPositiveButton(mParams.positiveButtonLabelId, mHandler)
                .setNegativeButton(R.string.close, mHandler)
                .create();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // If there is savedInstanceState, then the dialog is being recreated by Android and will
        // lack the necessary callbacks. The user likely already saw it first and then replaced the
        // current activity. Therefore just close the dialog.
        if (savedInstanceState != null) {
            dismiss();
            return;
        }
    }

    /**
     * Set the parameters for the strings to be shown. Must be called exactly once, before the
     * dialog is shown.
     */
    public void initialize(ErrorDialogParams params) {
        assert mParams == null && params != null;
        assert params.positiveButtonLabelId != 0;
        assert params.description != null;
        mParams = params;
    }
}
