// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.variations;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.services.IVariationsSeedServer;
import org.chromium.android_webview.common.services.IVariationsSeedServerCallback;
import org.chromium.android_webview.common.services.ServiceConnectionDelayRecorder;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.android_webview.common.variations.VariationsServiceMetricsHelper;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.variations.LoadSeedResult;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
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
@JNINamespace("android_webview")
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
    public static final String DOWNLOAD_JOB_INTERVAL_HISTOGRAM_NAME =
            "Variations.WebViewDownloadJobInterval";
    @VisibleForTesting
    public static final String DOWNLOAD_JOB_QUEUE_TIME_HISTOGRAM_NAME =
            "Variations.WebViewDownloadJobQueueTime";
    private static final String SEED_LOAD_BLOCKING_TIME_HISTOGRAM_NAME =
            "Variations.SeedLoadBlockingTime";
    // This metric is also written by VariationsSeedStore::LoadSeed and is used by other platforms.
    private static final String SEED_LOAD_RESULT_HISTOGRAM_NAME = "Variations.SeedLoadResult";

    private SeedLoadAndUpdateRunnable mRunnable;
    private SeedServerCallback mSeedServerCallback = new SeedServerCallback();

    private static void recordLoadSeedResult(@LoadSeedResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                SEED_LOAD_RESULT_HISTOGRAM_NAME, result, LoadSeedResult.MAX_VALUE + 1);
    }

    private static void recordSeedLoadBlockingTime(long timeMs) {
        RecordHistogram.recordTimesHistogram(SEED_LOAD_BLOCKING_TIME_HISTOGRAM_NAME, timeMs);
    }

    private static void recordAppSeedFreshness(long freshnessMinutes) {
        // Bucket parameters should match Variations.SeedFreshness.
        // See variations::RecordSeedFreshness.
        RecordHistogram.recordCustomCountHistogram(APP_SEED_FRESHNESS_HISTOGRAM_NAME,
                (int) freshnessMinutes, /*min=*/1, /*max=*/(int) TimeUnit.DAYS.toMinutes(30),
                /*numBuckets=*/50);
    }

    private static void recordMinuteHistogram(String name, long value, long maxValue) {
        // 50 buckets from 1min to maxValue minutes.
        RecordHistogram.recordCustomCountHistogram(name, (int) value, 1, (int) maxValue, 50);
    }

    private static boolean shouldThrottleRequests(long now) {
        long lastRequestTime = VariationsUtils.getStampTime();
        if (lastRequestTime == 0) {
            return false;
        }
        long maxRequestPeriodMillis = VariationsUtils.getDurationSwitchValueInMillis(
                AwSwitches.FINCH_SEED_MIN_UPDATE_PERIOD, MAX_REQUEST_PERIOD_MILLIS);
        return now < lastRequestTime + maxRequestPeriodMillis;
    }

    private boolean isSeedExpired(long seedFileTime) {
        long expirationDuration = VariationsUtils.getDurationSwitchValueInMillis(
                AwSwitches.FINCH_SEED_EXPIRATION_AGE, SEED_EXPIRATION_MILLIS);
        return getCurrentTimeMillis() > seedFileTime + expirationDuration;
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
        private boolean mFoundNewSeed;
        private boolean mNeedNewSeed;
        private long mCurrentSeedDate = Long.MIN_VALUE;
        private long mSeedFileTime;

        private boolean parseSeedFile(File seedFile) {
            if (!VariationsSeedLoaderJni.get().parseAndSaveSeedProto(seedFile.getPath())) {
                VariationsUtils.debugLog("Failed reading seed file \"" + seedFile + '"');
                return false;
            }
            return true;
        }

        private FutureTask<Boolean> mLoadTask = new FutureTask<>(() -> {
            File newSeedFile = VariationsUtils.getNewSeedFile();
            File oldSeedFile = VariationsUtils.getSeedFile();

            // First check for a new seed.
            boolean loadedSeed = false;
            if (parseSeedFile(newSeedFile)) {
                loadedSeed = true;
                mSeedFileTime = newSeedFile.lastModified();

                // If a valid new seed was found, make a note to replace the old seed with
                // the new seed. (Don't do it now, to avoid delaying FutureTask.get().)
                mFoundNewSeed = true;
            } else if (parseSeedFile(oldSeedFile)) { // If no new seed, check for an old one.
                loadedSeed = true;
                mSeedFileTime = oldSeedFile.lastModified();
            }

            // Make a note to request a new seed if necessary. (Don't request it now, to
            // avoid delaying FutureTask.get().)
            if (!loadedSeed || isSeedExpired(mSeedFileTime)) {
                mNeedNewSeed = true;

                // Rate-limit the requests.
                if (shouldThrottleRequests(getCurrentTimeMillis())) {
                    mNeedNewSeed = false;
                }
            }

            // Save the date field of whatever seed was loaded, if any.
            if (loadedSeed) {
                mCurrentSeedDate = VariationsSeedLoaderJni.get().getSavedSeedDate();
            }
            return loadedSeed;
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

        public boolean get(long timeout, TimeUnit unit)
                throws InterruptedException, ExecutionException, TimeoutException {
            boolean success = mLoadTask.get(timeout, unit);
            if (mSeedFileTime != 0) {
                long freshnessMinutes =
                        TimeUnit.MILLISECONDS.toMinutes(getCurrentTimeMillis() - mSeedFileTime);
                recordAppSeedFreshness(freshnessMinutes);
            }
            return success;
        }

        public long getLoadedSeedDate() {
            return mCurrentSeedDate;
        }
    }

    // Connects to VariationsSeedServer service. Sends a file descriptor for our local copy of the
    // seed to the service, to which the service will write a new seed.
    private class SeedServerConnection extends ServiceConnectionDelayRecorder {
        private ParcelFileDescriptor mNewSeedFd;
        private long mOldSeedDate;

        public SeedServerConnection(ParcelFileDescriptor newSeedFd, long oldSeedDate) {
            mNewSeedFd = newSeedFd;
            mOldSeedDate = oldSeedDate;
        }

        public void start() {
            try {
                if (!bind(ContextUtils.getApplicationContext(), getServerIntent(),
                            Context.BIND_AUTO_CREATE)) {
                    Log.e(TAG, "Failed to bind to WebView service");
                }
                // Connect to nonembedded metrics Service at the same time we connect to variation
                // service.
                AwBrowserProcess.collectNonembeddedMetrics();
            } catch (NameNotFoundException e) {
                Log.e(TAG,
                        "WebView provider \"" + AwBrowserProcess.getWebViewPackageName()
                                + "\" not found!");
            }
        }

        @Override
        public void onServiceConnectedImpl(ComponentName name, IBinder service) {
            try {
                if (mNewSeedFd.getFd() >= 0) {
                    IVariationsSeedServer.Stub.asInterface(service).getSeed(
                            mNewSeedFd, mOldSeedDate, mSeedServerCallback);
                }
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

    private class SeedServerCallback extends IVariationsSeedServerCallback.Stub {
        @Override
        public void reportVariationsServiceMetrics(Bundle metricsBundle) {
            VariationsServiceMetricsHelper metrics =
                    VariationsServiceMetricsHelper.fromBundle(metricsBundle);
            if (metrics.hasJobInterval()) {
                // Variations.DownloadJobInterval records time in minutes.
                recordMinuteHistogram(DOWNLOAD_JOB_INTERVAL_HISTOGRAM_NAME,
                        TimeUnit.MILLISECONDS.toMinutes(metrics.getJobInterval()),
                        TimeUnit.DAYS.toMinutes(30));
            }
            if (metrics.hasJobQueueTime()) {
                // Variations.DownloadJobQueueTime records time in minutes.
                recordMinuteHistogram(DOWNLOAD_JOB_QUEUE_TIME_HISTOGRAM_NAME,
                        TimeUnit.MILLISECONDS.toMinutes(metrics.getJobQueueTime()),
                        TimeUnit.DAYS.toMinutes(30));
            }
        }
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
    // Returns false if it didn't connect to the service.
    protected boolean requestSeedFromService(long oldSeedDate) {
        File newSeedFile = VariationsUtils.getNewSeedFile();
        try {
            newSeedFile.createNewFile(); // Silently returns false if already exists.
        } catch (IOException e) {
            Log.e(TAG, "Failed to create seed file " + newSeedFile);
            return false;
        }
        ParcelFileDescriptor newSeedFd = null;
        try {
            newSeedFd =
                    ParcelFileDescriptor.open(newSeedFile, ParcelFileDescriptor.MODE_WRITE_ONLY);
        } catch (FileNotFoundException e) {
            Log.e(TAG, "Failed to open seed file " + newSeedFile);
            return false;
        }

        VariationsUtils.debugLog("Requesting new seed from IVariationsSeedServer");
        SeedServerConnection connection = new SeedServerConnection(newSeedFd, oldSeedDate);
        connection.start();

        return true;
    }

    // Begin asynchronously loading the variations seed. ContextUtils.getApplicationContext() and
    // AwBrowserProcess.getWebViewPackageName() must be ready to use before calling this.
    public void startVariationsInit() {
        mRunnable = new SeedLoadAndUpdateRunnable();
        (new Thread(mRunnable)).start();
    }

    // Block on loading the seed with a timeout. Then if a seed was successfully loaded, initialize
    // variations. Returns whether or not variations was initialized.
    public boolean finishVariationsInit() {
        long start = SystemClock.elapsedRealtime();
        try {
            try {
                boolean gotSeed = mRunnable.get(getSeedLoadTimeoutMillis(), TimeUnit.MILLISECONDS);
                // Log the seed age to help with debugging.
                long seedDate = mRunnable.getLoadedSeedDate();
                if (gotSeed && seedDate > 0) {
                    long seedAge = TimeUnit.MILLISECONDS.toSeconds(new Date().getTime() - seedDate);
                    // Changes to the log message below must be accompanied with changes to WebView
                    // finch smoke tests since they look for this message in the logcat.
                    VariationsUtils.debugLog("Loaded seed with age " + seedAge + "s");
                }
                return gotSeed;
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
        return false;
    }

    @NativeMethods
    interface Natives {
        // Parses the AwVariationsSeed proto stored in the file with the given path, saving it in
        // memory for later use by native code if the parsing succeeded. Returns true if the loading
        // and parsing were successful.
        boolean parseAndSaveSeedProto(String path);

        // Returns the timestamp in millis since unix epoch that the saved seed was generated on
        // the server. This value corresponds to the |date| field in the AwVariationsSeed proto.
        long getSavedSeedDate();
    }
}
