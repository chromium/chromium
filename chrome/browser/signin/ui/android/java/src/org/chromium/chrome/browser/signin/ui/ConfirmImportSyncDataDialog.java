// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;

import java.util.Arrays;
import java.util.List;

/**
 * A dialog that is displayed when the user switches the account they are syncing to. It gives the
 * option to merge the data of the two accounts or to keep them separate.
 */
public class ConfirmImportSyncDataDialog
        extends DialogFragment implements DialogInterface.OnClickListener {
    /**
     * Callback for completion of the dialog. Only one of {@link Listener#onConfirm} or
     * {@link Listener#onCancel} will be called and it will only be called once.
     */
    interface Listener {
        /**
         * The user has completed the dialog using the positive button.
         * @param wipeData Whether the user requested that existing data should be wiped.
         */
        void onConfirm(boolean wipeData);

        /**
         * The user dismisses the dialog through any means other than the positive button.
         */
        void onCancel();
    }

    private static final String KEY_OLD_ACCOUNT_NAME = "lastAccountName";
    private static final String KEY_NEW_ACCOUNT_NAME = "newAccountName";

    private RadioButtonWithDescription mConfirmImportOption;
    private RadioButtonWithDescription mKeepSeparateOption;

    private Listener mListener;

    /**
     * Creates a new instance of ConfirmImportSyncDataDialog, a dialog that gives the
     * user the option to merge data between the account they are attempting to sign in to and the
     * account they were previously signed into, or to keep the data separate.
     * @param listener        Callback to be called if the user completes the dialog (as opposed to
     *                        hitting cancel).
     * @param oldAccountName  The previous sync account name.
     * @param newAccountName  The potential next sync account name.
     */
    static ConfirmImportSyncDataDialog create(
            Listener listener, String oldAccountName, String newAccountName) {
        ConfirmImportSyncDataDialog fragment = new ConfirmImportSyncDataDialog();
        Bundle args = new Bundle();
        args.putString(KEY_OLD_ACCOUNT_NAME, oldAccountName);
        args.putString(KEY_NEW_ACCOUNT_NAME, newAccountName);
        fragment.setArguments(args);
        fragment.setListener(listener);
        return fragment;
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
        String oldAccountName = getArguments().getString(KEY_OLD_ACCOUNT_NAME);
        String newAccountName = getArguments().getString(KEY_NEW_ACCOUNT_NAME);

        LayoutInflater inflater = getActivity().getLayoutInflater();
        View v = inflater.inflate(R.layout.confirm_import_sync_data, null);

        TextView prompt = v.findViewById(R.id.sync_import_data_prompt);
        prompt.setText(getActivity().getString(R.string.sync_import_data_prompt, oldAccountName));

        mConfirmImportOption = v.findViewById(R.id.sync_confirm_import_choice);
        mKeepSeparateOption = v.findViewById(R.id.sync_keep_separate_choice);

        mConfirmImportOption.setDescriptionText(getActivity().getString(
                R.string.sync_import_existing_data_subtext, newAccountName));
        mKeepSeparateOption.setDescriptionText(getActivity().getString(
                R.string.sync_keep_existing_data_separate_subtext_existing_data));

        List<RadioButtonWithDescription> radioGroup =
                Arrays.asList(mConfirmImportOption, mKeepSeparateOption);
        mConfirmImportOption.setRadioButtonGroup(radioGroup);
        mKeepSeparateOption.setRadioButtonGroup(radioGroup);

        boolean isManagedAccount = IdentityServicesProvider.get()
                                           .getSigninManager(Profile.getLastUsedRegularProfile())
                                           .getManagementDomain()
                != null;
        final AlertDialog alertDialog =
                new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                        .setPositiveButton(R.string.continue_button, this)
                        .setNegativeButton(R.string.cancel, this)
                        .setView(v)
                        .create();
        // For non-managed accounts, the confirmation button starts out disabled, since none of the
        // options are chosen by default.
        alertDialog.setOnShowListener(dialog
                -> alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(isManagedAccount));

        // If the account is managed, disallow merging information.
        if (isManagedAccount) {
            mKeepSeparateOption.setChecked(true);
            mConfirmImportOption.setOnClickListener(
                    view -> ManagedPreferencesUtils.showManagedByAdministratorToast(getActivity()));
        } else {
            // The confirmation button gets enabled as soon as either of the radio button options
            // was selected.
            mConfirmImportOption.setOnCheckedChangeListener(radioButton
                    -> alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(true));
            mKeepSeparateOption.setOnCheckedChangeListener(radioButton
                    -> alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(true));
        }

        return alertDialog;
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == AlertDialog.BUTTON_POSITIVE) {
            assert mConfirmImportOption.isChecked() ^ mKeepSeparateOption.isChecked();

            RecordUserAction.record(mKeepSeparateOption.isChecked()
                            ? "Signin_ImportDataPrompt_DontImport"
                            : "Signin_ImportDataPrompt_ImportData");
            mListener.onConfirm(mKeepSeparateOption.isChecked());
        } else {
            assert which == AlertDialog.BUTTON_NEGATIVE;

            RecordUserAction.record("Signin_ImportDataPrompt_Cancel");
            mListener.onCancel();
        }
    }

    @Override
    public void onCancel(DialogInterface dialog) {
        super.onCancel(dialog);
        mListener.onCancel();
    }
}
