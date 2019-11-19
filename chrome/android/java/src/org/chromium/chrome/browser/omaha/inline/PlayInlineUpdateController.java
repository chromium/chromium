// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import android.app.Activity;
import android.content.IntentSender.SendIntentException;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import com.google.android.play.core.appupdate.AppUpdateInfo;
import com.google.android.play.core.appupdate.AppUpdateManager;
import com.google.android.play.core.install.InstallState;
import com.google.android.play.core.install.InstallStateUpdatedListener;
import com.google.android.play.core.install.model.AppUpdateType;
import com.google.android.play.core.install.model.InstallErrorCode;
import com.google.android.play.core.install.model.InstallStatus;
import com.google.android.play.core.install.model.UpdateAvailability;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class for gluing interactions with the Play store's AppUpdateManager with Chrome.  This
 * involves hooking up to Play as a listener for install state changes, should only happen if we are
 * in the foreground.
 */
public class PlayInlineUpdateController
        implements InlineUpdateController, InstallStateUpdatedListener {
    /**
     * Converts Play's InstallErrorCode enum to a stable monotomically incrementing Chrome enum.
     * This is used for metric stability.
     * Treat this as append only as it is used by UMA.
     */
    @IntDef({InstallErrorCodeMetrics.NO_ERROR, InstallErrorCodeMetrics.NO_ERROR_PARTIALLY_ALLOWED,
            InstallErrorCodeMetrics.ERROR_UNKNOWN, InstallErrorCodeMetrics.ERROR_API_NOT_AVAILABLE,
            InstallErrorCodeMetrics.ERROR_INVALID_REQUEST,
            InstallErrorCodeMetrics.ERROR_INSTALL_UNAVAILABLE,
            InstallErrorCodeMetrics.ERROR_INSTALL_NOT_ALLOWED,
            InstallErrorCodeMetrics.ERROR_DOWNLOAD_NOT_PRESENT,
            InstallErrorCodeMetrics.ERROR_INTERNAL_ERROR, InstallErrorCodeMetrics.ERROR_UNTRACKED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface InstallErrorCodeMetrics {
        int NO_ERROR = 0;
        int NO_ERROR_PARTIALLY_ALLOWED = 1;
        int ERROR_UNKNOWN = 2;
        int ERROR_API_NOT_AVAILABLE = 3;
        int ERROR_INVALID_REQUEST = 4;
        int ERROR_INSTALL_UNAVAILABLE = 5;
        int ERROR_INSTALL_NOT_ALLOWED = 6;
        int ERROR_DOWNLOAD_NOT_PRESENT = 7;
        int ERROR_INTERNAL_ERROR = 8;
        int ERROR_UNTRACKED = 9;

        int NUM_ENTRIES = 10;
    }

    /**
     * Converts Play's {@link UpdateAvailability} enum to a stable monotomically incrementing Chrome
     * enum. This is used for metric stability.
     * Treat this as append only as it is used by UMA as the InlineUpdateAvailability enum.
     */
    @IntDef({UpdateAvailabilityMetric.UNTRACKED, UpdateAvailabilityMetric.UNKNOWN,
            UpdateAvailabilityMetric.UPDATE_NOT_AVAILABLE,
            UpdateAvailabilityMetric.UPDATE_AVAILABLE,
            UpdateAvailabilityMetric.DEVELOPER_TRIGGERED_UPDATE_IN_PROGRESS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UpdateAvailabilityMetric {
        int UNTRACKED = 0;
        int UNKNOWN = 1;
        int UPDATE_NOT_AVAILABLE = 2;
        int UPDATE_AVAILABLE = 3;
        int DEVELOPER_TRIGGERED_UPDATE_IN_PROGRESS = 4;

        int NUM_ENTRIES = 5;
    }

    /**
     * Converts Play's {@link InstallStatus} enum to a stable monotomically incrementing Chrome
     * enum. This is used for metric stability.
     * Treat this as append only as it is used by UMA as the InlineUpdateAvailability enum.
     */
    @IntDef({InstallStatusMetric.UNTRACKED, InstallStatusMetric.UNKNOWN,
            InstallStatusMetric.REQUIRES_UI_INTENT, InstallStatusMetric.PENDING,
            InstallStatusMetric.DOWNLOADING, InstallStatusMetric.DOWNLOADED,
            InstallStatusMetric.INSTALLING, InstallStatusMetric.INSTALLED,
            InstallStatusMetric.FAILED, InstallStatusMetric.CANCELED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface InstallStatusMetric {
        int UNTRACKED = 0;
        int UNKNOWN = 1;
        int REQUIRES_UI_INTENT = 2;
        int PENDING = 3;
        int DOWNLOADING = 4;
        int DOWNLOADED = 5;
        int INSTALLING = 6;
        int INSTALLED = 7;
        int FAILED = 8;
        int CANCELED = 9;

        int NUM_ENTRIES = 10;
    }

    /**
     * A list of possible Play API call site failures.
     * Treat this as append only as it is used by UMA.
     */
    @IntDef({CallFailure.START_FAILED, CallFailure.START_EXCEPTION, CallFailure.COMPLETE_FAILED,
            CallFailure.QUERY_FAILED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface CallFailure {
        int START_FAILED = 0;
        int START_EXCEPTION = 1;
        int COMPLETE_FAILED = 2;
        int QUERY_FAILED = 3;

        int NUM_ENTRIES = 4;
    }

    private static final String TAG = "PlayInline";
    private static final int RESULT_IN_APP_UPDATE_FAILED = 1;
    private static final int REQUEST_CODE = 8123;

    private final Runnable mCallback;
    private final AppUpdateManager mAppUpdateManager;

    private boolean mEnabled;
    private @Nullable @UpdateState Integer mUpdateState;

    private AppUpdateInfo mAppUpdateInfo;
    private @Nullable @UpdateAvailability Integer mUpdateAvailability;
    private @Nullable @InstallStatus Integer mInstallStatus;

    /**
     * Builds an instance of {@link PlayInlineUpdateController}.
     * @param callback The {@link Runnable} to notify when an inline update state change occurs.
     */
    PlayInlineUpdateController(Runnable callback, AppUpdateManager appUpdateManager) {
        mCallback = callback;
        mAppUpdateManager = appUpdateManager;
        setEnabled(true);
    }

    // InlineUpdateController implementation.
    @Override
    public void setEnabled(boolean enabled) {
        if (mEnabled == enabled) return;
        mEnabled = enabled;

        if (mEnabled) {
            mUpdateState = UpdateState.NONE;
            mAppUpdateManager.registerListener(this);
            pullCurrentState();
        } else {
            mAppUpdateManager.unregisterListener(this);
        }
    }

    @Override
    public @Nullable @UpdateState Integer getStatus() {
        return mUpdateState;
    }

    @Override
    public void startUpdate(Activity activity) {
        try {
            boolean success = mAppUpdateManager.startUpdateFlowForResult(
                    mAppUpdateInfo, AppUpdateType.FLEXIBLE, activity, REQUEST_CODE);
            Log.i(TAG, "startUpdateFlowForResult() returned " + success);

            if (!success) recordCallFailure(CallFailure.START_FAILED);
        } catch (SendIntentException exception) {
            mInstallStatus = InstallStatus.FAILED;
            Log.i(TAG, "startUpdateFlowForResult() threw an exception.");
            recordCallFailure(CallFailure.START_EXCEPTION);
        }
        // TODO(dtrainor): Use success.
    }

    @Override
    public void completeUpdate() {
        mAppUpdateManager.completeUpdate()
                .addOnSuccessListener(unused -> {
                    Log.i(TAG, "completeUpdate() success.");
                    pushStatus();
                })
                .addOnFailureListener(exception -> {
                    Log.i(TAG, "completeUpdate() failed.");
                    recordCallFailure(CallFailure.COMPLETE_FAILED);
                    mInstallStatus = InstallStatus.FAILED;
                    pushStatus();
                });
    }

    // InstallStateUpdatedListener implementation.
    @Override
    public void onStateUpdate(InstallState state) {
        Log.i(TAG,
                "onStateUpdate(" + state.installStatus() + ", " + state.installErrorCode() + ")");

        if (state.installStatus() != mInstallStatus) {
            RecordHistogram.recordEnumeratedHistogram("GoogleUpdate.Inline.StateChange.Error."
                            + installStatusToEnumSuffix(state.installStatus()),
                    installErrorCodeToMetrics(state.installErrorCode()),
                    InstallErrorCodeMetrics.NUM_ENTRIES);
        }

        mInstallStatus = state.installStatus();
        pushStatus();
    }

    private void pullCurrentState() {
        mAppUpdateManager.getAppUpdateInfo()
                .addOnSuccessListener(info -> {
                    mAppUpdateInfo = info;
                    mUpdateAvailability = info.updateAvailability();
                    mInstallStatus = info.installStatus();
                    Log.i(TAG,
                            "pullCurrentState(" + mUpdateAvailability + ", " + mInstallStatus
                                    + ") success.");
                    recordOnAppUpdateInfo(info);
                    pushStatus();
                })
                .addOnFailureListener(exception -> {
                    mAppUpdateInfo = null;
                    mUpdateAvailability = UpdateAvailability.UNKNOWN;
                    mInstallStatus = InstallStatus.UNKNOWN;
                    Log.i(TAG, "pullCurrentState() failed.");
                    recordCallFailure(CallFailure.QUERY_FAILED);
                    pushStatus();
                });
    }

    private void pushStatus() {
        if (!mEnabled || mUpdateAvailability == null || mInstallStatus == null) return;

        @UpdateState
        int newState = toUpdateState(mUpdateAvailability, mInstallStatus);
        if (mUpdateState != null && mUpdateState == newState) return;

        Log.i(TAG, "Pushing inline update state to " + newState);
        mUpdateState = newState;
        mCallback.run();
    }

    private static @UpdateState int toUpdateState(
            @UpdateAvailability int updateAvailability, @InstallStatus int installStatus) {
        @UpdateState
        int newStatus = UpdateState.NONE;

        // Note, use InstallStatus first then UpdateAvailability if InstallStatus doesn't indicate
        // a currently active install.
        switch (installStatus) {
            case InstallStatus.PENDING:
                // Intentional fall through.
            case InstallStatus.DOWNLOADING:
                newStatus = UpdateState.INLINE_UPDATE_DOWNLOADING;
                break;
            case InstallStatus.DOWNLOADED:
                newStatus = UpdateState.INLINE_UPDATE_READY;
                break;
            case InstallStatus.FAILED:
                newStatus = UpdateState.INLINE_UPDATE_FAILED;
                break;
        }

        if (newStatus == UpdateState.NONE) {
            switch (updateAvailability) {
                case UpdateAvailability.UPDATE_AVAILABLE:
                    newStatus = UpdateState.INLINE_UPDATE_AVAILABLE;
                    break;
            }
        }

        return newStatus;
    }

    private static String installStatusToEnumSuffix(@InstallStatus int status) {
        switch (status) {
            case InstallStatus.UNKNOWN:
                return "Unknown";
            case InstallStatus.REQUIRES_UI_INTENT:
                return "RequiresUiIntent";
            case InstallStatus.PENDING:
                return "Pending";
            case InstallStatus.DOWNLOADING:
                return "Downloading";
            case InstallStatus.DOWNLOADED:
                return "Downloaded";
            case InstallStatus.INSTALLING:
                return "Installing";
            case InstallStatus.INSTALLED:
                return "Installed";
            case InstallStatus.FAILED:
                return "Failed";
            case InstallStatus.CANCELED:
                return "Canceled";
            default:
                return "Untracked";
        }
    }

    private static @InstallErrorCodeMetrics int installErrorCodeToMetrics(
            @InstallErrorCode int error) {
        switch (error) {
            case InstallErrorCode.NO_ERROR:
                return InstallErrorCodeMetrics.NO_ERROR;
            case InstallErrorCode.NO_ERROR_PARTIALLY_ALLOWED:
                return InstallErrorCodeMetrics.NO_ERROR_PARTIALLY_ALLOWED;
            case InstallErrorCode.ERROR_UNKNOWN:
                return InstallErrorCodeMetrics.ERROR_UNKNOWN;
            case InstallErrorCode.ERROR_API_NOT_AVAILABLE:
                return InstallErrorCodeMetrics.ERROR_API_NOT_AVAILABLE;
            case InstallErrorCode.ERROR_INVALID_REQUEST:
                return InstallErrorCodeMetrics.ERROR_INVALID_REQUEST;
            case InstallErrorCode.ERROR_INSTALL_UNAVAILABLE:
                return InstallErrorCodeMetrics.ERROR_INSTALL_UNAVAILABLE;
            case InstallErrorCode.ERROR_INSTALL_NOT_ALLOWED:
                return InstallErrorCodeMetrics.ERROR_INSTALL_NOT_ALLOWED;
            case InstallErrorCode.ERROR_DOWNLOAD_NOT_PRESENT:
                return InstallErrorCodeMetrics.ERROR_DOWNLOAD_NOT_PRESENT;
            case InstallErrorCode.ERROR_INTERNAL_ERROR:
                return InstallErrorCodeMetrics.ERROR_INTERNAL_ERROR;
            default:
                return InstallErrorCodeMetrics.ERROR_UNTRACKED;
        }
    }

    private static @UpdateAvailabilityMetric int updateAvailabilityToMetrics(
            @UpdateAvailability int updateAvailability) {
        switch (updateAvailability) {
            case UpdateAvailability.UNKNOWN:
                return UpdateAvailabilityMetric.UNKNOWN;
            case UpdateAvailability.UPDATE_NOT_AVAILABLE:
                return UpdateAvailabilityMetric.UPDATE_NOT_AVAILABLE;
            case UpdateAvailability.UPDATE_AVAILABLE:
                return UpdateAvailabilityMetric.UPDATE_AVAILABLE;
            case UpdateAvailability.DEVELOPER_TRIGGERED_UPDATE_IN_PROGRESS:
                return UpdateAvailabilityMetric.DEVELOPER_TRIGGERED_UPDATE_IN_PROGRESS;
            default:
                return UpdateAvailabilityMetric.UNTRACKED;
        }
    }

    private static @InstallStatusMetric int installStatusToMetrics(
            @InstallStatus int installStatus) {
        switch (installStatus) {
            case InstallStatus.UNKNOWN:
                return InstallStatusMetric.UNKNOWN;
            case InstallStatus.REQUIRES_UI_INTENT:
                return InstallStatusMetric.REQUIRES_UI_INTENT;
            case InstallStatus.PENDING:
                return InstallStatusMetric.PENDING;
            case InstallStatus.DOWNLOADING:
                return InstallStatusMetric.DOWNLOADING;
            case InstallStatus.DOWNLOADED:
                return InstallStatusMetric.DOWNLOADED;
            case InstallStatus.INSTALLING:
                return InstallStatusMetric.INSTALLING;
            case InstallStatus.INSTALLED:
                return InstallStatusMetric.INSTALLED;
            case InstallStatus.FAILED:
                return InstallStatusMetric.FAILED;
            case InstallStatus.CANCELED:
                return InstallStatusMetric.CANCELED;
            default:
                return InstallStatusMetric.UNTRACKED;
        }
    }

    private static void recordOnAppUpdateInfo(AppUpdateInfo info) {
        RecordHistogram.recordEnumeratedHistogram(
                "GoogleUpdate.Inline.AppUpdateInfo.UpdateAvailability",
                updateAvailabilityToMetrics(info.updateAvailability()),
                UpdateAvailabilityMetric.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram("GoogleUpdate.Inline.AppUpdateInfo.InstallStatus",
                installStatusToMetrics(info.installStatus()), InstallStatusMetric.NUM_ENTRIES);
    }

    private static void recordCallFailure(@CallFailure int failure) {
        RecordHistogram.recordEnumeratedHistogram(
                "GoogleUpdate.Inline.CallFailure", failure, CallFailure.NUM_ENTRIES);
    }
}