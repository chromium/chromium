// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.VariationsSeedLoader;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.android_webview.test.services.MockVariationsSeedServer;
import org.chromium.android_webview.test.util.VariationsTestUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test VariationsSeedLoader.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class VariationsSeedLoaderTest {
    private static final long CURRENT_TIME_MILLIS = 1234567890;
    private static final long EXPIRED_TIMESTAMP = 0;
    private static final long TIMEOUT_MILLIS = 10000;

    // Needed for tests that test histograms, which rely on native code.
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static class TestLoaderResult extends CallbackHelper {
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

    private static class TestLoader extends VariationsSeedLoader {
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
        protected void requestSeedFromService(long oldSeedDate) {
            super.requestSeedFromService(oldSeedDate);
            mResult.markSeedRequested();
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

    // Create a TestLoader, run it on the UI thread, and block until it's finished. The return value
    // indicates whether the loader decided to request a new seed.
    private boolean runTestLoaderBlocking() throws TimeoutException {
        final TestLoaderResult result = new TestLoaderResult();
        Runnable run = () -> {
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
            onRequestReceived.waitForCallback("Seed requested, but timed out waiting for request"
                            + " to arrive in MockVariationsSeedServer",
                    requestsReceived);
            return true;
        }
        return false;
    }

    @Before
    public void setUp() throws IOException {
        mMainHandler = new Handler(Looper.getMainLooper());
        RecordHistogram.setDisabledForTests(true);
        VariationsTestUtils.deleteSeeds();
    }

    @After
    public void tearDown() throws IOException {
        RecordHistogram.setDisabledForTests(false);
        VariationsTestUtils.deleteSeeds();
    }

    private void assertNoAppSeedRequestStateValues() {
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedLoader.APP_SEED_REQUEST_STATE_HISTOGRAM_NAME));
    }

    private void assertSingleAppSeedRequestStateValue(
            @VariationsSeedLoader.AppSeedRequestState int state) {
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedLoader.APP_SEED_REQUEST_STATE_HISTOGRAM_NAME));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedLoader.APP_SEED_REQUEST_STATE_HISTOGRAM_NAME, state));
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
            Assert.assertFalse("New seed was requested when it should not have been",
                    seedRequested);
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
            Assert.assertFalse("New seed was requested when it should not have been",
                    seedRequested);
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
    // VariationsUtils.getSeedFile() - doesn't exist
    // VariationsUtils.getNewSeedFile() - doesn't exist
    @Test
    @MediumTest
    public void testDoubleLoad() throws Exception {
        try {
            boolean seedRequested = runTestLoaderBlocking();
            Assert.assertTrue("No seed requested", seedRequested);

            seedRequested = runTestLoaderBlocking();
            Assert.assertFalse("New seed was requested when it should not have been",
                    seedRequested);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    // Test we record the Variations.AppSeedRequestState metric when the seed is fresh.
    @Test
    @MediumTest
    public void testRecordSeedFresh() throws Exception {
        RecordHistogram.setDisabledForTests(false);
        File oldFile = VariationsUtils.getSeedFile();
        Assert.assertTrue("Expected seed file to not already exist", oldFile.createNewFile());
        VariationsTestUtils.writeMockSeed(oldFile);
        oldFile.setLastModified(CURRENT_TIME_MILLIS);
        assertNoAppSeedRequestStateValues();

        runTestLoaderBlocking();

        assertSingleAppSeedRequestStateValue(VariationsSeedLoader.AppSeedRequestState.SEED_FRESH);
    }

    // Test we record the Variations.AppSeedRequestState metric when a new seed is requested.
    @Test
    @MediumTest
    public void testRecordSeedRequested() throws Exception {
        RecordHistogram.setDisabledForTests(false);
        File oldFile = VariationsUtils.getSeedFile();
        Assert.assertTrue("Expected seed file to not already exist", oldFile.createNewFile());
        VariationsTestUtils.writeMockSeed(oldFile);
        oldFile.setLastModified(EXPIRED_TIMESTAMP);
        assertNoAppSeedRequestStateValues();

        runTestLoaderBlocking();

        assertSingleAppSeedRequestStateValue(
                VariationsSeedLoader.AppSeedRequestState.SEED_REQUESTED);
    }

    // Test we record the Variations.AppSeedRequestState metric when a seed request is throttled.
    @Test
    @MediumTest
    public void testRecordSeedRequestThrottled() throws Exception {
        RecordHistogram.setDisabledForTests(false);
        File oldFile = VariationsUtils.getSeedFile();
        Assert.assertTrue("Expected seed file to not already exist", oldFile.createNewFile());
        VariationsTestUtils.writeMockSeed(oldFile);
        oldFile.setLastModified(EXPIRED_TIMESTAMP);
        // Update the last modified time of the stamp file to simulate having just requested a
        // new seed from the service.
        VariationsUtils.updateStampTime();
        assertNoAppSeedRequestStateValues();

        runTestLoaderBlocking();

        assertSingleAppSeedRequestStateValue(
                VariationsSeedLoader.AppSeedRequestState.SEED_REQUEST_THROTTLED);
    }

    // Test we record the Variations.AppSeedFreshness metric with loading a seed.
    @Test
    @MediumTest
    public void testRecordAppSeedFreshness() throws Exception {
        long seedAgeHours = 2;
        RecordHistogram.setDisabledForTests(false);
        File oldFile = VariationsUtils.getSeedFile();
        Assert.assertTrue("Expected seed file to not already exist", oldFile.createNewFile());
        VariationsTestUtils.writeMockSeed(oldFile);
        oldFile.setLastModified(CURRENT_TIME_MILLIS - TimeUnit.HOURS.toMillis(seedAgeHours));

        runTestLoaderBlocking();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedLoader.APP_SEED_FRESHNESS_HISTOGRAM_NAME,
                        (int) TimeUnit.HOURS.toMinutes(seedAgeHours)));
    }
}
