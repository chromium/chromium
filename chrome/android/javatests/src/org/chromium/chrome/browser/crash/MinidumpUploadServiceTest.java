// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.annotation.SuppressLint;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.app.job.JobWorkItem;
import android.content.Context;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.minidump_uploader.CrashTestRule;

import java.io.File;
import java.io.IOException;
import java.util.List;

/** Testcase for {@link MinidumpUploadService}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class MinidumpUploadServiceTest {
    @Rule public CrashTestRule mTestRule = new CrashTestRule();

    private static final String BOUNDARY = "TESTBOUNDARY";

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testHandleForceUploadCrash_MinidumpFileExists_WithJobScheduler()
            throws IOException {
        // Set up prerequisites.
        CrashTestRule.setUpMinidumpFile(
                new File(
                        mTestRule.getCrashDir(),
                        "chromium-renderer-minidump-f297dbcba7a2d0bb.dmp0.try3"),
                BOUNDARY);
        AdvancedMockContext context =
                new MinidumpPreparationContext(
                        InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getApplicationContext());

        // Run test.
        ContextUtils.initApplicationContextForTests(context);
        MinidumpUploadServiceImpl.tryUploadCrashDumpWithLocalId("f297dbcba7a2d0bb");

        // Verify.
        final File expectedRenamedMinidumpFile =
                new File(
                        mTestRule.getCrashDir(),
                        "chromium-renderer-minidump-f297dbcba7a2d0bb.forced0.try0");
        Assert.assertTrue(
                "Should have renamed the minidump file for forced upload",
                expectedRenamedMinidumpFile.exists());
        Assert.assertTrue(
                "Should have tried to schedule an upload job",
                context.isFlagSet(TestJobScheduler.SCHEDULE_JOB_FLAG));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testHandleForceUploadCrash_SkippedMinidumpFileExists_WithJobScheduler()
            throws IOException {
        // Set up prerequisites.
        CrashTestRule.setUpMinidumpFile(
                new File(
                        mTestRule.getCrashDir(),
                        "chromium-renderer-minidump-f297dbcba7a2d0bb.skipped0.try3"),
                BOUNDARY);
        AdvancedMockContext context =
                new MinidumpPreparationContext(
                        InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getApplicationContext());

        // Run test.
        ContextUtils.initApplicationContextForTests(context);
        MinidumpUploadServiceImpl.tryUploadCrashDumpWithLocalId("f297dbcba7a2d0bb");

        // Verify.
        final File expectedRenamedMinidumpFile =
                new File(
                        mTestRule.getCrashDir(),
                        "chromium-renderer-minidump-f297dbcba7a2d0bb.forced0.try0");
        Assert.assertTrue(
                "Should have renamed the minidump file for forced upload",
                expectedRenamedMinidumpFile.exists());
        Assert.assertTrue(
                "Should have tried to schedule an upload job",
                context.isFlagSet(TestJobScheduler.SCHEDULE_JOB_FLAG));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testHandleForceUploadCrash_FileDoesntExist_WithJobScheduler() {
        // Set up prerequisites.
        AdvancedMockContext context =
                new MinidumpPreparationContext(
                        InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getApplicationContext());

        // Run test.
        MinidumpUploadServiceImpl.tryUploadCrashDumpWithLocalId("f297dbcba7a2d0bb");

        // Verify.
        Assert.assertFalse(
                "Should not have tried to schedule an upload job",
                context.isFlagSet(TestJobScheduler.SCHEDULE_JOB_FLAG));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testHandleForceUploadCrash_FileAlreadyUploaded_WithJobScheduler()
            throws IOException {
        // Set up prerequisites.
        CrashTestRule.setUpMinidumpFile(
                new File(
                        mTestRule.getCrashDir(),
                        "chromium-renderer-minidump-f297dbcba7a2d0bb.up0.try0"),
                BOUNDARY);
        AdvancedMockContext context =
                new MinidumpPreparationContext(
                        InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getApplicationContext());

        // Run test.
        MinidumpUploadServiceImpl.tryUploadCrashDumpWithLocalId("f297dbcba7a2d0bb");

        // Verify.
        Assert.assertFalse(
                "Should not have tried to schedule an upload job",
                context.isFlagSet(TestJobScheduler.SCHEDULE_JOB_FLAG));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetCrashType1() throws IOException {
        final File minidumpFile =
                new File(mTestRule.getCrashDir(), "chromium_renderer-123.dmp.try0");
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY, "browser");
        Assert.assertEquals(
                MinidumpUploadServiceImpl.ProcessType.BROWSER,
                MinidumpUploadServiceImpl.getCrashType(minidumpFile.getAbsolutePath()));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetCrashType2() throws IOException {
        final File minidumpFile =
                new File(mTestRule.getCrashDir(), "chromium_renderer-123.dmp.try0");
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY, "renderer");
        Assert.assertEquals(
                MinidumpUploadServiceImpl.ProcessType.RENDERER,
                MinidumpUploadServiceImpl.getCrashType(minidumpFile.getAbsolutePath()));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetCrashType3() throws IOException {
        final File minidumpFile =
                new File(mTestRule.getCrashDir(), "chromium_renderer-123.dmp.try0");
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY, "gpu-process");
        Assert.assertEquals(
                MinidumpUploadServiceImpl.ProcessType.GPU,
                MinidumpUploadServiceImpl.getCrashType(minidumpFile.getAbsolutePath()));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetCrashType4() throws IOException {
        final File minidumpFile =
                new File(mTestRule.getCrashDir(), "chromium_renderer-123.dmp.try0");
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY, "weird test type");
        Assert.assertEquals(
                MinidumpUploadServiceImpl.ProcessType.OTHER,
                MinidumpUploadServiceImpl.getCrashType(minidumpFile.getAbsolutePath()));
    }

    private static class MinidumpPreparationContext extends AdvancedMockContext {
        /** Field used in overridden versions of startService() so we can support retries. */
        protected MinidumpUploadServiceImpl mService;

        public MinidumpPreparationContext(Context targetContext) {
            this(targetContext, null);
        }

        public MinidumpPreparationContext(
                Context targetContext, MinidumpUploadServiceImpl service) {
            super(targetContext);
            mService = service;
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.JOB_SCHEDULER_SERVICE.equals(name)) {
                return new TestJobScheduler(this);
            }

            return super.getSystemService(name);
        }
    }

    /** A JobScheduler wrapper that verifies that the expected properties are set correctly. */
    private static class TestJobScheduler extends JobScheduler {
        static final String SCHEDULE_JOB_FLAG = "scheduleJobFlag";

        private final AdvancedMockContext mContext;

        TestJobScheduler(AdvancedMockContext context) {
            mContext = context;
        }

        @Override
        public void cancel(int jobId) {}

        @Override
        public void cancelAll() {}

        @Override
        @SuppressLint("WrongConstant")
        public int enqueue(JobInfo job, JobWorkItem work) {
            return 0;
        }

        @Override
        public List<JobInfo> getAllPendingJobs() {
            return null;
        }

        @Override
        public JobInfo getPendingJob(int jobId) {
            return null;
        }

        @Override
        public int schedule(JobInfo job) {
            mContext.setFlag(SCHEDULE_JOB_FLAG);
            Assert.assertEquals(TaskIds.CHROME_MINIDUMP_UPLOADING_JOB_ID, job.getId());
            Assert.assertEquals(
                    ChromeMinidumpUploadJobService.class.getName(),
                    job.getService().getClassName());
            return JobScheduler.RESULT_SUCCESS;
        }
    }
}
