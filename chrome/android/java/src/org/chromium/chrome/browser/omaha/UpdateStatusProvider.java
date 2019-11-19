// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.StatFs;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.common.GooglePlayServicesUtil;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.AsyncTask.Status;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.omaha.inline.InlineUpdateController;
import org.chromium.chrome.browser.omaha.inline.InlineUpdateControllerFactory;
import org.chromium.chrome.browser.omaha.metrics.UpdateSuccessMetrics;
import org.chromium.chrome.browser.omaha.metrics.UpdateSuccessMetrics.UpdateType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides the current update state for Chrome.  This update state is asynchronously determined and
 * can change as Chrome runs.
 *
 * For manually testing this functionality, see {@link UpdateConfigs}.
 */
public class UpdateStatusProvider implements ActivityStateListener {
    /**
     * Possible sources of user interaction regarding updates.
     * Treat this as append only as it is used by UMA.
     */
    @IntDef({UpdateInteractionSource.FROM_MENU, UpdateInteractionSource.FROM_INFOBAR,
            UpdateInteractionSource.FROM_NOTIFICATION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UpdateInteractionSource {
        int FROM_MENU = 0;
        int FROM_INFOBAR = 1;
        int FROM_NOTIFICATION = 2;

        int NUM_ENTRIES = 3;
    }

    /**
     * Possible update states.
     * Treat this as append only as it is used by UMA.
     */
    @IntDef({UpdateState.NONE, UpdateState.UPDATE_AVAILABLE, UpdateState.UNSUPPORTED_OS_VERSION,
            UpdateState.INLINE_UPDATE_AVAILABLE, UpdateState.INLINE_UPDATE_DOWNLOADING,
            UpdateState.INLINE_UPDATE_READY, UpdateState.INLINE_UPDATE_FAILED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UpdateState {
        int NONE = 0;
        int UPDATE_AVAILABLE = 1;
        int UNSUPPORTED_OS_VERSION = 2;
        int INLINE_UPDATE_AVAILABLE = 3;
        int INLINE_UPDATE_DOWNLOADING = 4;
        int INLINE_UPDATE_READY = 5;
        int INLINE_UPDATE_FAILED = 6;

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

        /**
         * Whether or not we are currently trying to simulate an inline flow.  Used to allow
         * overriding Omaha update state, which usually supersedes inline update states.
         */
        private boolean mIsInlineSimulated;

        public UpdateStatus() {}

        UpdateStatus(UpdateStatus other) {
            updateState = other.updateState;
            updateUrl = other.updateUrl;
            latestVersion = other.latestVersion;
            latestUnsupportedVersion = other.latestUnsupportedVersion;
            mIsSimulated = other.mIsSimulated;
            mIsInlineSimulated = other.mIsInlineSimulated;
        }
    }

    private final ObserverList<Callback<UpdateStatus>> mObservers = new ObserverList<>();

    private final InlineUpdateController mInlineController;
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
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> observer.onResult(mStatus));
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

        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.LATEST_UNSUPPORTED_VERSION, currentlyUsedVersion);
        mStatus.latestUnsupportedVersion = currentlyUsedVersion;
        pingObservers();
    }

    /**
     * Starts the inline update process, if possible.
     * @source         The source of the action (the UI that caused it).
     * @param activity An {@link Activity} that will be used to interact with Play.
     */
    public void startInlineUpdate(@UpdateInteractionSource int source, Activity activity) {
        if (mStatus == null || mStatus.updateState != UpdateState.INLINE_UPDATE_AVAILABLE) return;
        RecordHistogram.recordEnumeratedHistogram(
                "GoogleUpdate.Inline.UI.Start.Source", source, UpdateInteractionSource.NUM_ENTRIES);
        mMetrics.startUpdate(UpdateType.INLINE, source);
        mInlineController.startUpdate(activity);
    }

    /**
     * Retries the inline update process, if possible.
     * @param activity An {@link Activity} that will be used to interact with Play.
     */
    public void retryInlineUpdate(@UpdateInteractionSource int source, Activity activity) {
        if (mStatus == null || mStatus.updateState != UpdateState.INLINE_UPDATE_AVAILABLE) return;
        RecordHistogram.recordEnumeratedHistogram(
                "GoogleUpdate.Inline.UI.Retry.Source", source, UpdateInteractionSource.NUM_ENTRIES);
        mMetrics.startUpdate(UpdateType.INLINE, source);
        mInlineController.startUpdate(activity);
    }

    /** Finishes the inline update process, which may involve restarting the app. */
    public void finishInlineUpdate(@UpdateInteractionSource int source) {
        if (mStatus == null || mStatus.updateState != UpdateState.INLINE_UPDATE_READY) return;
        RecordHistogram.recordEnumeratedHistogram("GoogleUpdate.Inline.UI.Install.Source", source,
                UpdateInteractionSource.NUM_ENTRIES);
        mInlineController.completeUpdate();
    }

    /**
     * Starts the intent update process, if possible
     * @param context An {@link Context} that will be used to fire off the update intent.
     * @param source  The source of the action (the UI that caused it).
     * @param newTask Whether or not to make the intent a new task.
     * @return        Whether or not the update intent was sent and had a valid handler.
     */
    public boolean startIntentUpdate(
            Context context, @UpdateInteractionSource int source, boolean newTask) {
        if (mStatus == null || mStatus.updateState != UpdateState.UPDATE_AVAILABLE) return false;
        if (TextUtils.isEmpty(mStatus.updateUrl)) return false;

        try {
            mMetrics.startUpdate(UpdateType.INTENT, source);

            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(mStatus.updateUrl));
            if (newTask) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            return false;
        }

        return true;
    }

