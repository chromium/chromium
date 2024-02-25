// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Environment;
import android.os.StatFs;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.android.gms.common.GooglePlayServicesUtil;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.AsyncTask.Status;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.omaha.metrics.UpdateSuccessMetrics;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.util.ConversionUtils;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides the current update state for Chrome.  This update state is asynchronously determined and
 * can change as Chrome runs.
 *
 * For manually testing this functionality, see {@link UpdateConfigs}.
 */
public class UpdateStatusProvider {
    /**
     * Possible update states.
     * Treat this as append only as it is used by UMA.
     */
    @IntDef({UpdateState.NONE, UpdateState.UPDATE_AVAILABLE, UpdateState.UNSUPPORTED_OS_VERSION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UpdateState {
        int NONE = 0;
        int UPDATE_AVAILABLE = 1;
        int UNSUPPORTED_OS_VERSION = 2;
        // Inline updates are deprecated.
        // int INLINE_UPDATE_AVAILABLE = 3;
        // int INLINE_UPDATE_DOWNLOADING = 4;
        // int INLINE_UPDATE_READY = 5;
        // int INLINE_UPDATE_FAILED = 6;

        int NUM_ENTRIES = 7;
    }

    /** A set of properties that represent the current update state for Chrome. */
    public static final class UpdateStatus {
        /**
         * The current state of whether an update is available or whether it ever will be
         * (unsupported OS).
         */
        public @UpdateState int updateState;

        /** URL to direct the user to when Omaha detects a newer version available. */
        public String updateUrl;

        /**
         * The latest Chrome version available if OmahaClient.isNewerVersionAvailable() returns
         * true.
         */
        public String latestVersion;

        /**
         * If the current OS version is unsupported, and we show the menu badge, and then the user
         * clicks the badge and sees the unsupported message, we store the current version to a
         * preference and cache it here. This preference is read on startup to ensure we only show
         * the unsupported message once per version.
         */
        public String latestUnsupportedVersion;

        /**
         * Whether or not we are currently trying to simulate the update.  Used to ignore other
         * update signals.
         */
        private boolean mIsSimulated;

        public UpdateStatus() {}

        UpdateStatus(UpdateStatus other) {
            updateState = other.updateState;
            updateUrl = other.updateUrl;
            latestVersion = other.latestVersion;
            latestUnsupportedVersion = other.latestUnsupportedVersion;
            mIsSimulated = other.mIsSimulated;
        }
    }

    private final ObserverList<Callback<UpdateStatus>> mObservers = new ObserverList<>();

    private final UpdateQuery mOmahaQuery;
    private final UpdateSuccessMetrics mMetrics;
    private @Nullable UpdateStatus mStatus;

    /** Whether or not we've recorded the initial update status yet. */
    private boolean mRecordedInitialStatus;

    /** @return Returns a singleton of {@link UpdateStatusProvider}. */
    public static UpdateStatusProvider getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * Adds {@code observer} to notify about update state changes.  It is safe to call this multiple
     * times with the same {@code observer}.  This method will always notify {@code observer} of the
     * current status.  If that status has not been calculated yet this method call will trigger the
     * async work to calculate it.
     * @param observer The observer to notify about update state changes.
     * @return {@code true} if {@code observer} is newly registered.  {@code false} if it was
     *         already registered.
     */
    public boolean addObserver(Callback<UpdateStatus> observer) {
        if (mObservers.hasObserver(observer)) return false;
        mObservers.addObserver(observer);

        if (mStatus != null) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, observer.bind(mStatus));
        } else {
            if (mOmahaQuery.getStatus() == Status.PENDING) {
                mOmahaQuery.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            }
        }

        return true;
    }

    /**
     * No longer notifies {@code observer} about update state changes.  It is safe to call this
     * multiple times with the same {@code observer}.
     * @param observer To no longer notify about update state changes.
     */
    public void removeObserver(Callback<UpdateStatus> observer) {
        if (!mObservers.hasObserver(observer)) return;
        mObservers.removeObserver(observer);
    }

