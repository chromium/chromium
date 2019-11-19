// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.services.IVariationsSeedServer;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.CachedMetrics.CustomCountHistogramSample;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.base.metrics.CachedMetrics.TimesHistogramSample;
import org.chromium.components.variations.LoadSeedResult;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Date;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * VariationsSeedLoader asynchronously loads and updates the variations seed. VariationsSeedLoader
 * wraps a Runnable which wraps a FutureTask. The FutureTask loads the seed. The Runnable invokes
 * the FutureTask and then updates the seed if necessary, possibly requesting a new seed from
 * VariationsSeedServer. The reason for splitting the work this way is that WebView startup must
 * block on loading the seed (by using FutureTask.get()) but should not block on the other work done
 * by the Runnable.
 *
 * The Runnable and FutureTask together perform these steps:
 * 1. Pre-load the metrics client ID. This is needed to seed the EntropyProvider. If there is no
 *    client ID, variations can't be used on this run.
 * 2. Load the new seed file, if any.
 * 3. If no new seed file, load the old seed file, if any.
 * 4. Make the loaded seed available via get() (or null if there was no seed).
 * 5. If there was a new seed file, replace the old with the new (but only after making the loaded
 *    seed available, as the replace need not block startup).
 * 6. If there was no seed, or the loaded seed was expired, request a new seed (but don't request
 *    more often than MAX_REQUEST_PERIOD_MILLIS).
 *
 * VariationsSeedLoader should be used during WebView startup like so:
 * 1. Ensure ContextUtils.getApplicationContext(), AwBrowserProcess.getWebViewPackageName(), and
 *    PathUtils are ready to use.
 * 2. As early as possible, call startVariationsInit() to begin the task.
 * 3. Perform any WebView startup tasks which don't require variations to be initialized.
 * 4. Call finishVariationsInit() with the value returned from startVariationsInit(). This will
 *    block for up to SEED_LOAD_TIMEOUT_MILLIS if the task hasn't fininshed loading the seed. If the
 *    seed is loaded on time, variations will be initialized. finishVariationsInit() must be called
 *    before AwFeatureListCreator::SetUpFieldTrials() runs.
 */
public class VariationsSeedLoader {
    private static final String TAG = "VariationsSeedLoader";

    // The expiration time for an app's copy of the Finch seed, after which we'll still use it,
    // but we'll request a new one from VariationsSeedService.
    private static final long SEED_EXPIRATION_MILLIS = TimeUnit.HOURS.toMillis(6);

    // After requesting a new seed, wait at least this long before making a new request.
    private static final long MAX_REQUEST_PERIOD_MILLIS = TimeUnit.HOURS.toMillis(1);

    // Block in finishVariationsInit() for at most this value waiting for the seed. If the timeout
    // is exceeded, proceed with variations disabled, and record the event in the
    // Variations.SeedLoadResult histogram's "Seed Load Timed Out" bucket. See the discussion on
    // https://crbug.com/936172 about the trade-offs of increasing or decreasing this value.
    private static final long SEED_LOAD_TIMEOUT_MILLIS = 20;

    @VisibleForTesting
    public static final String APP_SEED_FRESHNESS_HISTOGRAM_NAME = "Variations.AppSeedFreshness";
    @VisibleForTesting
    public static final String APP_SEED_REQUEST_STATE_HISTOGRAM_NAME =
            "Variations.AppSeedRequestState";
    private static final String SEED_LOAD_BLOCKING_TIME_HISTOGRAM_NAME =
            "Variations.SeedLoadBlockingTime";
    // This metric is also written by VariationsSeedStore::LoadSeed and is used by other platforms.
    private static final String SEED_LOAD_RESULT_HISTOGRAM_NAME = "Variations.SeedLoadResult";

    private SeedLoadAndUpdateRunnable mRunnable;

    // UMA histogram values for the result of checking if the app needs a new variations seed.
    // Keep in sync with AppSeedRequestState enum in enums.xml.
    //
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({AppSeedRequestState.UNKNOWN, AppSeedRequestState.SEED_FRESH,
            AppSeedRequestState.SEED_REQUESTED, AppSeedRequestState.SEED_REQUEST_THROTTLED})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    public @interface AppSeedRequestState {
        int UNKNOWN = 0;
        int SEED_FRESH = 1;
        int SEED_REQUESTED = 2;
        int SEED_REQUEST_THROTTLED = 3;
        int NUM_ENTRIES = 4;
    }

    private static void recordLoadSeedResult(int result) {
        EnumeratedHistogramSample histogram = new EnumeratedHistogramSample(
                SEED_LOAD_RESULT_HISTOGRAM_NAME, LoadSeedResult.ENUM_SIZE);
        histogram.record(result);
    }

    private static void recordSeedLoadBlockingTime(long timeMs) {
        TimesHistogramSample histogram =
                new TimesHistogramSample(SEED_LOAD_BLOCKING_TIME_HISTOGRAM_NAME);
        histogram.record(timeMs);
    }

