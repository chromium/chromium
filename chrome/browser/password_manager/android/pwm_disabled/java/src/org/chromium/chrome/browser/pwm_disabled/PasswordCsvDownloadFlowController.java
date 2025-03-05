// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import android.net.Uri;

import androidx.fragment.app.FragmentActivity;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.DeviceAuthSource;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridge;
import org.chromium.chrome.browser.password_manager.settings.DialogManager;
import org.chromium.chrome.browser.password_manager.settings.NonCancelableProgressBar;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.widget.Toast;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/** Oversees the multiple steps of the password CSV download flow. */
public class PasswordCsvDownloadFlowController {
    private final Runnable mEndOfFlowCallback;
    private PasswordCsvDownloadDialogController mCsvDownloadDialogController;
    private Profile mProfile;
    private FragmentActivity mFragmentActivity;
    private ReauthenticatorBridge mReauthenticatorBridge;
    private DialogManager mProgressBarManager;

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
        mProgressBarManager = new DialogManager(null);
        Uri sourceFileUri = getSourceFileUri();
        if (sourceFileUri == null) {
            endFlow();
            return;
        }
        mProgressBarManager.show(
                new NonCancelableProgressBar(R.string.passwords_export_in_progress_title),
                mFragmentActivity.getSupportFragmentManager());
        new AsyncTask<String>() {
            @Override
            protected String doInBackground() {
                try {
                    copyInternalCsvToSelectedDocument(sourceFileUri, destinationFileUri);
                } catch (IOException e) {
                    return e.getMessage();
                }
                return null;
            }

            @Override
            protected void onPostExecute(String exceptionMessage) {
                if (exceptionMessage == null) {
                    deleteOriginalFile(sourceFileUri);
                }
                // TODO(crbug.com/378653384): Add error dialog.
                mProgressBarManager.hide(null);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        endFlow();
    }

    private Uri getSourceFileUri() {
        String autoExportedFilePath =
                LoginDbDeprecationUtilBridge.getAutoExportCsvFilePath(mProfile);
        File autoExportedFile = new File(autoExportedFilePath);
        Uri sourceFileUri = null;
        try {
            sourceFileUri = Uri.fromFile(autoExportedFile);
        } catch (IllegalArgumentException e) {
            // TODO(crbug.com/378653384): Record metrics and show error either here or in the
            // caller.
        }
        return sourceFileUri;
    }

    private void copyInternalCsvToSelectedDocument(Uri sourceUri, Uri destinationUri)
            throws IOException {
        try (InputStream fileInputStream =
                ContextUtils.getApplicationContext()
                        .getContentResolver()
                        .openInputStream(sourceUri)) {
            try (OutputStream fileOutputStream =
                    ContextUtils.getApplicationContext()
                            .getContentResolver()
                            .openOutputStream(destinationUri)) {
                FileUtils.copyStream(fileInputStream, fileOutputStream);
            }
        }
    }

    private void deleteOriginalFile(Uri fileUri) {
        File file = new File(fileUri.getPath());
        if (!file.delete()) {
            // The deletion will be re-attempted later.
            UserPrefs.get(mProfile).setBoolean(Pref.UPM_AUTO_EXPORT_CSV_NEEDS_DELETION, true);
        }
    }

    private void endFlow() {
        mCsvDownloadDialogController.dismiss();
        mEndOfFlowCallback.run();
    }
}
