// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.DialogInterface;
import android.net.Uri;

import androidx.fragment.app.FragmentActivity;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.DeviceAuthSource;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridge;
import org.chromium.chrome.browser.password_manager.settings.DialogManager;
import org.chromium.chrome.browser.password_manager.settings.ExportErrorDialogFragment;
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
@NullMarked
public class PasswordCsvDownloadFlowController {
    private final Runnable mEndOfFlowCallback;
    private PasswordCsvDownloadDialogController mCsvDownloadDialogController;
    private Profile mProfile;
    private FragmentActivity mFragmentActivity;
    private @Nullable ReauthenticatorBridge mReauthenticatorBridge;
    private @Nullable DialogManager mProgressBarManager;

    public PasswordCsvDownloadFlowController(Runnable endOfFlowCallback) {
        mEndOfFlowCallback = endOfFlowCallback;
    }

    /** Starts the CSV download flow by showing the dialog explaining the reason and risks. */
    @Initializer
    public void showDialogAndStartFlow(
            FragmentActivity activity, Profile profile, boolean isGooglePlayServicesAvailable) {
        mProfile = profile;
        mFragmentActivity = activity;
        mCsvDownloadDialogController =
                new PasswordCsvDownloadDialogController(
                        activity,
                        isGooglePlayServicesAvailable,
                        this::reauthenticateUser,
                        () -> {
                            dismissDownloadDialog();
                            endFlow();
                        });
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
            dismissDownloadDialog();
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
        dismissDownloadDialog();
        endFlow();
    }

    private void onDownloadLocationSet(Uri destinationFileUri) {
        dismissDownloadDialog();
        mProgressBarManager = new DialogManager(null);
        Uri sourceFileUri = getSourceFileUri();
        if (sourceFileUri == null) {
            showErrorDialog();
            return;
        }
        mProgressBarManager.show(
                new NonCancelableProgressBar(R.string.passwords_export_in_progress_title),
                mFragmentActivity.getSupportFragmentManager());
        new AsyncTask<@Nullable Exception>() {
            @Override
            protected @Nullable Exception doInBackground() {
                try {
                    copyInternalCsvToSelectedDocument(sourceFileUri, destinationFileUri);
                } catch (IOException e) {
                    return e;
                }
                return null;
            }

            @Override
            protected void onPostExecute(@Nullable Exception exception) {
                if (exception == null) {
                    deleteOriginalFile(sourceFileUri);
                }
                assumeNonNull(mProgressBarManager);
                mProgressBarManager.hide(exception == null ? null : () -> showErrorDialog());
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private @Nullable Uri getSourceFileUri() {
        String autoExportedFilePath =
                LoginDbDeprecationUtilBridge.getAutoExportCsvFilePath(mProfile);
        File autoExportedFile = new File(autoExportedFilePath);
        Uri sourceFileUri = Uri.fromFile(autoExportedFile);
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
                assumeNonNull(fileInputStream);
                assumeNonNull(fileOutputStream);
                FileUtils.copyStream(fileInputStream, fileOutputStream);
            }
        }
    }

    private void deleteOriginalFile(Uri fileUri) {
        File file = new File(assumeNonNull(fileUri.getPath()));
        if (!file.delete()) {
            // The deletion will be re-attempted later.
            UserPrefs.get(mProfile).setBoolean(Pref.UPM_AUTO_EXPORT_CSV_NEEDS_DELETION, true);
        }
        endFlow();
    }

    private void showErrorDialog() {
        ExportErrorDialogFragment exportErrorDialogFragment = new ExportErrorDialogFragment();
        ExportErrorDialogFragment.ErrorDialogParams params =
                new ExportErrorDialogFragment.ErrorDialogParams();
        params.positiveButtonLabelId = 0;
        params.description =
                mFragmentActivity.getResources().getString(R.string.password_settings_export_tips);
        exportErrorDialogFragment.initialize(params);
        exportErrorDialogFragment.setExportErrorHandler(
                (DialogInterface dialog, int which) -> endFlow());
        exportErrorDialogFragment.show(mFragmentActivity.getSupportFragmentManager(), null);
    }

    private void dismissDownloadDialog() {
        mCsvDownloadDialogController.dismiss();
    }

    private void endFlow() {
        mEndOfFlowCallback.run();
    }
}