    /**
     * Notes that the user is aware that this version of Chrome is no longer supported and
     * potentially updates the update state accordingly.
     */
    public void updateLatestUnsupportedVersion() {
        if (mStatus == null) return;

        // If we have already stored the current version to a preference, no need to store it again,
        // unless their Chrome version has changed.
        String currentlyUsedVersion = BuildInfo.getInstance().versionName;
        if (mStatus.latestUnsupportedVersion != null
                && mStatus.latestUnsupportedVersion.equals(currentlyUsedVersion)) {
            return;
        }

        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.LATEST_UNSUPPORTED_VERSION, currentlyUsedVersion);
        mStatus.latestUnsupportedVersion = currentlyUsedVersion;
        pingObservers();
    }

    /**
     * Starts the intent update process, if possible
     * @param context An {@link Context} that will be used to fire off the update intent.
     * @param newTask Whether or not to make the intent a new task.
     * @return        Whether or not the update intent was sent and had a valid handler.
     */
    public boolean startIntentUpdate(Context context, boolean newTask) {
        if (mStatus == null || mStatus.updateState != UpdateState.UPDATE_AVAILABLE) return false;
        if (TextUtils.isEmpty(mStatus.updateUrl)) return false;

        try {
            mMetrics.startUpdate();

            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(mStatus.updateUrl));
            if (newTask) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            return false;
        }

        return true;
    }

    private UpdateStatusProvider() {
        mOmahaQuery = new UpdateQuery(this::resolveStatus);
        mMetrics = new UpdateSuccessMetrics();
    }

    private void pingObservers() {
        for (Callback<UpdateStatus> observer : mObservers) observer.onResult(mStatus);
    }

    private void resolveStatus() {
        if (mOmahaQuery.getStatus() != Status.FINISHED) {
            return;
        }

        // We pull the Omaha result once as it will never change.
        if (mStatus == null) mStatus = new UpdateStatus(mOmahaQuery.getResult());

        if (!mStatus.mIsSimulated) {
            mStatus.updateState = mOmahaQuery.getResult().updateState;
        }

        if (!mRecordedInitialStatus) {
            mMetrics.analyzeFirstStatus();
            mRecordedInitialStatus = true;
        }

        pingObservers();
    }

    private static final class LazyHolder {
        private static final UpdateStatusProvider INSTANCE = new UpdateStatusProvider();
    }

    private static final class UpdateQuery extends AsyncTask<UpdateStatus> {
        private final Runnable mCallback;

        private @Nullable UpdateStatus mStatus;

        public UpdateQuery(@NonNull Runnable resultReceiver) {
            mCallback = resultReceiver;
        }

        public UpdateStatus getResult() {
            return mStatus;
        }

        @Override
        protected UpdateStatus doInBackground() {
            UpdateStatus testStatus = getTestStatus();
            if (testStatus != null) return testStatus;
            return getRealStatus();
        }

        @Override
        protected void onPostExecute(UpdateStatus result) {
            mStatus = result;
            PostTask.postTask(TaskTraits.UI_DEFAULT, mCallback);
        }

        private UpdateStatus getTestStatus() {
            @UpdateState Integer forcedUpdateState = UpdateConfigs.getMockUpdateState();
            if (forcedUpdateState == null) return null;

            UpdateStatus status = new UpdateStatus();

            status.mIsSimulated = true;
            status.updateState = forcedUpdateState;

            // Push custom configurations for certain update states.
            switch (forcedUpdateState) {
                case UpdateState.UPDATE_AVAILABLE:
                    String updateUrl = UpdateConfigs.getMockMarketUrl();
                    if (!TextUtils.isEmpty(updateUrl)) status.updateUrl = updateUrl;
                    break;
                case UpdateState.UNSUPPORTED_OS_VERSION:
                    status.latestUnsupportedVersion =
                            ChromeSharedPreferences.getInstance()
                                    .readString(
                                            ChromePreferenceKeys.LATEST_UNSUPPORTED_VERSION, null);
                    break;
            }

            return status;
        }

        private UpdateStatus getRealStatus() {
            UpdateStatus status = new UpdateStatus();

            if (VersionNumberGetter.isNewerVersionAvailable()) {
                status.updateUrl = MarketURLGetter.getMarketUrl();
                status.latestVersion = VersionNumberGetter.getInstance().getLatestKnownVersion();

                boolean allowedToUpdate =
                        checkForSufficientStorage()
                                // Disable the version update check for automotive. See b/297925838.
                                && !BuildInfo.getInstance().isAutomotive
                                && PackageUtils.isPackageInstalled(
                                        GooglePlayServicesUtil.GOOGLE_PLAY_STORE_PACKAGE);
                status.updateState =
                        allowedToUpdate ? UpdateState.UPDATE_AVAILABLE : UpdateState.NONE;

                ChromeSharedPreferences.getInstance()
                        .removeKey(ChromePreferenceKeys.LATEST_UNSUPPORTED_VERSION);
            } else if (!VersionNumberGetter.isCurrentOsVersionSupported()) {
                status.updateState = UpdateState.UNSUPPORTED_OS_VERSION;
                status.latestUnsupportedVersion =
                        ChromeSharedPreferences.getInstance()
                                .readString(ChromePreferenceKeys.LATEST_UNSUPPORTED_VERSION, null);
            } else {
                status.updateState = UpdateState.NONE;
            }

            return status;
        }

        private boolean checkForSufficientStorage() {
            assert !ThreadUtils.runningOnUiThread();

            File path = Environment.getDataDirectory();
            StatFs statFs = new StatFs(path.getAbsolutePath());
            long size = getSize(statFs);

            int minRequiredStorage = UpdateConfigs.getMinRequiredStorage();
            if (minRequiredStorage == -1) return true;

            return size >= minRequiredStorage;
        }

        private long getSize(StatFs statFs) {
            return ConversionUtils.bytesToMegabytes(statFs.getAvailableBytes());
        }
    }
}
