// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import android.net.Uri;

import androidx.fragment.app.FragmentActivity;

import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.DeviceAuthSource;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.widget.Toast;

/** Oversees the multiple steps of the password CSV download flow. */
public class PasswordCsvDownloadFlowController {
    private final Runnable mEndOfFlowCallback;
    private PasswordCsvDownloadDialogController mCsvDownloadDialogController;
    private Profile mProfile;
    private FragmentActivity mFragmentActivity;
    private ReauthenticatorBridge mReauthenticatorBridge;

    public PasswordCsvDownloadFlowController(Runnable endOfFlowCallback) {
        mEndOfFlowCallback = endOfFlowCallback;
    }

    /** Starts the CSV download flow by showing the dialog explaining the reason and risks. */
    public void showDialogAndStartFlow(
            FragmentActivity activity, Profile profile, boolean isGooglePlayServicesAvailable) {
        mProfile = profile;
        mFragmentActivity = activity;
        mCsvDownloadDialogController =
                new PasswordCsvDownloadDialogController(
                        activity,
                        isGooglePlayServicesAvailable,
                        this::reauthenticateUser,
                        this::endFlow);
        mCsvDownloadDialogController.showDialog();
    }

    private void reauthenticateUser() {
        mReauthenticatorBridge =
                ReauthenticatorBridge.create(
                        mFragmentActivity, mProfile, DeviceAuthSource.PASSWORDS_CSV_DOWNLOAD);
        if (mReauthenticatorBridge.getBiometricAvailabilityStatus()
                == BiometricStatus.UNAVAILABLE) {
            Toast.makeText(
                            mFragmentActivity.getApplicationContext(),
                            R.string.password_export_set_lock_screen,
                            Toast.LENGTH_LONG)
                    .show();
            endFlow();
            return;
        }
        mReauthenticatorBridge.reauthenticate(this::onReauthResult);
    }

    private void onReauthResult(boolean success) {
        if (success) {
            mCsvDownloadDialogController.askForDownloadLocation(this::onDownloadLocationSet);
            return;
        }
        endFlow();
    }

    private void onDownloadLocationSet(Uri destinationFileUri) {
        // TODO(crbug.com/378653384): Write file and delete the old one.
        endFlow();
    }

    private void endFlow() {
        mCsvDownloadDialogController.dismiss();
        mEndOfFlowCallback.run();
    }
}
