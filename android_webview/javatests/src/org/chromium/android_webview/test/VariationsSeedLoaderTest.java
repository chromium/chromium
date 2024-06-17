// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.variations.VariationsServiceMetricsHelper;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.android_webview.test.services.MockVariationsSeedServer;
import org.chromium.android_webview.test.util.VariationsTestUtils;
import org.chromium.android_webview.variations.VariationsSeedLoader;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Test VariationsSeedLoader. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
public class VariationsSeedLoaderTest extends AwParameterizedTest {
    private static final long CURRENT_TIME_MILLIS = 1234567890;
    private static final long EXPIRED_TIMESTAMP = 0;
    private static final long TIMEOUT_MILLIS = 10000;

    // Needed for tests that test histograms, which rely on native code.
    @Rule public AwActivityTestRule mActivityTestRule;

    /**
     * Helper class to interact with {@link TestLoader}. This can be used to retrieve whether
     * TestLoader requested a seed.
     */
    public static class TestLoaderResult extends CallbackHelper {
        private volatile boolean mBackgroundWorkFinished;
        private volatile boolean mForegroundWorkFinished;
        private volatile boolean mSeedRequested;

        public boolean wasSeedRequested() {
            assert getCallCount() > 0;
            return mSeedRequested;
        }

        public void markSeedRequested() {
            mSeedRequested = true;
        }

        public void onBackgroundWorkFinished() {
            mBackgroundWorkFinished = true;
            maybeNotifyCalled();
        }

        public void onForegroundWorkFinished() {
            mForegroundWorkFinished = true;
            maybeNotifyCalled();
        }

        private void maybeNotifyCalled() {
            if (mBackgroundWorkFinished && mForegroundWorkFinished) {
                notifyCalled();
            }
        }
    }

    /**
     * A {@link VariationsSeedLoader} which is suitable for integration tests. This overrides the
     * default timeout to be suitable for integration tests, allowing the test to call
     * startVariationsInit() immediately before finishVariationsInit(). This also overrides the
     * service Intent to match the test environment.
     */
    public static class TestLoader extends VariationsSeedLoader {
        private TestLoaderResult mResult;

        public TestLoader(TestLoaderResult result) {
            mResult = result;
        }

        // Bind to the MockVariationsSeedServer built in to the instrumentation test app, rather
        // than the real server in the WebView provider.
        @Override
        protected Intent getServerIntent() {
            return new Intent(ContextUtils.getApplicationContext(), MockVariationsSeedServer.class);
        }

        @Override
        protected boolean requestSeedFromService(long oldSeedDate) {
            boolean result = super.requestSeedFromService(oldSeedDate);
            mResult.markSeedRequested();
            return result;
        }

        @Override
        protected void onBackgroundWorkFinished() {
            mResult.onBackgroundWorkFinished();
        }

        @Override
        protected long getSeedLoadTimeoutMillis() {
            return TIMEOUT_MILLIS;
        }

        @Override
        protected long getCurrentTimeMillis() {
            return CURRENT_TIME_MILLIS;
        }
    }

    private Handler mMainHandler;

    public VariationsSeedLoaderTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    // Create a TestLoader, run it on the UI thread, and block until it's finished. The return value
    // indicates whether the loader decided to request a new seed.
    private boolean runTestLoaderBlocking() throws TimeoutException {
        final TestLoaderResult result = new TestLoaderResult();
        Runnable run =
                () -> {
                    TestLoader loader = new TestLoader(result);
                    loader.startVariationsInit();
                    loader.finishVariationsInit();
                    result.onForegroundWorkFinished();
                };

        CallbackHelper onRequestReceived = MockVariationsSeedServer.getRequestHelper();
        int requestsReceived = onRequestReceived.getCallCount();
        Assert.assertTrue("Failed to post seed loader Runnable", mMainHandler.post(run));
        result.waitForCallback("Timed out waiting for loader to finish.", 0);
        if (result.wasSeedRequested()) {
            onRequestReceived.waitForCallback(
                    "Seed requested, but timed out waiting for request"
                            + " to arrive in MockVariationsSeedServer",
                    requestsReceived);
            return true;
        }
        return false;
    }

    @Before
    public void setUp() throws IOException {
        mMainHandler = new Handler(Looper.getMainLooper());
        VariationsTestUtils.deleteSeeds();
    }

    @After
    public void tearDown() throws IOException {
        VariationsTestUtils.deleteSeeds();
    }