    // ApplicationStateListener implementation.
    @Override
    public void onActivityStateChange(Activity changedActivity, @ActivityState int newState) {
        boolean hasActiveActivity = false;

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity == null || !(activity instanceof ChromeActivity)) continue;

            hasActiveActivity |=
                    ApplicationStatus.getStateForActivity(activity) == ActivityState.RESUMED;
            if (hasActiveActivity) break;
        }

        mInlineController.setEnabled(hasActiveActivity);
    }

    private UpdateStatusProvider() {
        mInlineController = InlineUpdateControllerFactory.create(this::resolveStatus);
        mOmahaQuery = new UpdateQuery(this::resolveStatus);
        mMetrics = new UpdateSuccessMetrics();

        // Note that as a singleton this class never unregisters.
        ApplicationStatus.registerStateListenerForAllActivities(this);
    }

    private void pingObservers() {
        for (Callback<UpdateStatus> observer : mObservers) observer.onResult(mStatus);
    }

    private void resolveStatus() {
        if (mOmahaQuery.getStatus() != Status.FINISHED || mInlineController.getStatus() == null) {
            return;
        }

        // We pull the Omaha result once as it will never change.
        if (mStatus == null) mStatus = new UpdateStatus(mOmahaQuery.getResult());

        if (mStatus.mIsSimulated) {
            if (mStatus.mIsInlineSimulated) {
                @UpdateState
                int inlineState = mInlineController.getStatus();

                if (inlineState == UpdateState.NONE) {
                    mStatus.updateState = mOmahaQuery.getResult().updateState;
                } else {
                    mStatus.updateState = inlineState;
                }
            }
        } else {
            @UpdateState
            int omahaState = mOmahaQuery.getResult().updateState;
            @UpdateState
            int inlineState = mInlineController.getStatus();
            mStatus.updateState = resolveOmahaAndInlineStatus(
                    UpdateConfigs.getConfiguration(), omahaState, inlineState);
        }

        if (!mRecordedInitialStatus) {
            RecordHistogram.recordEnumeratedHistogram(
                    "GoogleUpdate.StartUp.State", mStatus.updateState, UpdateState.NUM_ENTRIES);
            mMetrics.analyzeFirstStatus(mStatus);
            mRecordedInitialStatus = true;
        }

        pingObservers();
    }

    @VisibleForTesting
    static @UpdateState int resolveOmahaAndInlineStatus(
            @UpdateConfigs.UpdateFlowConfiguration int configuration, @UpdateState int omahaState,
            @UpdateState int inlineState) {
        switch (configuration) {
            case UpdateConfigs.UpdateFlowConfiguration.NEVER_SHOW:
                return UpdateState.NONE;
            case UpdateConfigs.UpdateFlowConfiguration.INLINE_ONLY:
                if (omahaState != UpdateState.UPDATE_AVAILABLE) return omahaState;
                if (inlineState == UpdateState.NONE) return UpdateState.NONE;
                return inlineState;
            case UpdateConfigs.UpdateFlowConfiguration.BEST_EFFORT:
                if (omahaState != UpdateState.UPDATE_AVAILABLE) return omahaState;
                if (inlineState == UpdateState.NONE) return omahaState;
                return inlineState;
            case UpdateConfigs.UpdateFlowConfiguration.INTENT_ONLY: // Intentional fall through.
            default:
                // Fall back to use Omaha only and use the old flow.
                return omahaState;
        }
    }

    private static final class LazyHolder {
        private static final UpdateStatusProvider INSTANCE = new UpdateStatusProvider();
    }

    private static final class UpdateQuery extends AsyncTask<UpdateStatus> {
        private final Context mContext = ContextUtils.getApplicationContext();
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
            return getRealStatus(mContext);
        }

        @Override
        protected void onPostExecute(UpdateStatus result) {
            mStatus = result;
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, mCallback);
        }

        private UpdateStatus getTestStatus() {
            @UpdateState
            Integer forcedUpdateState = UpdateConfigs.getMockUpdateState();
            if (forcedUpdateState == null) return null;

            UpdateStatus status = new UpdateStatus();

            status.mIsSimulated = true;
            status.updateState = forcedUpdateState;

            status.mIsInlineSimulated = forcedUpdateState == UpdateState.INLINE_UPDATE_AVAILABLE;

            // Push custom configurations for certain update states.
            switch (forcedUpdateState) {
                case UpdateState.UPDATE_AVAILABLE:
                    String updateUrl = UpdateConfigs.getMockMarketUrl();
                    if (!TextUtils.isEmpty(updateUrl)) status.updateUrl = updateUrl;
                    break;
                case UpdateState.UNSUPPORTED_OS_VERSION:
                    status.latestUnsupportedVersion =
                            SharedPreferencesManager.getInstance().readString(
                                    ChromePreferenceKeys.LATEST_UNSUPPORTED_VERSION, null);
                    break;
            }

            return status;
        }

        private UpdateStatus getRealStatus(Context context) {
            UpdateStatus status = new UpdateStatus();

            if (VersionNumberGetter.isNewerVersionAvailable(context)) {
                status.updateUrl = MarketURLGetter.getMarketUrl();
                status.latestVersion =
                        VersionNumberGetter.getInstance().getLatestKnownVersion(context);

                boolean allowedToUpdate =
                        checkForSufficientStorage() && isGooglePlayStoreAvailable(context);
                status.updateState =
                        allowedToUpdate ? UpdateState.UPDATE_AVAILABLE : UpdateState.NONE;

                SharedPreferencesManager.getInstance().removeKey(
                        ChromePreferenceKeys.LATEST_UNSUPPORTED_VERSION);
            } else if (!VersionNumberGetter.isCurrentOsVersionSupported()) {
                status.updateState = UpdateState.UNSUPPORTED_OS_VERSION;
                status.latestUnsupportedVersion = SharedPreferencesManager.getInstance().readString(
                        ChromePreferenceKeys.LATEST_UNSUPPORTED_VERSION, null);
            } else {
                status.updateState = UpdateState.NONE;
            }

            return status;
        }

        private boolean checkForSufficientStorage() {
            assert !ThreadUtils.runningOnUiThread();

            File path = Environment.getDataDirectory();
            StatFs statFs = new StatFs(path.getAbsolutePath());
            long size;
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) {
                size = getSize(statFs);
            } else {
                size = getSizeUpdatedApi(statFs);
            }
            RecordHistogram.recordLinearCountHistogram(
                    "GoogleUpdate.InfoBar.InternalStorageSizeAvailable", (int) size, 1, 200, 100);
            RecordHistogram.recordLinearCountHistogram(
                    "GoogleUpdate.InfoBar.DeviceFreeSpace", (int) size, 1, 1000, 50);

            int minRequiredStorage = UpdateConfigs.getMinRequiredStorage();
            if (minRequiredStorage == -1) return true;

            return size >= minRequiredStorage;
        }

        private boolean isGooglePlayStoreAvailable(Context context) {
            try {
                context.getPackageManager().getPackageInfo(
                        GooglePlayServicesUtil.GOOGLE_PLAY_STORE_PACKAGE, 0);
            } catch (PackageManager.NameNotFoundException e) {
                return false;
            }
            return true;
        }

        @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
        private long getSizeUpdatedApi(StatFs statFs) {
            return ConversionUtils.bytesToMegabytes(statFs.getAvailableBytes());
        }

        @SuppressWarnings("deprecation")
        private long getSize(StatFs statFs) {
            int blockSize = statFs.getBlockSize();
            int availableBlocks = statFs.getAvailableBlocks();
            return ConversionUtils.bytesToMegabytes(blockSize * availableBlocks);
        }
    }
}