    private static void recordSeedRequestState(@AppSeedRequestState int state) {
        EnumeratedHistogramSample histogram = new EnumeratedHistogramSample(
                APP_SEED_REQUEST_STATE_HISTOGRAM_NAME, AppSeedRequestState.NUM_ENTRIES);
        histogram.record(state);
    }

    private static void recordAppSeedFreshness(long freshnessMinutes) {
        // Bucket parameters should match Variations.SeedFreshness.
        // See variations::RecordSeedFreshness.
        CustomCountHistogramSample histogram = new CustomCountHistogramSample(
                APP_SEED_FRESHNESS_HISTOGRAM_NAME, 1, (int) TimeUnit.DAYS.toMinutes(30), 50);
        histogram.record((int) freshnessMinutes);
    }

    private static boolean shouldThrottleRequests(long now) {
        long lastRequestTime = VariationsUtils.getStampTime();
        if (lastRequestTime == 0) {
            return false;
        }
        return now < lastRequestTime + MAX_REQUEST_PERIOD_MILLIS;
    }

    private boolean isSeedExpired(long seedFileTime) {
        long expirationTime = seedFileTime + SEED_EXPIRATION_MILLIS;
        return getCurrentTimeMillis() > expirationTime;
    }

    // Loads our local copy of the seed, if any, and then renames our local copy and/or requests a
    // new seed, if necessary.
    private class SeedLoadAndUpdateRunnable implements Runnable {
        // mLoadTask will set these to indicate what additional work to do after mLoadTask finishes:
        // - mFoundNewSeed: Is a "new" seed file present? (If so, it should be renamed to an "old"
        //   seed, replacing any existing "old" seed.)
        // - mNeedNewSeed: Should we request a new seed from the service?
        // - mCurrentSeedDate: The "date" field of our local seed, converted to milliseconds since
        //   epoch, or Long.MIN_VALUE if we have no seed. This value originates from the server.
        // - mSeedFileTime: The time, in milliseconds since the UNIX epoch, our local copy of the
        //   seed was last written to disk as measured by the device's clock.
        // - mSeedRequestState: The result of checking if a new seed is required.
        private boolean mFoundNewSeed;
        private boolean mNeedNewSeed;
        private long mCurrentSeedDate = Long.MIN_VALUE;
        private long mSeedFileTime;
        private int mSeedRequestState = AppSeedRequestState.UNKNOWN;

        private FutureTask<SeedInfo> mLoadTask = new FutureTask<>(() -> {
            File newSeedFile = VariationsUtils.getNewSeedFile();
            File oldSeedFile = VariationsUtils.getSeedFile();

            // First check for a new seed.
            SeedInfo seed = VariationsUtils.readSeedFile(newSeedFile);
            if (seed != null) {
                // If a valid new seed was found, make a note to replace the old seed with
                // the new seed. (Don't do it now, to avoid delaying FutureTask.get().)
                mFoundNewSeed = true;

                mSeedFileTime = newSeedFile.lastModified();
            } else {
                // If there is no new seed, check for an old seed.
                seed = VariationsUtils.readSeedFile(oldSeedFile);

                if (seed != null) {
                    mSeedFileTime = oldSeedFile.lastModified();
                }
            }

            // Make a note to request a new seed if necessary. (Don't request it now, to
            // avoid delaying FutureTask.get().)
            if (seed == null || isSeedExpired(mSeedFileTime)) {
                mNeedNewSeed = true;
                mSeedRequestState = AppSeedRequestState.SEED_REQUESTED;

                // Rate-limit the requests.
                if (shouldThrottleRequests(getCurrentTimeMillis())) {
                    mNeedNewSeed = false;
                    mSeedRequestState = AppSeedRequestState.SEED_REQUEST_THROTTLED;
                }
            } else {
                mSeedRequestState = AppSeedRequestState.SEED_FRESH;
            }

            // Note the date field of whatever seed was loaded, if any.
            if (seed != null) {
                mCurrentSeedDate = seed.date;
            }

            return seed;
        });

        @Override
        public void run() {
            mLoadTask.run();
            // The loaded seed is now available via get(). The following steps won't block startup.

            if (mFoundNewSeed) {
                // The move happens synchronously. It's not possible for the service to still be
                // writing to this file when we move it, because mFoundNewSeed means we already read
                // the seed and found it to be complete. Therefore the service must have already
                // finished writing.
                VariationsUtils.replaceOldWithNewSeed();
            }

            if (mNeedNewSeed) {
                // The new seed will arrive asynchronously; the new seed file is written by the
                // service, and may complete after this app process has died.
                requestSeedFromService(mCurrentSeedDate);
                VariationsUtils.updateStampTime();
            }

            onBackgroundWorkFinished();
        }

