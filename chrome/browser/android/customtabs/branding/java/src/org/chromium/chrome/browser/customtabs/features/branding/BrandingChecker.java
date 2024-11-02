// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import org.chromium.base.Callback;
import org.chromium.base.PackageUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;

/** Class that maintain the data for the client app id -> last time branding is shown. */
class BrandingChecker extends AsyncTask<BrandingInfo> {
    public static final int BRANDING_TIME_NOT_FOUND = -1;

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({
        BrandingAppIdType.INVALID,
        BrandingAppIdType.PACKAGE_NAME,
        BrandingAppIdType.REFERRER,
        BrandingAppIdType.NUM_ENTRIES
    })
    @interface BrandingAppIdType {
        int INVALID = 0;
        int PACKAGE_NAME = 1;
        int REFERRER = 2;

        // Must be the last one.
        int NUM_ENTRIES = 3;
    }

    /**
     * Interface BrandingChecked used to fetch branding information. If the storage involves any
     * worker thread operation (e.g. Disk I/O), the storage impl has the responsibility to manage
     * switching calls to the right thread.
     */
    public interface BrandingLaunchTimeStorage {
        /**
         * Return the last time branding was shown for given embedded app. If not found, return
         * {@link BrandingChecker#BRANDING_TIME_NOT_FOUND}.
         *
         * @param appId ID of CCT embedded app.
         * @return Timestamp when CCT branding was last shown.
         */
        @WorkerThread
        long get(String appId);

        /**
         * Record the timestamp when CCT branding for an app was last shown.
         *
         * @param appId ID of CCT embedded app.
         * @param brandingLaunchTime Timestamp when CCT branding was last shown.
         */
        @MainThread
        void put(String appId, long brandingLaunchTime);

        /** Return the last time branding was shown, with all apps/other branding considered. */
        @WorkerThread
        long getLastShowTimeGlobal();

        /**
         * Record the global timestamp when CCT branding/mismatch notification was last shown.
         *
         * @param launchTime The timestamp
         */
        @MainThread
        void putLastShowTimeGlobal(long launchTime);

        /**
         * Returns all the account mismatch notification data. If not found, return {@code null}.
         *
         * @return {@link MismatchNotificationData} object.
         */
        @WorkerThread
        @Nullable
        MismatchNotificationData getMimData();

        /**
         * Record the account mismatch notification data. Putting empty or null data is no-op since
         * no entry is removed.
         *
         * @param {@link MismatchNotificationData} object.
         */
        @MainThread
        void putMimData(MismatchNotificationData data);
    }

    private final String mAppId;
    private final long mBrandingCadence;
    private final Callback<BrandingInfo> mBrandingCheckCallback;
    @BrandingDecision private final int mDefaultBrandingDecision;

    private BrandingLaunchTimeStorage mStorage;

    /**
     * Create a BrandingChecker used to fetch BrandingDecision.
     *
     * @param appId ID of Embedded app.
     * @param storage Storage option that used to retrieve branding information.
     * @param brandingCheckCallback Callback that will executed when branding check is complete.
     * @param brandingCadence The minimum time required to show another branding, to avoid overflow
     *     clients with branding info.
     * @param defaultBrandingDecision Default branding decision when task is canceled.
     */
    BrandingChecker(
            String appId,
            BrandingLaunchTimeStorage storage,
            @NonNull Callback<BrandingInfo> brandingCheckCallback,
            long brandingCadence,
            @BrandingDecision int defaultBrandingDecision) {
        mAppId = appId;
        mStorage = storage;
        mBrandingCheckCallback = brandingCheckCallback;
        mBrandingCadence = brandingCadence;
        mDefaultBrandingDecision = defaultBrandingDecision;
    }

    @WorkerThread
    @Override
    protected BrandingInfo doInBackground() {
        @BrandingDecision Integer brandingDecision = null;
        long startTime = SystemClock.elapsedRealtime();
        long lastShowTime = BRANDING_TIME_NOT_FOUND;
        long lastShowTimeGlobal = BRANDING_TIME_NOT_FOUND; // Last show time for all apps combined.
        MismatchNotificationData mimData = null;
        if (!TextUtils.isEmpty(mAppId)) {
            lastShowTime = mStorage.get(mAppId);
            lastShowTimeGlobal = mStorage.getLastShowTimeGlobal();
            mimData = mStorage.getMimData();
            brandingDecision = makeBrandingDecisionFromLaunchTime(startTime, lastShowTime);
        }
        BrandingInfo info = new BrandingInfo(brandingDecision, lastShowTimeGlobal, mimData);
        @BrandingAppIdType int appIdType = getAppIdType(mAppId);
        RecordHistogram.recordTimesHistogram(
                "CustomTabs.Branding.BrandingCheckDuration",
                SystemClock.elapsedRealtime() - startTime);
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.Branding.AppIdType", appIdType, BrandingAppIdType.NUM_ENTRIES);

        return info;
    }

    @MainThread
    @Override
    protected void onPostExecute(BrandingInfo info) {
        onTaskFinished(info);
    }

    @MainThread
    @Override
    protected void onCancelled() {
        onTaskFinished(BrandingInfo.EMPTY);
    }

    @VisibleForTesting
    static @BrandingAppIdType int getAppIdType(String appId) {
        if (TextUtils.isEmpty(appId)) return BrandingAppIdType.INVALID;
        if (PackageUtils.isPackageInstalled(appId)) return BrandingAppIdType.PACKAGE_NAME;
        return BrandingAppIdType.REFERRER;
    }

    private @BrandingDecision int makeBrandingDecisionFromLaunchTime(
            long startTime, long lastBrandingShowTime) {
        if (lastBrandingShowTime == BRANDING_TIME_NOT_FOUND) {
            return BrandingDecision.TOAST;
        } else if (startTime - lastBrandingShowTime >= mBrandingCadence) {
            return BrandingDecision.TOOLBAR;
        } else {
            return BrandingDecision.NONE;
        }
    }

    private void onTaskFinished(BrandingInfo info) {
        long taskFinishedTime = SystemClock.elapsedRealtime();
        if (info.getDecision() == null) info.setDecision(mDefaultBrandingDecision);
        mBrandingCheckCallback.onResult(info);

        // Note: Branding decision can be altered to MIM when mismatch notification UI overrides it
        // later, but this still counts as 'shown' to the respect global rate-limiting policy.
        if (info.getDecision() != BrandingDecision.NONE && !TextUtils.isEmpty(mAppId)) {
            mStorage.put(mAppId, taskFinishedTime);
        }

        // Remove the storage from reference.
        mStorage = null;
    }
}
