// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.app.job.JobWorkItem;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

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
import org.chromium.components.minidump_uploader.CrashTestRule.MockCrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.MinidumpUploadCallable;
import org.chromium.components.minidump_uploader.MinidumpUploadCallable.MinidumpUploadStatus;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.NetworkChangeNotifier;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Testcase for {@link MinidumpUploadService}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class MinidumpUploadServiceTest {
    @Rule
    public CrashTestRule mTestRule = new CrashTestRule();

    private static final int CHECK_INTERVAL_MS = 250;
    private static final int MAX_TIMEOUT_MS = 20000;
    private static final String BOUNDARY = "TESTBOUNDARY";

    private static class TestMinidumpUploadService extends MinidumpUploadService {
        private final NetworkChangingPermissionManager mPermissionManager =
                new NetworkChangingPermissionManager();
        private TestMinidumpUploadService() {}
        private TestMinidumpUploadService(Context context) {
            attachBaseContext(context);
        }

        private void attachBaseContextLate(Context base) {
            super.attachBaseContext(base);
        }

        private static class NetworkChangingPermissionManager
                extends MockCrashReportingPermissionManager {
            @Override
            public boolean isNetworkAvailableForCrashUploads() {
                return mIsNetworkAvailable;
            }

            public void setIsNetworkAvailableForCrashUploads(boolean networkAvailable) {
                mIsNetworkAvailable = networkAvailable;
            }
        }

        @Override
        CrashReportingPermissionManager getCrashReportingPermissionManager() {
            return mPermissionManager;
        }

        public void setIsNetworkAvailableForCrashUploads(boolean networkAvailable) {
            mPermissionManager.setIsNetworkAvailableForCrashUploads(networkAvailable);
        }
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testTryUploadAllCrashDumps() throws IOException {
        // The JobScheduler API is used on Android M+.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) return;

        // Setup prerequisites.
        final AtomicInteger numServiceStarts = new AtomicInteger(0);
        final File[] minidumpFiles = {
                new File(mTestRule.getCrashDir(), "chromium_renderer-111.dmp1.try0"),
                new File(mTestRule.getCrashDir(), "chromium_renderer-222.dmp2.try1"),
                new File(mTestRule.getCrashDir(), "chromium_renderer-333.dmp3.try2"),
        };
        final File[] invalidMinidumpFiles = {
                // The ".try" suffix is required.
                new File(mTestRule.getCrashDir(), "chromium_renderer-111.dmp4"),
                // The minidump should not have exceeded the maximum number of tries.
                new File(mTestRule.getCrashDir(), "chromium_renderer-222.dmp5.try3"),
        };
        MinidumpPreparationContext context = new MinidumpPreparationContext(
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext()) {
            @Override
            public ComponentName startService(Intent intentToCheck) {
                String filePath =
                        intentToCheck.getStringExtra(MinidumpUploadService.FILE_TO_UPLOAD_KEY);
                // Assuming numServicesStart value corresponds to minidumpFiles index.
                Assert.assertEquals("Action should be correct", MinidumpUploadService.ACTION_UPLOAD,
                        intentToCheck.getAction());
                Assert.assertTrue("Should not call service more than number of files",
                        numServiceStarts.incrementAndGet() <= minidumpFiles.length);
                Assert.assertEquals("Minidump path should be the absolute path",
                        minidumpFiles[numServiceStarts.intValue() - 1].getAbsolutePath(), filePath);
                return new ComponentName(getPackageName(), MinidumpUploadService.class.getName());
            }

        };
        MinidumpUploadService service = new TestMinidumpUploadService(context);
        for (File minidumpFile : minidumpFiles) {
            CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY);
        }
        for (File minidumpFile : invalidMinidumpFiles) {
            CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY);
        }

        // Run test.
        service.onCreate();
        ContextUtils.initApplicationContextForTests(context);
        MinidumpUploadService.tryUploadAllCrashDumps();

        // Verify.
        for (File minidumpFile : minidumpFiles) {
            Assert.assertTrue("Minidump file should exist: " + minidumpFile, minidumpFile.isFile());
        }
        Assert.assertEquals(
                "Should have called startService() same number of times as there are files",
                minidumpFiles.length, numServiceStarts.intValue());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadCrash() throws IOException {
        List<CountedMinidumpUploadCallable> callables =
                new ArrayList<CountedMinidumpUploadCallable>();
        callables.add(new CountedMinidumpUploadCallable(
                "chromium_renderer-111.dmp1.try0", MinidumpUploadStatus.SUCCESS, false));
        runUploadCrashTest(callables);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadCrashWithThreeFails() throws IOException {
        // Create |MAX_TRIES_ALLOWED| callables.
        final List<CountedMinidumpUploadCallable> callables =
                new ArrayList<CountedMinidumpUploadCallable>();
        for (int i = 0; i < MinidumpUploadService.MAX_TRIES_ALLOWED; i++) {
            callables.add(new CountedMinidumpUploadCallable(
                    "chromium_renderer-111.dmp1.try" + i, MinidumpUploadStatus.FAILURE, true));
        }
        runUploadCrashTest(callables);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadCrashWithOneFailWithNetwork() throws IOException {
        List<CountedMinidumpUploadCallable> callables =
                new ArrayList<CountedMinidumpUploadCallable>();
        callables.add(new CountedMinidumpUploadCallable(
                "chromium_renderer-111.dmp1.try0", MinidumpUploadStatus.FAILURE, true));
        callables.add(new CountedMinidumpUploadCallable(
                "chromium_renderer-111.dmp1.try1", MinidumpUploadStatus.SUCCESS, true));
        runUploadCrashTest(callables);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadCrashWithOneFailNoNetwork() throws IOException {
        List<CountedMinidumpUploadCallable> callables =
                new ArrayList<CountedMinidumpUploadCallable>();
        callables.add(new CountedMinidumpUploadCallable(
                "chromium_renderer-111.dmp1.try0", MinidumpUploadStatus.FAILURE, false));
        runUploadCrashTest(callables);
    }

    private void runUploadCrashTest(final List<CountedMinidumpUploadCallable> callables)
            throws IOException {
        // The JobScheduler API is used on Android M+.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) return;

        // Setup prerequisites.
        // This version of the service overrides the createMinidumpUploadCallable(...) to be able
        // to return fake ones. It also ensures that the service never tries to create a callable
        // too many times.
        TestMinidumpUploadService service = new TestMinidumpUploadService() {
            int mIndex;
            boolean mTriggerNetworkChange;

            @Override
            MinidumpUploadCallable createMinidumpUploadCallable(File minidumpFile, File logfile) {
                if (mIndex >= callables.size()) {
                    Assert.fail("Should not create callable number " + mIndex);
                }
                CountedMinidumpUploadCallable callable = callables.get(mIndex++);
                if (callable.mTriggerNetworkChange) {
                    mTriggerNetworkChange = true;
                }
                return callable;
            }

            @Override
            protected void onHandleIntent(Intent intent) {
                try {
                    InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
                        @Override
                        public void run() {
                            // Set up basically a fake.
                            if (!NetworkChangeNotifier.isInitialized()) {
                                NetworkChangeNotifier.init();
                            }
                        }
                    });
                } catch (Throwable t) {
                    t.printStackTrace();
                    Assert.fail("Failed to set up NetworkChangeNotifier");
                }

                super.onHandleIntent(intent);

                if (mTriggerNetworkChange) {
                    mTriggerNetworkChange = false;
                    try {
                        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
                            @Override
                            public void run() {
                                NetworkChangeNotifier.setAutoDetectConnectivityState(false);
                                // Quickly force the state to be connected and back to disconnected.
                                // An event should be triggered for retry logics.
                                setIsNetworkAvailableForCrashUploads(false);
                                NetworkChangeNotifier.forceConnectivityState(false);
                                setIsNetworkAvailableForCrashUploads(true);
                                NetworkChangeNotifier.forceConnectivityState(true);
                            }
                        });
                    } catch (Throwable t) {
                        t.printStackTrace();
                        Assert.fail("Failed to trigger NetworkChangeNotifier");
                    }
                }
            }
        };
        // Create a context that supports call to startService(...), where it runs the new service
        // calls on a handler thread. We pass in the MinidumpUploadService as an argument so we
        // can call it directly without going through the Android framework.
        final MinidumpPreparationContext context = new MinidumpPreparationContext(
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext(),
                service) {
            Handler mHandler;
            {
                HandlerThread handlerThread =
                        new HandlerThread("MinidumpUploadServiceTest Handler Thread");
                handlerThread.start();
                mHandler = new Handler(handlerThread.getLooper());
            }

            @Override
            public ComponentName startService(final Intent intentToCheck) {
                Assert.assertTrue(
                        MinidumpUploadService.ACTION_UPLOAD.equals(intentToCheck.getAction()));
                // Post to the handler thread to run the retry intent.
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        mService.onHandleIntent(intentToCheck);
                    }
                });
                return new ComponentName(getPackageName(), MinidumpUploadService.class.getName());
            }

        };
        // We need the context before we can attach it to the service, so since Context is
        // dependent on the service, we do this after context creation.
        service.attachBaseContextLate(context);
        // Create the file used for uploading.
        File minidumpFile = new File(mTestRule.getCrashDir(), "chromium_renderer-111.dmp1.try0");
        minidumpFile.createNewFile();
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY);

        // Run test.
        service.onCreate();
        ContextUtils.initApplicationContextForTests(context);
        MinidumpUploadService.tryUploadCrashDump(minidumpFile);

        // Verify asynchronously.
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("All callables should have a call-count of 1") {
                    @Override
                    public boolean isSatisfied() {
                        for (CountedMinidumpUploadCallable callable : callables) {
                            if (callable.mCalledCount != 1) {
                                return false;
                            }
                        }
                        return true;
                    }
                },
                MAX_TIMEOUT_MS, CHECK_INTERVAL_MS);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testHandleForceUploadCrash_MinidumpFileExists_SansJobScheduler()
            throws IOException {
        // The JobScheduler API is used on Android M+.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) return;

        // Set up prerequisites.
        File minidumpFile = new File(
                mTestRule.getCrashDir(), "chromium-renderer-minidump-f297dbcba7a2d0bb.dmp0.try3");
        final File expectedRenamedMinidumpFile = new File(mTestRule.getCrashDir(),
                "chromium-renderer-minidump-f297dbcba7a2d0bb.forced0.try0");
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY);
        final String startServiceFlag = "startServiceFlag";
        MinidumpPreparationContext context = new MinidumpPreparationContext(
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext()) {
            @Override
            public ComponentName startService(Intent intentToCheck) {
                Assert.assertEquals(MinidumpUploadService.ACTION_UPLOAD, intentToCheck.getAction());
                String filePath =
                        intentToCheck.getStringExtra(MinidumpUploadService.FILE_TO_UPLOAD_KEY);
                Assert.assertEquals("Minidump path should be for a fresh upload",
                        expectedRenamedMinidumpFile.getAbsolutePath(), filePath);
                setFlag(startServiceFlag);
                return new ComponentName(getPackageName(), MinidumpUploadService.class.getName());
            }
        };

        // Run test.
        ContextUtils.initApplicationContextForTests(context);
        MinidumpUploadService.tryUploadCrashDumpWithLocalId("f297dbcba7a2d0bb");

        // Verify.
        Assert.assertTrue(
                "Should have called startService(...)", context.isFlagSet(startServiceFlag));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testHandleForceUploadCrash_MinidumpFileExists_WithJobScheduler()
            throws IOException {
        // The JobScheduler API is only available as of Android M+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;

        // Set up prerequisites.
        CrashTestRule.setUpMinidumpFile(
                new File(mTestRule.getCrashDir(),
                        "chromium-renderer-minidump-f297dbcba7a2d0bb.dmp0.try3"),
                BOUNDARY);
        AdvancedMockContext context =
                new MinidumpPreparationContext(InstrumentationRegistry.getInstrumentation()
                                                       .getTargetContext()
                                                       .getApplicationContext());

        // Run test.
        ContextUtils.initApplicationContextForTests(context);
        MinidumpUploadService.tryUploadCrashDumpWithLocalId("f297dbcba7a2d0bb");

        // Verify.
        final File expectedRenamedMinidumpFile = new File(mTestRule.getCrashDir(),
                "chromium-renderer-minidump-f297dbcba7a2d0bb.forced0.try0");
        Assert.assertTrue("Should have renamed the minidump file for forced upload",
                expectedRenamedMinidumpFile.exists());
        Assert.assertTrue("Should have tried to schedule an upload job",
                context.isFlagSet(TestJobScheduler.SCHEDULE_JOB_FLAG));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testHandleForceUploadCrash_SkippedMinidumpFileExists_SansJobScheduler()
            throws IOException {
        // The JobScheduler API is used on Android M+.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) return;

        // Set up prerequisites.
        File minidumpFile = new File(mTestRule.getCrashDir(),
                "chromium-renderer-minidump-f297dbcba7a2d0bb.skipped0.try0");
        final File expectedRenamedMinidumpFile = new File(mTestRule.getCrashDir(),
                "chromium-renderer-minidump-f297dbcba7a2d0bb.forced0.try0");
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY);
        final String startServiceFlag = "startServiceFlag";
        MinidumpPreparationContext context = new MinidumpPreparationContext(
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext()) {
            @Override
            public ComponentName startService(Intent intentToCheck) {
                Assert.assertEquals(MinidumpUploadService.ACTION_UPLOAD, intentToCheck.getAction());
                String filePath =
                        intentToCheck.getStringExtra(MinidumpUploadService.FILE_TO_UPLOAD_KEY);
                Assert.assertEquals("Minidump path should be for a fresh upload",
                        expectedRenamedMinidumpFile.getAbsolutePath(), filePath);
                setFlag(startServiceFlag);
                return new ComponentName(getPackageName(), MinidumpUploadService.class.getName());
            }
        };

        // Run test.
        ContextUtils.initApplicationContextForTests(context);
        MinidumpUploadService.tryUploadCrashDumpWithLocalId("f297dbcba7a2d0bb");

        // Verify.
        Assert.assertTrue(
                "Should have called startService(...)", context.isFlagSet(startServiceFlag));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testHandleForceUploadCrash_SkippedMinidumpFileExists_WithJobScheduler()
            throws IOException {
        // The JobScheduler API is only available as of Android M.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;

        // Set up prerequisites.
        CrashTestRule.setUpMinidumpFile(
                new File(mTestRule.getCrashDir(),
                        "chromium-renderer-minidump-f297dbcba7a2d0bb.skipped0.try3"),
                BOUNDARY);
        AdvancedMockContext context =
                new MinidumpPreparationContext(InstrumentationRegistry.getInstrumentation()
                                                       .getTargetContext()
                                                       .getApplicationContext());

        // Run test.
        ContextUtils.initApplicationContextForTests(context);
        MinidumpUploadService.tryUploadCrashDumpWithLocalId("f297dbcba7a2d0bb");

        // Verify.
        final File expectedRenamedMinidumpFile = new File(mTestRule.getCrashDir(),
                "chromium-renderer-minidump-f297dbcba7a2d0bb.forced0.try0");
        Assert.assertTrue("Should have renamed the minidump file for forced upload",
                expectedRenamedMinidumpFile.exists());
        Assert.assertTrue("Should have tried to schedule an upload job",
                context.isFlagSet(TestJobScheduler.SCHEDULE_JOB_FLAG));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testHandleForceUploadCrash_FileDoesntExist_SansJobScheduler() {
        // The JobScheduler API is used on Android M+.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) return;

        // Set up prerequisites.
        final String startServiceFlag = "startServiceFlag";
        MinidumpPreparationContext context = new MinidumpPreparationContext(
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext()) {
            @Override
            public ComponentName startService(Intent unused) {
                setFlag(startServiceFlag);
                return new ComponentName(getPackageName(), MinidumpUploadService.class.getName());
            }
        };

        // Run test.
        MinidumpUploadService.tryUploadCrashDumpWithLocalId("f297dbcba7a2d0bb");

        // Verify.
        Assert.assertFalse(
                "Should not have called startService(...)", context.isFlagSet(startServiceFlag));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testHandleForceUploadCrash_FileDoesntExist_WithJobScheduler() {
        // The JobScheduler API is only available as of Android M.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;

        // Set up prerequisites.
        AdvancedMockContext context =
                new MinidumpPreparationContext(InstrumentationRegistry.getInstrumentation()
                                                       .getTargetContext()
                                                       .getApplicationContext());

        // Run test.
        MinidumpUploadService.tryUploadCrashDumpWithLocalId("f297dbcba7a2d0bb");

        // Verify.
        Assert.assertFalse("Should not have tried to schedule an upload job",
                context.isFlagSet(TestJobScheduler.SCHEDULE_JOB_FLAG));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testHandleForceUploadCrash_FileAlreadyUploaded_SansJobScheduler()
            throws IOException {
        // The JobScheduler API is used on Android M+.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) return;

        // Set up prerequisites.
        CrashTestRule.setUpMinidumpFile(
                new File(mTestRule.getCrashDir(),
                        "chromium-renderer-minidump-f297dbcba7a2d0bb.up0.try0"),
                BOUNDARY);
        final String startServiceFlag = "startServiceFlag";
        MinidumpPreparationContext context = new MinidumpPreparationContext(
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext()) {
            @Override
            public ComponentName startService(Intent unused) {
                setFlag(startServiceFlag);
                return new ComponentName(getPackageName(), MinidumpUploadService.class.getName());
            }
        };

        // Run test.
        MinidumpUploadService.tryUploadCrashDumpWithLocalId("f297dbcba7a2d0bb");

        // Verify.
        Assert.assertFalse(
                "Should not have called startService(...)", context.isFlagSet(startServiceFlag));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testHandleForceUploadCrash_FileAlreadyUploaded_WithJobScheduler()
            throws IOException {
        // The JobScheduler API is only available as of Android M.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;

        // Set up prerequisites.
        CrashTestRule.setUpMinidumpFile(
                new File(mTestRule.getCrashDir(),
                        "chromium-renderer-minidump-f297dbcba7a2d0bb.up0.try0"),
                BOUNDARY);
        AdvancedMockContext context =
                new MinidumpPreparationContext(InstrumentationRegistry.getInstrumentation()
                                                       .getTargetContext()
                                                       .getApplicationContext());

        // Run test.
        MinidumpUploadService.tryUploadCrashDumpWithLocalId("f297dbcba7a2d0bb");

        // Verify.
        Assert.assertFalse("Should not have tried to schedule an upload job",
                context.isFlagSet(TestJobScheduler.SCHEDULE_JOB_FLAG));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetCrashType1() throws IOException {
        final File minidumpFile =
                new File(mTestRule.getCrashDir(), "chromium_renderer-123.dmp.try0");
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY, "browser");
        Assert.assertEquals(MinidumpUploadService.ProcessType.BROWSER,
                MinidumpUploadService.getCrashType(minidumpFile.getAbsolutePath()));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetCrashType2() throws IOException {
        final File minidumpFile =
                new File(mTestRule.getCrashDir(), "chromium_renderer-123.dmp.try0");
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY, "renderer");
        Assert.assertEquals(MinidumpUploadService.ProcessType.RENDERER,
                MinidumpUploadService.getCrashType(minidumpFile.getAbsolutePath()));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetCrashType3() throws IOException {
        final File minidumpFile =
                new File(mTestRule.getCrashDir(), "chromium_renderer-123.dmp.try0");
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY, "gpu-process");
        Assert.assertEquals(MinidumpUploadService.ProcessType.GPU,
                MinidumpUploadService.getCrashType(minidumpFile.getAbsolutePath()));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetCrashType4() throws IOException {
        final File minidumpFile =
                new File(mTestRule.getCrashDir(), "chromium_renderer-123.dmp.try0");
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY, "weird test type");
        Assert.assertEquals(MinidumpUploadService.ProcessType.OTHER,
                MinidumpUploadService.getCrashType(minidumpFile.getAbsolutePath()));
    }

    private class MinidumpPreparationContext extends AdvancedMockContext {
        /**
         * Field used in overridden versions of startService() so we can support retries.
         */
        protected MinidumpUploadService mService;

        public MinidumpPreparationContext(Context targetContext) {
            this(targetContext, null);
        }

        public MinidumpPreparationContext(Context targetContext, MinidumpUploadService service) {
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

    /**
     * A JobScheduler wrapper that verifies that the expected properties are set correctly.
     */
    @TargetApi(Build.VERSION_CODES.M)
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
            Assert.assertEquals(ChromeMinidumpUploadJobService.class.getName(),
                    job.getService().getClassName());
            return JobScheduler.RESULT_SUCCESS;
        }
    };

    /**
     * A fake callable, that just counts the number of times it is called.
     *
     * It can be constructed with the wanted return-value of the call()-method.
     */
    private static class CountedMinidumpUploadCallable extends MinidumpUploadCallable {
        private int mCalledCount;
        @MinidumpUploadCallable.MinidumpUploadStatus private final int mResult;
        private final boolean mTriggerNetworkChange;

        /**
         * Creates a fake callable, that just counts the number of times it is called.
         *
         * @param result the value to return from the call()-method.
         * @param networkChange Should trigger a network change after this callable is finished.
         *     This essentially triggers a retry if result is set to fail.
         */
        private CountedMinidumpUploadCallable(String fileName, int result, boolean networkChange) {
            super(new File(fileName), null, null, null);
            this.mResult = result;
            this.mTriggerNetworkChange = networkChange;
        }

        @Override
        public Integer call() {
            ++mCalledCount;
            return mResult;
        }
    }
}
