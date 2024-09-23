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
class BrandingChecker extends AsyncTask<Integer> {
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
     * Interface BrandingChecked used to fetch branding information.
     * If the storage involves any worker thread operation (e.g. Disk I/O), the storage impl has
     * the responsibility to manage switching calls to the right thread.
     */
    public interface BrandingLaunchTimeStorage {
        /**
         * Return the last time branding was shown for given embedded app. If not found, return
         * {@link BrandingChecker#BRANDING_TIME_NOT_FOUND}.
         *
         * @param appId ID of CCT embedded app.
         * @return Timestamp when CCT branding was last shown.
         * */
        @WorkerThread
        long get(String appId);

        /**
         * Record the timestamp when CCT branding was last shown.
         *
         * @param appId ID of CCT embedded app.
         * @param brandingLaunchTime Timestamp when CCT branding was last shown.
         * */
        @MainThread
        void put(String appId, long brandingLaunchTime);
    }

    private final String mAppId;
    private final long mBrandingCadence;
    @BrandingDecision private final Callback<Integer> mBrandingCheckCallback;
    @BrandingDecision private final int mDefaultBrandingDecision;

    private BrandingLaunchTimeStorage mStorage;

    /**
     * Create a BrandingChecker used to fetch BrandingDecision.
     * @param appId ID of Embedded app.
     * @param storage Storage option that used to retrieve branding information.
     * @param brandingCheckCallback Callback that will executed when branding check is complete.
     * @param brandingCadence The minimum time required to show another branding, to avoid overflow
     *                        clients with branding info.
     * @param defaultBrandingDecision Default branding decision when task is canceled.
     */
    BrandingChecker(
            String appId,
            BrandingLaunchTimeStorage storage,
            @NonNull @BrandingDecision Callback<Integer> brandingCheckCallback,
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
    protected @Nullable @BrandingDecision Integer doInBackground() {
        @BrandingDecision Integer brandingDecision = null;
        long startTime = SystemClock.elapsedRealtime();
        if (!TextUtils.isEmpty(mAppId)) {
            long timeLastBranding = mStorage.get(mAppId);
            brandingDecision = makeBrandingDecisionFromLaunchTime(startTime, timeLastBranding);
        }
        @BrandingAppIdType int appIdType = getAppIdType(mAppId);
        RecordHistogram.recordTimesHistogram(
                "CustomTabs.Branding.BrandingCheckDuration",
                SystemClock.elapsedRealtime() - startTime);
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.Branding.AppIdType", appIdType, BrandingAppIdType.NUM_ENTRIES);

        return brandingDecision;
    }

    @MainThread
    @Override
    protected void onPostExecute(@Nullable @BrandingDecision Integer brandingDecision) {
        onTaskFinished(brandingDecision);
    }

    @MainThread
    @Override
    protected void onCancelled() {
        onTaskFinished(null);
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

    private void onTaskFinished(@BrandingDecision Integer brandingDecision) {
        long taskFinishedTime = SystemClock.elapsedRealtime();
        if (brandingDecision == null) {
            brandingDecision = mDefaultBrandingDecision;
        }
        mBrandingCheckCallback.onResult(brandingDecision);

        // Do not record branding time for invalid app id, or branding is not shown.
        if (brandingDecision != BrandingDecision.NONE && !TextUtils.isEmpty(mAppId)) {
            mStorage.put(mAppId, taskFinishedTime);
        }

        // Remove the storage from reference.
        mStorage = null;
    }
}
