// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import android.content.Context;
import android.os.SystemClock;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;

import org.chromium.base.Callback;
import org.chromium.base.PackageUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;

/**
 * Class that maintain the data for the client app package name -> last time branding is shown.
 */
class BrandingChecker extends AsyncTask<Integer> {
    public static final int BRANDING_TIME_NOT_FOUND = -1;
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
         * @param packageName Package name of CCT embedded app.
         * @return Timestamp when CCT branding was last shown.
         * */
        @WorkerThread
        long get(String packageName);

        /**
         * Record the timestamp when CCT branding was last shown.
         *
         * @param packageName Package name of CCT embedded app.
         * @param brandingLaunchTime Timestamp when CCT branding was last shown.
         * */
        @MainThread
        void put(String packageName, long brandingLaunchTime);
    }

    private final Context mContext;
    private final String mPackageName;
    private final long mBrandingCadence;
    private final BrandingLaunchTimeStorage mStorage;
    @BrandingDecision
    private final Callback<Integer> mBrandingCheckCallback;
    @BrandingDecision
    private final int mDefaultBrandingDecision;

    private @Nullable Boolean mIsPackageValid;

    /**
     * Create a BrandingChecker used to fetch BrandingDecision.
     * @param context Application Context used to get package information.
     * @param packageName Package name of Embedded app.
     * @param storage Storage option that used to retrieve branding information.
     * @param brandingCheckCallback Callback that will executed when branding check is complete.
     * @param brandingCadence The minimum time required to show another branding, to avoid overflow
     *                        clients with branding info.
     * @param defaultBrandingDecision Default branding decision when task is canceled.
     */
    BrandingChecker(Context context, String packageName, BrandingLaunchTimeStorage storage,
            @NonNull @BrandingDecision Callback<Integer> brandingCheckCallback,
            long brandingCadence, @BrandingDecision int defaultBrandingDecision) {
        mContext = context;
        mPackageName = packageName;
        mStorage = storage;
        mBrandingCheckCallback = brandingCheckCallback;
        mBrandingCadence = brandingCadence;
        mDefaultBrandingDecision = defaultBrandingDecision;
    }

    @WorkerThread
    @Override
    protected @Nullable @BrandingDecision Integer doInBackground() {
        @BrandingDecision
        Integer brandingDecision = null;
        long startTime = SystemClock.elapsedRealtime();
        mIsPackageValid = PackageUtils.isPackageInstalled(mPackageName);
        if (mIsPackageValid) {
            long timeLastBranding = mStorage.get(mPackageName);
            brandingDecision = makeBrandingDecisionFromLaunchTime(startTime, timeLastBranding);
        }

        RecordHistogram.recordTimesHistogram("CustomTabs.Branding.BrandingCheckDuration",
                SystemClock.elapsedRealtime() - startTime);
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.Branding.IsPackageNameValid", mIsPackageValid);

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

        // Do not record branding time for invalid package name, or branding is not shown.
        // TODO(https://crbug.com/1350658): Add short term storage option for invalid packages.
        if (brandingDecision != BrandingDecision.NONE && mIsPackageValid != null
                && mIsPackageValid) {
            mStorage.put(mPackageName, taskFinishedTime);
        }

        RecordHistogram.recordEnumeratedHistogram("CustomTabs.Branding.BrandingDecision",
                brandingDecision, BrandingDecision.NUM_ENTRIES);
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.Branding.BrandingCheckCanceled", isCancelled());
    }
}
