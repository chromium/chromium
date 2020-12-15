// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.components.signin.GAIAServiceType;

/**
 * Shows the dialog that explains the user the consequences of signing out of Chrome.
 * Calls the listener callback if the user signs out.
 */
public class SignOutDialogFragment
        extends DialogFragment implements DialogInterface.OnClickListener {
    /**
     * The extra key used to specify the GAIA service that triggered this dialog.
     */
    private static final String SHOW_GAIA_SERVICE_TYPE_EXTRA = "ShowGAIAServiceType";

    /**
     * Receives updates when the user clicks "Sign out".
     */
    public interface SignOutDialogListener {
        /**
         * Called when the user clicks "Sign out".
         *
         * @param forceWipeUserData Whether the user selected to wipe local device data.
         */
        void onSignOutClicked(boolean forceWipeUserData);
    }

    private CheckBox mWipeUserData;

    /**
     * The GAIA service that's prompted this dialog.
     */
    private @GAIAServiceType int mGaiaServiceType = GAIAServiceType.GAIA_SERVICE_TYPE_NONE;

    public static SignOutDialogFragment create(@GAIAServiceType int gaiaServiceType) {
        SignOutDialogFragment signOutFragment = new SignOutDialogFragment();
        Bundle args = new Bundle();
        args.putInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, gaiaServiceType);
        signOutFragment.setArguments(args);
        return signOutFragment;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        if (getArguments() != null) {
            mGaiaServiceType =
                    getArguments().getInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, mGaiaServiceType);
        }
        String domain = IdentityServicesProvider.get()
                                .getSigninManager(Profile.getLastUsedRegularProfile())
                                .getManagementDomain();
        if (domain != null) {
            return createDialogForManagedAccount(domain);
        }

        return createDialog();
    }

    private Dialog createDialogForManagedAccount(String domain) {
        return new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                .setTitle(R.string.signout_managed_account_title)
                .setPositiveButton(R.string.continue_button, this)
                .setNegativeButton(R.string.cancel, this)
                .setMessage(getString(R.string.signout_managed_account_message, domain))
                .create();
    }

    private Dialog createDialog() {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog);
        LayoutInflater inflater = LayoutInflater.from(builder.getContext());
        View body = inflater.inflate(R.layout.signout_wipe_storage_dialog, null);
        mWipeUserData = body.findViewById(R.id.remove_local_data);

        ((TextView) body.findViewById(android.R.id.message)).setText(R.string.signout_message);
        return builder.setTitle(R.string.signout_title)
                .setView(body)
                .setPositiveButton(R.string.continue_button, this)
                .setNegativeButton(R.string.cancel, this)
                .create();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == AlertDialog.BUTTON_POSITIVE) {
            SigninMetricsUtils.logProfileAccountManagementMenu(
                    ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT, mGaiaServiceType);
            SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                    Profile.getLastUsedRegularProfile());
            if (signinManager.getManagementDomain() == null) {
                RecordHistogram.recordBooleanHistogram(
                        "Signin.UserRequestedWipeDataOnSignout", mWipeUserData.isChecked());
            }
            SignOutDialogListener targetFragment = (SignOutDialogListener) getTargetFragment();
            targetFragment.onSignOutClicked(mWipeUserData != null && mWipeUserData.isChecked());
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        SigninMetricsUtils.logProfileAccountManagementMenu(
                ProfileAccountManagementMetrics.SIGNOUT_CANCEL, mGaiaServiceType);
    }
}