    // Test that Seed and AppSeed Freshness diff is correct and recorded
    @Test
    @MediumTest
    public void testRecordSeedDiff() throws Exception {
        // The first line is needed to set the seed freshness to zero
        // in order to calculate the diff correctly
        VariationsSeedLoader.cacheSeedFreshness(0);
        long seedFreshnessInMinutes = 100;
        long appSeedFreshnessInMinutes = 40;
        long diff = seedFreshnessInMinutes - appSeedFreshnessInMinutes;
        var histogramWatcherOne =
                HistogramWatcher.newSingleRecordWatcher(
                        VariationsSeedLoader.SEED_FRESHNESS_DIFF_HISTOGRAM_NAME, (int) diff);
        VariationsSeedLoader.cacheAppSeedFreshness(appSeedFreshnessInMinutes);
        VariationsSeedLoader.cacheSeedFreshness(seedFreshnessInMinutes);
        histogramWatcherOne.assertExpected();

        var histogramWatcherTwo =
                HistogramWatcher.newSingleRecordWatcher(
                        VariationsSeedLoader.SEED_FRESHNESS_DIFF_HISTOGRAM_NAME, (int) diff);
        VariationsSeedLoader.cacheSeedFreshness(seedFreshnessInMinutes);
        VariationsSeedLoader.cacheAppSeedFreshness(appSeedFreshnessInMinutes);
        histogramWatcherTwo.assertExpected();
    }

