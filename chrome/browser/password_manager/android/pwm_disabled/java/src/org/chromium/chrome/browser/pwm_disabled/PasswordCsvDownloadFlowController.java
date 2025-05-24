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
import org.chromium.chrome.browser.pwm_disabled.PwmDeprecationDialogsMetricsRecorder.DownloadCsvDialogType;
import org.chromium.chrome.browser.pwm_disabled.PwmDeprecationDialogsMetricsRecorder.DownloadCsvFlowStep;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
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

    private @DownloadCsvDialogType int mDialogType;

    private @DownloadCsvFlowStep int mLastFlowStep;

    public PasswordCsvDownloadFlowController(Runnable endOfFlowCallback) {
        mEndOfFlowCallback = endOfFlowCallback;
    }

    /** Starts the CSV download flow by showing the dialog explaining the reason and risks. */
    @Initializer
    public void showDialogAndStartFlow(
            FragmentActivity activity,
            Profile profile,
            boolean isGooglePlayServicesAvailable,
            boolean isPasswordManagerAvailable,
            SettingsCustomTabLauncher settingsCustomTabLauncher) {
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
                        },
                        settingsCustomTabLauncher,
                        this::onDownloadLocationSet);
        mCsvDownloadDialogController.showDialog();
        mLastFlowStep = DownloadCsvFlowStep.DISMISSED_DIALOG;
        mDialogType =
                getCurrentDialogType(isGooglePlayServicesAvailable, isPasswordManagerAvailable);
    }

    private void reauthenticateUser() {
        mReauthenticatorBridge =
                ReauthenticatorBridge.create(
                        mFragmentActivity, mProfile, DeviceAuthSource.PASSWORDS_CSV_DOWNLOAD);
        if (mReauthenticatorBridge.getBiometricAvailabilityStatus()
                == BiometricStatus.UNAVAILABLE) {
            mLastFlowStep = DownloadCsvFlowStep.NO_SCREEN_LOCK;
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
            mCsvDownloadDialogController.askForDownloadLocation();
            return;
        }
        mLastFlowStep = DownloadCsvFlowStep.REAUTH_FAILED;
        dismissDownloadDialog();
        endFlow();
    }

    public void onDownloadLocationSet(Uri destinationFileUri) {
        dismissDownloadDialog();
        if (destinationFileUri == null) {
            mLastFlowStep = DownloadCsvFlowStep.CANCELLED_FILE_SELECTION;
            endFlow();
            return;
        }

        Uri sourceFileUri = getSourceFileUri();
        if (sourceFileUri == null) {
            mLastFlowStep = DownloadCsvFlowStep.CANT_FIND_SOURCE_CSV;
            showErrorDialog();
            return;
        }

        mProgressBarManager = new DialogManager(null);
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
                    mLastFlowStep = DownloadCsvFlowStep.SUCCESS;
                    deleteOriginalFile(sourceFileUri);
                }
                mLastFlowStep = DownloadCsvFlowStep.CSV_WRITE_FAILED;
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
        PwmDeprecationDialogsMetricsRecorder.recordLastStepOfDownloadCsvFlow(
                mDialogType, mLastFlowStep);
        mEndOfFlowCallback.run();
    }

    private @DownloadCsvDialogType int getCurrentDialogType(
            boolean isGooglePlayServicesAvailable, boolean isPasswordManagerAvailable) {
        if (!isGooglePlayServicesAvailable) {
            return DownloadCsvDialogType.NO_GMS;
        }
        if (isPasswordManagerAvailable) {
            return DownloadCsvDialogType.FULL_UPM_SUPPORT_GMS;
        }
        // If the download CSV dialog is shown and Google Play Services is available, but the PWM
        // isn't, the only possible reason is that the Google Play Services version on the device
        // is too old and doesn't have full UPM support.
        return DownloadCsvDialogType.OLD_GMS;
    }

    /**
     * Re-initializes the component after the activity and fragment have been re-created. This is
     * needed in cases in which the system temporarily destroys the current activity, when the file
     * chooser activity if open. Upon coming back to Chrome, the activity and fragment are
     * re-created and they need to be rewired.
     *
     * @param activity The newly created activity.
     * @param fragment The newly created fragment.
     */
    void reinitializeComponent(
            FragmentActivity activity, PasswordCsvDownloadDialogFragment fragment) {
        mFragmentActivity = activity;
        mCsvDownloadDialogController.reinitializeFragment(fragment);
    }
}
