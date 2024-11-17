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

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

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
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.variations.LoadSeedResult;

import java.io.File;
import java.io.FileNotFoundException;
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
    public static final String SEED_FRESHNESS_DIFF_HISTOGRAM_NAME = "Variations.SeedFreshnessDiff";

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
    // These two variables below are used for caching the difference between Seed and
    // AppSeed Freshness.
    private static long sCachedSeedFreshness;
    private static long sCachedAppSeedFreshness;

    private FutureTask<SeedLoadResult> mLoadTask;
    private SeedServerCallback mSeedServerCallback = new SeedServerCallback();

    private static void recordLoadSeedResult(@LoadSeedResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                SEED_LOAD_RESULT_HISTOGRAM_NAME, result, LoadSeedResult.MAX_VALUE + 1);
    }

    private static void recordSeedLoadBlockingTime(long timeMs) {
        RecordHistogram.recordTimesHistogram(SEED_LOAD_BLOCKING_TIME_HISTOGRAM_NAME, timeMs);
    }

    private static void recordAppSeedFreshness(long appSeedFreshnessMinutes) {
        // Bucket parameters should match Variations.SeedFreshness.
        // See variations::RecordSeedFreshness.
        RecordHistogram.recordCustomCountHistogram(
                APP_SEED_FRESHNESS_HISTOGRAM_NAME,
                (int) appSeedFreshnessMinutes,
                /* min= */ 1,
                /* max= */ (int) TimeUnit.DAYS.toMinutes(30),
                /* numBuckets= */ 50);
        cacheAppSeedFreshness(appSeedFreshnessMinutes);
    }

    // This method is to cache the AppSeedFreshness value
    @VisibleForTesting
    public static void cacheAppSeedFreshness(long appSeedFreshnessMinutes) {
        if (appSeedFreshnessMinutes < 0) {
            return;
        }
        sCachedAppSeedFreshness = appSeedFreshnessMinutes;
        calculateSeedFreshnessDiff();
    }

    // This method is to cache the SeedFreshness value
    @CalledByNative
    @VisibleForTesting
    public static void cacheSeedFreshness(long seedFreshness) {
        if (seedFreshness < 0) {
            return;
        }
        sCachedSeedFreshness = seedFreshness;
        calculateSeedFreshnessDiff();
    }

    // This method is to calculate the difference between SeedFreshness
    // and AppSeedFreshness
    private static void calculateSeedFreshnessDiff() {
        if (sCachedSeedFreshness == 0 || sCachedAppSeedFreshness == 0) {
            return;
        }
        long diff = sCachedSeedFreshness - sCachedAppSeedFreshness;
        recordAppSeedFreshnessDiff(diff);
    }

    // This method is to record the difference between SeedFreshness
    // and AppSeedFreshness
    private static void recordAppSeedFreshnessDiff(long diff) {
        RecordHistogram.recordCustomCountHistogram(
                SEED_FRESHNESS_DIFF_HISTOGRAM_NAME,
                (int) diff,
                /* min= */ 1,
                /* max= */ (int) TimeUnit.DAYS.toMinutes(30),
                /* numBuckets= */ 50);
        sCachedSeedFreshness = 0;
        sCachedAppSeedFreshness = 0;
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
        long maxRequestPeriodMillis =
                VariationsUtils.getDurationSwitchValueInMillis(
                        AwSwitches.FINCH_SEED_MIN_UPDATE_PERIOD, MAX_REQUEST_PERIOD_MILLIS);
        return now < lastRequestTime + maxRequestPeriodMillis;
    }

    private boolean isSeedExpired(long seedFileTime) {
        long expirationDuration =
                VariationsUtils.getDurationSwitchValueInMillis(
                        AwSwitches.FINCH_SEED_EXPIRATION_AGE, SEED_EXPIRATION_MILLIS);
        return getCurrentTimeMillis() > seedFileTime + expirationDuration;
    }

    public static boolean parseAndSaveSeedFile(File seedFile) {
        if (!VariationsSeedLoaderJni.get().parseAndSaveSeedProto(seedFile.getPath())) {
            VariationsUtils.debugLog("Failed reading seed file \"" + seedFile + '"');
            return false;
        }
        return true;
    }

    public static boolean parseAndSaveSeedProtoFromByteArray(byte[] seedAsByteArray) {
        if (!VariationsSeedLoaderJni.get().parseAndSaveSeedProtoFromByteArray(seedAsByteArray)) {
            VariationsUtils.debugLog("Failed reading seed as string");
            return false;
        }
        return true;
    }

    public static void maybeRecordSeedFileTime(long seedFileTime) {
        if (seedFileTime != 0) {
            long freshnessMinutes =
                    TimeUnit.MILLISECONDS.toMinutes(new Date().getTime() - seedFileTime);
            recordAppSeedFreshness(freshnessMinutes);
        }
    }

    /** Result of loading the local copy of the seed. */
    private static class SeedLoadResult {
        /** Whether the seed was loaded successfully. */
        final boolean mLoadedSeed;

        /**
         * The "date" field of our local seed, converted to milliseconds since epoch, or
         * Long.MIN_VALUE if we have no seed. This value originates from the server.
         */
        final long mCurrentSeedDate;

        /**
         * The time, in milliseconds since the UNIX epoch, our local copy of the seed was last
         * written to disk as measured by the device's clock.
         */
        final long mSeedFileTime;

        private SeedLoadResult(boolean loadedSeed, long mCurrentSeedDate, long seedFileTime) {
            this.mLoadedSeed = loadedSeed;
            this.mCurrentSeedDate = mCurrentSeedDate;
            this.mSeedFileTime = seedFileTime;
        }
    }

    /**
     * Load the current variations seed file.
     *
     * <p>This method should be posted to a background thread to ensure that other initialization
     * work can happen concurrently.
     */
    @NonNull
    private SeedLoadResult loadSeedFile() {
        File newSeedFile = VariationsUtils.getNewSeedFile();
        File oldSeedFile = VariationsUtils.getSeedFile();
        long currentSeedDate = Long.MIN_VALUE;
        long seedFileTime = 0;
        boolean loadedSeed = false;
        boolean foundNewSeed = false;
        // First check for a new seed.
        if (parseAndSaveSeedFile(newSeedFile)) {
            loadedSeed = true;
            seedFileTime = newSeedFile.lastModified();

            // If a valid new seed was found, make a note to replace the old
            // seed with the new seed. (Don't do it now, to avoid delaying
            // FutureTask.get().)
            foundNewSeed = true;
        } else if (parseAndSaveSeedFile(oldSeedFile)) {
            // If no new seed, check for an old one.
            loadedSeed = true;
            seedFileTime = oldSeedFile.lastModified();
        }

        // Make a note to request a new seed if necessary. (Don't request it
        // now, to avoid delaying FutureTask.get().)
        boolean needNewSeed = false;
        if (!loadedSeed || isSeedExpired(seedFileTime)) {
            // Rate-limit the requests.
            needNewSeed = !shouldThrottleRequests(getCurrentTimeMillis());
        }

        // Save the date field of whatever seed was loaded, if any.
        if (loadedSeed) {
            currentSeedDate = VariationsSeedLoaderJni.get().getSavedSeedDate();
        }

        // Schedule a task to update the seed files from the service.
        updateSeedFileAndRequestNewFromServiceOnBackgroundThread(
                foundNewSeed, needNewSeed, currentSeedDate);

        return new SeedLoadResult(loadedSeed, currentSeedDate, seedFileTime);
    }

    /**
     * Post a task to replace the old seed with a new one and request an update.
     *
     * @param foundNewSeed Is a "new" seed file present? (If so, it should be renamed to an "old"
     *     seed, replacing any existing "old" seed.)
     * @param needNewSeed Should we request a new seed from the service?
     * @param seedFileTime timestamp of the current seed file.
     */
    private void updateSeedFileAndRequestNewFromServiceOnBackgroundThread(
            boolean foundNewSeed, boolean needNewSeed, long seedFileTime) {
        // This work is not time critical.
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    if (foundNewSeed) {
                        // The move happens synchronously. It's not possible for the service to
                        // still be writing to this file when we move it, because foundNewSeed means
                        // we already read the seed and found it to be complete. Therefore the
                        // service must have already finished writing.
                        VariationsUtils.replaceOldWithNewSeed();
                    }

                    if (needNewSeed) {
                        // The new seed will arrive asynchronously; the new seed file is written by
                        // the service, and may complete after this app process has died.
                        requestSeedFromService(seedFileTime);
                        VariationsUtils.updateStampTime();
                    }

                    onBackgroundWorkFinished();
                });
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
                if (!bind(
                        ContextUtils.getApplicationContext(),
                        getServerIntent(),
                        Context.BIND_AUTO_CREATE)) {
                    Log.e(TAG, "Failed to bind to WebView service");
                    // If we don't close the file descriptor here, it will be leaked since this
                    // service only wants to close it once the service has been connected.
                    // Problematic if we can't connect to the service in the first place.
                    VariationsUtils.closeSafely(mNewSeedFd);
                }
                // Connect to nonembedded metrics Service at the same time we connect to variation
                // service.
                AwBrowserProcess.collectNonembeddedMetrics();
            } catch (NameNotFoundException e) {
                Log.e(
                        TAG,
                        "WebView provider \""
                                + AwBrowserProcess.getWebViewPackageName()
                                + "\" not found!");
            }
        }

        @Override
        public void onServiceConnectedImpl(ComponentName name, IBinder service) {
            try {
                if (mNewSeedFd.getFd() >= 0) {
                    IVariationsSeedServer.Stub.asInterface(service)
                            .getSeed(mNewSeedFd, mOldSeedDate, mSeedServerCallback);
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

    private static class SeedServerCallback extends IVariationsSeedServerCallback.Stub {
        @Override
        public void reportVariationsServiceMetrics(Bundle metricsBundle) {
            VariationsServiceMetricsHelper metrics =
                    VariationsServiceMetricsHelper.fromBundle(metricsBundle);
            if (metrics.hasJobInterval()) {
                // Variations.DownloadJobInterval records time in minutes.
                recordMinuteHistogram(
                        DOWNLOAD_JOB_INTERVAL_HISTOGRAM_NAME,
                        TimeUnit.MILLISECONDS.toMinutes(metrics.getJobInterval()),
                        TimeUnit.DAYS.toMinutes(30));
            }
            if (metrics.hasJobQueueTime()) {
                // Variations.DownloadJobQueueTime records time in minutes.
                recordMinuteHistogram(
                        DOWNLOAD_JOB_QUEUE_TIME_HISTOGRAM_NAME,
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
        ParcelFileDescriptor newSeedFd = null;
        try {
            newSeedFd =
                    ParcelFileDescriptor.open(
                            newSeedFile,
                            ParcelFileDescriptor.MODE_WRITE_ONLY
                                    | ParcelFileDescriptor.MODE_TRUNCATE
                                    | ParcelFileDescriptor.MODE_CREATE);
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
        mLoadTask = new FutureTask<>(this::loadSeedFile);
        // The Runnable task must be scheduled with high priority to start the FutureTask as soon as
        // possible since that task is blocking WebView startup.
        PostTask.postTask(TaskTraits.USER_BLOCKING_MAY_BLOCK, mLoadTask);
    }

    // Block on loading the seed with a timeout. Then if a seed was successfully loaded, initialize
    // variations. Returns whether or not variations was initialized.
    public boolean finishVariationsInit() {
        long start = SystemClock.elapsedRealtime();
        try {
            try {
                SeedLoadResult loadResult =
                        mLoadTask.get(getSeedLoadTimeoutMillis(), TimeUnit.MILLISECONDS);
                maybeRecordSeedFileTime(loadResult.mSeedFileTime);
                boolean gotSeed = loadResult.mLoadedSeed;
                // Log the seed age to help with debugging.
                long seedDate = loadResult.mCurrentSeedDate;
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

        // Parses the AwVariationsSeed proto stored in the given byte array, saving it in
        // memory for later use by native code if the parsing succeeded. Returns true if the loading
        // and parsing were successful.
        boolean parseAndSaveSeedProtoFromByteArray(byte[] seedAsByteArray);

        // Returns the timestamp in millis since unix epoch that the saved seed was generated on
        // the server. This value corresponds to the |date| field in the AwVariationsSeed proto.
        long getSavedSeedDate();
    }
}