    // Test the case that:
    // VariationsUtils.getSeedFile() - doesn't exist
    // VariationsUtils.getNewSeedFile() - doesn't exist
    @Test
    @MediumTest
    public void testHaveNoSeed() throws Exception {
        try {
            boolean seedRequested = runTestLoaderBlocking();

            // Since there was no seed, another seed should be requested.
            Assert.assertTrue("No seed requested", seedRequested);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    // Test the case that:
    // VariationsUtils.getSeedFile() - exists, timestamp = now
    // VariationsUtils.getNewSeedFile() - doesn't exist
    @Test
    @MediumTest
    public void testHaveFreshSeed() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            Assert.assertTrue("Seed file already exists", oldFile.createNewFile());
            VariationsTestUtils.writeMockSeed(oldFile);

            boolean seedRequested = runTestLoaderBlocking();

            // Since there was a fresh seed, we should not request another seed.
            Assert.assertFalse(
                    "New seed was requested when it should not have been", seedRequested);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    // Test the case that:
    // VariationsUtils.getSeedFile() - exists, timestamp = epoch
    // VariationsUtils.getNewSeedFile() - doesn't exist
    @Test
    @MediumTest
    public void testHaveExpiredSeed() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            Assert.assertTrue("Seed file already exists", oldFile.createNewFile());
            VariationsTestUtils.writeMockSeed(oldFile);
            oldFile.setLastModified(0);

            boolean seedRequested = runTestLoaderBlocking();

            // Since the seed was expired, another seed should be requested.
            Assert.assertTrue("No seed requested", seedRequested);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    // Test the case that:
    // VariationsUtils.getSeedFile() - doesn't exist
    // VariationsUtils.getNewSeedFile() - exists, timestamp = now
    @Test
    @MediumTest
    public void testHaveFreshNewSeed() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            File newFile = VariationsUtils.getNewSeedFile();
            Assert.assertTrue("New seed file already exists", newFile.createNewFile());
            VariationsTestUtils.writeMockSeed(newFile);

            boolean seedRequested = runTestLoaderBlocking();

            // The "new" seed should have been renamed to the "old" seed.
            Assert.assertTrue("Old seed not found", oldFile.exists());
            Assert.assertFalse("New seed still exists", newFile.exists());

            // Since the "new" seed was fresh, we should not request another seed.
            Assert.assertFalse(
                    "New seed was requested when it should not have been", seedRequested);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    // Test the case that:
    // VariationsUtils.getSeedFile() - doesn't exist
    // VariationsUtils.getNewSeedFile() - exists, timestamp = epoch
    @Test
    @MediumTest
    public void testHaveExpiredNewSeed() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            File newFile = VariationsUtils.getNewSeedFile();
            Assert.assertTrue("Seed file already exists", newFile.createNewFile());
            VariationsTestUtils.writeMockSeed(newFile);
            newFile.setLastModified(0);

            boolean seedRequested = runTestLoaderBlocking();

            // The "new" seed should have been renamed to the "old" seed. Another empty "new" seed
            // should have been created as a destination for the request.
            Assert.assertTrue("Old seed not found", oldFile.exists());
            Assert.assertTrue("New seed not found", newFile.exists());
            Assert.assertTrue("New seed is not empty", newFile.length() == 0L);

            // Since the "new" seed was expired, another seed should be requested.
            Assert.assertTrue("No seed requested", seedRequested);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    // Test the case that:
    // VariationsUtils.getSeedFile() - doesn't exist
    // VariationsUtils.getNewSeedFile() - exists, empty
    @Test
    @MediumTest
    public void testHaveEmptyNewSeed() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            File newFile = VariationsUtils.getNewSeedFile();
            Assert.assertTrue("Seed file should not already exist", newFile.createNewFile());

            boolean seedRequested = runTestLoaderBlocking();

            // Neither file should have been touched.
            Assert.assertFalse("Old seed file should not exist", oldFile.exists());
            Assert.assertTrue("New seed file not found", newFile.exists());
            Assert.assertEquals("New seed file is not empty", 0L, newFile.length());

            // Since the "new" seed was empty/invalid, another seed should be requested.
            Assert.assertTrue("No seed requested", seedRequested);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    // Test the case that:
    // VariationsUtils.getSeedFile() - exists, timestamp = epoch
    // VariationsUtils.getNewSeedFile() - exists, timestamp = epoch + 1 day
    @Test
    @MediumTest
    public void testHaveBothExpiredSeeds() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            Assert.assertTrue("Old seed file already exists", oldFile.createNewFile());
            VariationsTestUtils.writeMockSeed(oldFile);
            oldFile.setLastModified(0);

            File newFile = VariationsUtils.getNewSeedFile();
            Assert.assertTrue("New seed file already exists", newFile.createNewFile());
            VariationsTestUtils.writeMockSeed(newFile);
            newFile.setLastModified(TimeUnit.DAYS.toMillis(1));

            boolean seedRequested = runTestLoaderBlocking();

            // The "new" seed should have been renamed to the "old" seed. Another empty "new" seed
            // should have been created as a destination for the request.
            Assert.assertTrue("Old seed not found", oldFile.exists());
            Assert.assertTrue("New seed not found", newFile.exists());
            Assert.assertTrue("New seed is not empty", newFile.length() == 0L);

            // Since the "new" seed was expired, another seed should be requested.
            Assert.assertTrue("No seed requested", seedRequested);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    // Test loading twice. The first load should trigger a request, but the second should not,
    // because requests should be rate-limited.
    // VariationsUtils.getSeedFile() - doesn't exist VariationsUtils.getNewSeedFile() - doesn't
    // exist
    @Test
    @MediumTest
    public void testDoubleLoad() throws Exception {
        try {
            boolean seedRequested = runTestLoaderBlocking();
            Assert.assertTrue("No seed requested", seedRequested);

            seedRequested = runTestLoaderBlocking();
            Assert.assertFalse(
                    "New seed was requested when it should not have been", seedRequested);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    // Tests that the finch-seed-expiration-age flag works.
    @Test
    @MediumTest
    @CommandLineFlags.Add(AwSwitches.FINCH_SEED_EXPIRATION_AGE + "=0")
    public void testFinchSeedExpirationAgeFlag() throws Exception {
        try {
            // Create a new seed file with a recent timestamp.
            File oldFile = VariationsUtils.getSeedFile();
            VariationsTestUtils.writeMockSeed(oldFile);
            oldFile.setLastModified(CURRENT_TIME_MILLIS);

            boolean seedRequested = runTestLoaderBlocking();

            Assert.assertTrue("Seed file should be requested", seedRequested);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    // Tests that the finch-seed-min-update-period flag overrides the seed request throttling.
    @Test
    @MediumTest
    @CommandLineFlags.Add(AwSwitches.FINCH_SEED_MIN_UPDATE_PERIOD + "=0")
    public void testFinchSeedMinUpdatePeriodFlag() throws Exception {
        try {
            // Update the last modified time of the stamp file to simulate having just requested a
            // new seed from the service.
            VariationsUtils.getStampFile().createNewFile();
            VariationsUtils.updateStampTime(CURRENT_TIME_MILLIS);

            boolean seedRequested = runTestLoaderBlocking();

            Assert.assertTrue("Seed file should be requested", seedRequested);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    // Tests that metrics passed from the service get recorded to histograms.
    @Test
    @MediumTest
    public void testRecordMetricsFromService() throws Exception {
        try {
            long nineMinutesMs = TimeUnit.MINUTES.toMillis(9);
            long twoWeeksMs = TimeUnit.DAYS.toMillis(14);
            long threeWeeksMs = TimeUnit.DAYS.toMillis(21);
            HistogramWatcher histogramExpectationInterval =
                    HistogramWatcher.newBuilder()
                            .expectIntRecordTimes(
                                    VariationsSeedLoader.DOWNLOAD_JOB_INTERVAL_HISTOGRAM_NAME,
                                    (int) TimeUnit.MILLISECONDS.toMinutes(threeWeeksMs),
                                    1)
                            .build();
            HistogramWatcher histogramExpectationQueueTime =
                    HistogramWatcher.newBuilder()
                            .expectIntRecordTimes(
                                    VariationsSeedLoader.DOWNLOAD_JOB_QUEUE_TIME_HISTOGRAM_NAME,
                                    (int) TimeUnit.MILLISECONDS.toMinutes(twoWeeksMs),
                                    1)
                            .build();

            VariationsServiceMetricsHelper metrics =
                    VariationsServiceMetricsHelper.fromBundle(new Bundle());
            metrics.setJobInterval(threeWeeksMs);
            metrics.setJobQueueTime(twoWeeksMs);
            MockVariationsSeedServer.setMetricsBundle(metrics.toBundle());

            runTestLoaderBlocking();
            histogramExpectationInterval.assertExpected();
            histogramExpectationQueueTime.assertExpected();
        } finally {
            MockVariationsSeedServer.setMetricsBundle(null);
        }
    }
}
