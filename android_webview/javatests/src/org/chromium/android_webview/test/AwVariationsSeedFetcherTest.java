// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.annotation.SuppressLint;
import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobWorkItem;
import android.content.ComponentName;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.android_webview.services.AwVariationsSeedFetcher;
import org.chromium.android_webview.test.util.VariationsTestUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test AwVariationsSeedFetcher.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class AwVariationsSeedFetcherTest {
    // Jan 1, 2019 12:00AM GMT
    private static final long FAKE_NOW_MS = 1546300800000L;

    // A mock JobScheduler which only holds one job, and never does anything with it.
    private class MockJobScheduler extends JobScheduler {
        public JobInfo mJob;

        public void clear() {
            mJob = null;
        }

        public void assertScheduledWithDelayEqualTo(long delay) {
            Assert.assertNotNull("No job scheduled", mJob);
            Assert.assertEquals(
                    "Job scheduled with wrong delay", delay, mJob.getMinLatencyMillis());
        }

        @Override
        public void cancel(int jobId) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void cancelAll() {
            throw new UnsupportedOperationException();
        }

        @Override
        public int enqueue(JobInfo job, JobWorkItem work) {
            throw new UnsupportedOperationException();
        }

        @Override
        public List<JobInfo> getAllPendingJobs() {
            ArrayList<JobInfo> list = new ArrayList<>();
            if (mJob != null) list.add(mJob);
            return list;
        }

        @Override
        public JobInfo getPendingJob(int jobId) {
            if (mJob != null && mJob.getId() == jobId) return mJob;
            return null;
        }

        @Override
        public int schedule(JobInfo job) {
            Assert.assertEquals(
                    "Job scheduled with wrong ID", AwVariationsSeedFetcher.JOB_ID, job.getId());
            Assert.assertEquals("Job scheduled with wrong network type",
                    JobInfo.NETWORK_TYPE_ANY, job.getNetworkType());
            Assert.assertTrue("Job scheduled without charging requirement",
                    job.isRequireCharging());
            mJob = job;
            return JobScheduler.RESULT_SUCCESS;
        }
    }

    // A mock VariationsSeedFetcher which doesn't actually download seeds, but verifies the request
    // parameters.
    private class MockFetcher extends VariationsSeedFetcher {
        public CallbackHelper helper = new CallbackHelper();

        @Override
        public SeedInfo downloadContent(@VariationsSeedFetcher.VariationsPlatform int platform,
                String restrictMode, String milestone, String channel) {
            Assert.assertEquals(VariationsSeedFetcher.VariationsPlatform.ANDROID_WEBVIEW, platform);
            Assert.assertTrue(Integer.parseInt(milestone) > 0);
            helper.notifyCalled();
            return null;
        }
    }

    private MockJobScheduler mScheduler = new MockJobScheduler();
    private MockFetcher mDownloader = new MockFetcher();

    @Before
    public void setUp() throws IOException {
        AwVariationsSeedFetcher.setMocks(mScheduler, mDownloader);
        AwVariationsSeedFetcher.setMinJobPeriodMillisForTesting(TimeUnit.DAYS.toMillis(1));
        VariationsTestUtils.deleteSeeds();
    }

    @After
    public void tearDown() throws IOException {
        AwVariationsSeedFetcher.setMocks(null, null);
        VariationsTestUtils.deleteSeeds();
    }

    // Test scheduleIfNeeded(), which should schedule a job.
    @Test
    @SmallTest
    public void testScheduleWithNoStamp() {
        try {
            AwVariationsSeedFetcher.scheduleIfNeeded(FAKE_NOW_MS);
            mScheduler.assertScheduledWithDelayEqualTo(0);
        } finally {
            mScheduler.clear();
        }
    }

    // Create a stamp file with time = epoch, indicating the download job hasn't run in a long time.
    // Then test scheduleIfNeeded(), which should schedule a job.
    @Test
    @MediumTest
    public void testScheduleWithExpiredStamp() throws IOException {
        File stamp = VariationsUtils.getStampFile();
        try {
            Assert.assertFalse("Stamp file already exists", stamp.exists());
            Assert.assertTrue("Failed to create stamp file", stamp.createNewFile());
            Assert.assertTrue("Failed to set stamp time", stamp.setLastModified(0));
            AwVariationsSeedFetcher.scheduleIfNeeded(FAKE_NOW_MS);
            mScheduler.assertScheduledWithDelayEqualTo(0);
        } finally {
            mScheduler.clear();
            VariationsTestUtils.deleteSeeds(); // Remove the stamp file.
        }
    }

    // Create a stamp file with time = 7 hours ago, indicating the download job ran recently. Then
    // test scheduleIfNeeded(), which should schedule the job 17 hours in the future.
    @Test
    @MediumTest
    public void testScheduleWithFreshStamp() throws IOException {
        long seedAge = TimeUnit.HOURS.toMillis(7);
        File stamp = VariationsUtils.getStampFile();
        try {
            long stampLastModified = FAKE_NOW_MS - seedAge;
            Assert.assertFalse("Stamp file already exists", stamp.exists());
            Assert.assertTrue("Failed to create stamp file", stamp.createNewFile());
            Assert.assertTrue("Failed to set stamp time", stamp.setLastModified(stampLastModified));

            AwVariationsSeedFetcher.scheduleIfNeeded(FAKE_NOW_MS);

            mScheduler.assertScheduledWithDelayEqualTo(TimeUnit.HOURS.toMillis(17));
        } finally {
            mScheduler.clear();
            VariationsTestUtils.deleteSeeds(); // Remove the stamp file.
        }
    }

    // Pretend that a job is already scheduled. Then test scheduleIfNeeded(), which should not
    // schedule a job.
    @Test
    @SmallTest
    public void testScheduleAlreadyScheduled() {
        File stamp = VariationsUtils.getStampFile();
        try {
            @SuppressLint("JobSchedulerService")
            ComponentName component = new ComponentName(
                    ContextUtils.getApplicationContext(), AwVariationsSeedFetcher.class);
            JobInfo job = new JobInfo.Builder(AwVariationsSeedFetcher.JOB_ID, component)
                                  .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                                  .setRequiresCharging(true)
                                  .build();
            mScheduler.schedule(job);
            AwVariationsSeedFetcher.scheduleIfNeeded(FAKE_NOW_MS);
            // Check that our job object hasn't been replaced (meaning that scheduleIfNeeded didn't
            // schedule a job).
            Assert.assertSame(job, mScheduler.getPendingJob(AwVariationsSeedFetcher.JOB_ID));
        } finally {
            mScheduler.clear();
        }
    }

    @Test
    @SmallTest
    public void testFetch() throws IOException, TimeoutException {
        try {
            AwVariationsSeedFetcher fetcher = new AwVariationsSeedFetcher() {
                // p is null in this test. Don't actually call JobService.jobFinished.
                @Override
                protected void jobFinished(JobParameters p) {}
            };
            int downloads = mDownloader.helper.getCallCount();
            fetcher.onStartJob(null);
            mDownloader.helper.waitForCallback(
                    "Timeout out waiting for AwVariationsSeedFetcher to call downloadContent",
                    downloads);
            File stamp = VariationsUtils.getStampFile();
            Assert.assertTrue("AwVariationsSeedFetcher should have updated stamp file " + stamp,
                    stamp.exists());
        } finally {
            VariationsTestUtils.deleteSeeds(); // Remove the stamp file.
        }
    }
}