        public SeedInfo get(long timeout, TimeUnit unit)
                throws InterruptedException, ExecutionException, TimeoutException {
            SeedInfo info = mLoadTask.get(timeout, unit);
            recordSeedRequestState(mSeedRequestState);
            if (mSeedFileTime != 0) {
                long freshnessMinutes =
                        TimeUnit.MILLISECONDS.toMinutes(getCurrentTimeMillis() - mSeedFileTime);
                recordAppSeedFreshness(freshnessMinutes);
            }
            return info;
        }

        public boolean isLoadedSeedFresh() {
            return mSeedRequestState == AppSeedRequestState.SEED_FRESH;
        }
    }

    // Connects to VariationsSeedServer service. Sends a file descriptor for our local copy of the
    // seed to the service, to which the service will write a new seed.
    private class SeedServerConnection implements ServiceConnection {
        private ParcelFileDescriptor mNewSeedFd;
        private long mOldSeedDate;

        public SeedServerConnection(ParcelFileDescriptor newSeedFd, long oldSeedDate) {
            mNewSeedFd = newSeedFd;
            mOldSeedDate = oldSeedDate;
        }

        public void start() {
            try {
                if (!ContextUtils.getApplicationContext()
                        .bindService(getServerIntent(), this, Context.BIND_AUTO_CREATE)) {
                    Log.e(TAG, "Failed to bind to WebView service");
                }
            } catch (NameNotFoundException e) {
                Log.e(TAG, "WebView provider \"" + AwBrowserProcess.getWebViewPackageName() +
                        "\" not found!");
            }
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            try {
                IVariationsSeedServer.Stub.asInterface(service).getSeed(mNewSeedFd, mOldSeedDate);
            } catch (RemoteException e) {
                Log.e(TAG, "Faild requesting seed", e);
            } finally {
                ContextUtils.getApplicationContext().unbindService(this);
                VariationsUtils.closeSafely(mNewSeedFd);
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {}
    }

    private SeedInfo getSeedBlockingAndLog() {
        long start = SystemClock.elapsedRealtime();
        try {
            try {
                return mRunnable.get(getSeedLoadTimeoutMillis(), TimeUnit.MILLISECONDS);
            } finally {
                long end = SystemClock.elapsedRealtime();
                recordSeedLoadBlockingTime(end - start);
            }
        } catch (TimeoutException e) {
            recordLoadSeedResult(LoadSeedResult.LOAD_TIMED_OUT);
        } catch (InterruptedException e) {
            recordLoadSeedResult(LoadSeedResult.LOAD_INTERRUPTED);
        } catch (ExecutionException e) {
            recordLoadSeedResult(LoadSeedResult.LOAD_OTHER_FAILURE);
        }
        Log.e(TAG, "Failed loading variations seed. Variations disabled.");
        return null;
    }

    private boolean isLoadedSeedFresh() {
        return mRunnable.isLoadedSeedFresh();
    }

    @VisibleForTesting // Overridden by tests to wait until all work is done.
    protected void onBackgroundWorkFinished() {}

    @VisibleForTesting
    protected long getSeedLoadTimeoutMillis() {
        return SEED_LOAD_TIMEOUT_MILLIS;
    }

    @VisibleForTesting
    protected long getCurrentTimeMillis() {
        return new Date().getTime();
    }

    @VisibleForTesting // and non-static for overriding by tests
    protected Intent getServerIntent() throws NameNotFoundException {
        Intent intent = new Intent();
        intent.setClassName(
                AwBrowserProcess.getWebViewPackageName(), ServiceNames.VARIATIONS_SEED_SERVER);
        return intent;
    }

    @VisibleForTesting
    protected void requestSeedFromService(long oldSeedDate) {
        File newSeedFile = VariationsUtils.getNewSeedFile();
        try {
            newSeedFile.createNewFile(); // Silently returns false if already exists.
        } catch (IOException e) {
            Log.e(TAG, "Failed to create seed file " + newSeedFile);
            return;
        }
        ParcelFileDescriptor newSeedFd = null;
        try {
            newSeedFd = ParcelFileDescriptor.open(
                    newSeedFile, ParcelFileDescriptor.MODE_WRITE_ONLY);
        } catch (FileNotFoundException e) {
            Log.e(TAG, "Failed to open seed file " + newSeedFile);
            return;
        }

        SeedServerConnection connection = new SeedServerConnection(newSeedFd, oldSeedDate);
        connection.start();
    }

    // Begin asynchronously loading the variations seed. ContextUtils.getApplicationContext() and
    // AwBrowserProcess.getWebViewPackageName() must be ready to use before calling this.
    public void startVariationsInit() {
        mRunnable = new SeedLoadAndUpdateRunnable();
        (new Thread(mRunnable)).start();
    }

    // Block on loading the seed with a timeout. Then if a seed was successfully loaded, initialize
    // variations.
    public void finishVariationsInit() {
        SeedInfo seed = getSeedBlockingAndLog();
        if (seed != null) {
            AwVariationsSeedBridge.setSeed(seed);
            AwVariationsSeedBridge.setLoadedSeedFresh(isLoadedSeedFresh());
        }
    }
}
