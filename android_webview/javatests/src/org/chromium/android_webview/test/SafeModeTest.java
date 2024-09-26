// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.hamcrest.Matchers.greaterThan;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.app.job.JobWorkItem;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.BrowserSafeModeActionList;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.common.VariationsFastFetchModeUtils;
import org.chromium.android_webview.common.services.ISafeModeService;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.android_webview.services.AwVariationsSeedFetcher;
import org.chromium.android_webview.services.NonEmbeddedFastVariationsSeedSafeModeAction;
import org.chromium.android_webview.services.NonEmbeddedSafeModeAction;
import org.chromium.android_webview.services.NonEmbeddedSafeModeActionsSetupCleanup;
import org.chromium.android_webview.services.SafeModeService;
import org.chromium.android_webview.services.SafeModeService.TrustedPackage;
import org.chromium.android_webview.test.VariationsSeedLoaderTest.TestLoader;
import org.chromium.android_webview.test.VariationsSeedLoaderTest.TestLoaderResult;
import org.chromium.android_webview.test.services.ServiceConnectionHelper;
import org.chromium.android_webview.test.util.VariationsTestUtils;
import org.chromium.android_webview.variations.FastVariationsSeedSafeModeAction;
import org.chromium.android_webview.variations.VariationsSeedSafeModeAction;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.BuildConfig;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/** Test WebView SafeMode. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
public class SafeModeTest extends AwParameterizedTest {
    // The package name of the test shell. This is acting both as the client app and the WebView
    // provider.
    public static final String TEST_WEBVIEW_PACKAGE_NAME = "org.chromium.android_webview.shell";

    // This is the actual certificate hash we use to sign webview_instrumentation_apk (Signer #1
    // certificate SHA-256 digest). This can be obtained by running:
    // $ out/Default/bin/webview_instrumentation_apk print-certs
    private static final byte[] TEST_WEBVIEW_CERT_HASH =
            new byte[] {
                (byte) 0x32,
                (byte) 0xa2,
                (byte) 0xfc,
                (byte) 0x74,
                (byte) 0xd7,
                (byte) 0x31,
                (byte) 0x10,
                (byte) 0x58,
                (byte) 0x59,
                (byte) 0xe5,
                (byte) 0xa8,
                (byte) 0x5d,
                (byte) 0xf1,
                (byte) 0x6d,
                (byte) 0x95,
                (byte) 0xf1,
                (byte) 0x02,
                (byte) 0xd8,
                (byte) 0x5b,
                (byte) 0x22,
                (byte) 0x09,
                (byte) 0x9b,
                (byte) 0x80,
                (byte) 0x64,
                (byte) 0xc5,
                (byte) 0xd8,
                (byte) 0x91,
                (byte) 0x5c,
                (byte) 0x61,
                (byte) 0xda,
                (byte) 0xd1,
                (byte) 0xe0
            };

    // Arbitrary sha256 digest which does not match TEST_WEBVIEW_PACKAGE_NAME's certificate.
    private static final byte[] FAKE_CERT_HASH =
            new byte[] {
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF,
                (byte) 0xFF
            };

    private static final String SAFEMODE_ACTION_NAME = "some_action_name";

    private AtomicInteger mTestSafeModeActionExecutionCounter;

    private TestJobScheduler mScheduler = new TestJobScheduler();
    private TestVariationsSeedFetcher mDownloader = new TestVariationsSeedFetcher();
    private static final int HTTP_NOT_FOUND = 404;
    private static final int HTTP_NOT_MODIFIED = 304;
    private static final int JOB_ID = TaskIds.WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID;

    // A test JobScheduler which only holds one job, and never does anything with it.
    private static class TestJobScheduler extends JobScheduler {
        public JobInfo mJob;
        public QueueContainer mQueueContainer = new QueueContainer();

        public void clear() {
            mJob = null;
        }

        public void assertScheduled() {
            Assert.assertNotNull("No job scheduled", mJob);
        }

        public void assertNotScheduled() {
            Assert.assertNull("Job should not have been scheduled", mJob);
        }

        public void assertScheduled(int jobId) {
            Assert.assertEquals("No job scheduled", mJob.getId(), jobId);
        }

        @Override
        public void cancel(int jobId) {
            if (mJob == null) return;
            if (mJob.getId() == jobId) mJob = null;
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
            Assert.assertTrue("Job scheduled with wrong ID", JOB_ID == job.getId());
            mJob = job;
            mQueueContainer.notifyCalled(job);
            return JobScheduler.RESULT_SUCCESS;
        }

        public JobInfo waitForMessageCallback() throws Exception {
            return mQueueContainer.waitForMessageCallback();
        }
    }

    private static class QueueContainer {
        private LinkedBlockingQueue<JobInfo> mQueue = new LinkedBlockingQueue<>();

        public void notifyCalled(JobInfo job) {
            try {
                mQueue.add(job);
            } catch (IllegalStateException e) {
                // We expect this add operation will always succeed since the default capacity of
                // the queue is Integer.MAX_VALUE.
            }
        }

        public JobInfo waitForMessageCallback() throws Exception {
            return AwActivityTestRule.waitForNextQueueElement(mQueue);
        }

        public boolean isQueueEmpty() {
            return mQueue.isEmpty();
        }
    }

    // A test VariationsSeedFetcher which doesn't actually download seeds, but verifies the request
    // parameters.
    private static class TestVariationsSeedFetcher extends VariationsSeedFetcher {
        private static final String SAVED_VARIATIONS_SEED_SERIAL_NUMBER = "savedSerialNumber";

        public int fetchResult;

        @Override
        public SeedFetchInfo downloadContent(
                VariationsSeedFetcher.SeedFetchParameters params, SeedInfo currInfo) {
            Assert.assertEquals(
                    VariationsSeedFetcher.VariationsPlatform.ANDROID_WEBVIEW, params.getPlatform());
            Assert.assertThat(Integer.parseInt(params.getMilestone()), greaterThan(0));

            SeedFetchInfo fetchInfo = new SeedFetchInfo();
            // Pretend the servers-side |serialNumber| equals |SAVED_VARIATIONS_SEED_SERIAL_NUMBER|
            // and return |HTTP_NOT_MODIFIED|
            if (currInfo != null
                    && currInfo.getParsedVariationsSeed()
                            .getSerialNumber()
                            .equals(SAVED_VARIATIONS_SEED_SERIAL_NUMBER)) {
                fetchInfo.seedInfo = currInfo;
                fetchInfo.seedInfo.date = getDateTime().newDate().getTime();
                fetchInfo.seedFetchResult = HTTP_NOT_MODIFIED;
            } else {
                fetchInfo.seedFetchResult = fetchResult;
            }
            return fetchInfo;
        }
    }

    @Rule public AwActivityTestRule mActivityTestRule;

    public SafeModeTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Throwable {
        mTestSafeModeActionExecutionCounter = new AtomicInteger(0);
        AwVariationsSeedFetcher.setMocks(mScheduler, mDownloader);
        VariationsTestUtils.deleteSeeds();
    }

    @After
    public void tearDown() throws Throwable {
        // Reset component state back to the default.
        final Context context = ContextUtils.getApplicationContext();
        ComponentName safeModeComponent =
                new ComponentName(
                        TEST_WEBVIEW_PACKAGE_NAME, SafeModeController.SAFE_MODE_STATE_COMPONENT);
        context.getPackageManager()
                .setComponentEnabledSetting(
                        safeModeComponent,
                        PackageManager.COMPONENT_ENABLED_STATE_DEFAULT,
                        PackageManager.DONT_KILL_APP);

        SafeModeController.getInstance().unregisterActionsForTesting();

        SafeModeService.clearSharedPrefsForTesting();
        mScheduler.cancel(JOB_ID);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSafeModeState_disabledByDefault() throws Throwable {
        Assert.assertFalse(
                "SafeMode should be off by default",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSafeModeState_readComponentState() throws Throwable {
        // Enable the component directly.
        final Context context = ContextUtils.getApplicationContext();
        ComponentName safeModeComponent =
                new ComponentName(
                        TEST_WEBVIEW_PACKAGE_NAME, SafeModeController.SAFE_MODE_STATE_COMPONENT);
        context.getPackageManager()
                .setComponentEnabledSetting(
                        safeModeComponent,
                        PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                        PackageManager.DONT_KILL_APP);

        Assert.assertTrue(
                "SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSafeModeState_enableWithMethod() throws Throwable {
        SafeModeService.setSafeMode(Arrays.asList(SAFEMODE_ACTION_NAME));
        Assert.assertTrue(
                "SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSafeModeState_disableWithMethod() throws Throwable {
        SafeModeService.setSafeMode(Arrays.asList(SAFEMODE_ACTION_NAME));
        Assert.assertTrue(
                "SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));

        SafeModeService.setSafeMode(Arrays.asList());
        Assert.assertFalse(
                "SafeMode should be re-disabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSafeModeState_enableWithService() throws Throwable {
        setSafeMode(Arrays.asList(SAFEMODE_ACTION_NAME));

        Assert.assertTrue(
                "SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSafeModeState_disableWithService() throws Throwable {
        setSafeMode(Arrays.asList(SAFEMODE_ACTION_NAME));

        Assert.assertTrue(
                "SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));

        setSafeMode(Arrays.asList());

        Assert.assertFalse(
                "SafeMode should be re-disabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_disabled() throws Throwable {
        Assert.assertEquals(
                "Querying the ContentProvider should yield empty set when SafeMode is disabled",
                asSet(),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_singleAction() throws Throwable {
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        setSafeMode(Arrays.asList(variationsActionId));

        Assert.assertTrue(
                "SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals(
                "Querying the ContentProvider should yield the action we set",
                asSet(variationsActionId),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_multipleActions() throws Throwable {
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        setSafeMode(Arrays.asList(SAFEMODE_ACTION_NAME, variationsActionId));

        Assert.assertTrue(
                "SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals(
                "Querying the ContentProvider should yield the action we set",
                asSet(SAFEMODE_ACTION_NAME, variationsActionId),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_autoDisableAfter30Days() throws Throwable {
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        final long initialStartTimeMs = 12345L;
        SafeModeService.setClockForTesting(
                () -> {
                    return initialStartTimeMs;
                });
        setSafeMode(Arrays.asList(variationsActionId));

        final long beforeTimeLimitMs =
                initialStartTimeMs + SafeModeService.SAFE_MODE_ENABLED_TIME_LIMIT_MS - 1L;
        SafeModeService.setClockForTesting(
                () -> {
                    return beforeTimeLimitMs;
                });

        Assert.assertTrue(
                "SafeMode should be enabled (before timeout)",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals(
                "Querying the ContentProvider should yield the action we set",
                asSet(variationsActionId),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));

        final long afterTimeLimitMs =
                initialStartTimeMs + SafeModeService.SAFE_MODE_ENABLED_TIME_LIMIT_MS;
        SafeModeService.setClockForTesting(
                () -> {
                    return afterTimeLimitMs;
                });

        Assert.assertTrue(
                "SafeMode should be enabled until querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals(
                "ContentProvider should return empty set after timeout",
                asSet(),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertFalse(
                "SafeMode should be disabled after querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_autoDisableIfTimestampInFuture() throws Throwable {
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        final long initialStartTimeMs = 12345L;
        SafeModeService.setClockForTesting(
                () -> {
                    return initialStartTimeMs;
                });
        setSafeMode(Arrays.asList(variationsActionId));

        // If the user manually sets their clock backward in time, then the time delta will be
        // negative. This case should also be treated as expired.
        final long queryTime = initialStartTimeMs - 1L;
        SafeModeService.setClockForTesting(
                () -> {
                    return queryTime;
                });

        Assert.assertTrue(
                "SafeMode should be enabled until querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals(
                "ContentProvider should return empty set after timeout",
                asSet(),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertFalse(
                "SafeMode should be disabled after querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_extendTimeoutWithDuplicateConfig() throws Throwable {
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        final long initialStartTimeMs = 12345L;
        SafeModeService.setClockForTesting(
                () -> {
                    return initialStartTimeMs;
                });
        setSafeMode(Arrays.asList(variationsActionId));

        // Send a duplicate config after 1 day to extend the SafeMode timeout for another 30 days.
        final long duplicateConfigTimeMs = initialStartTimeMs + TimeUnit.DAYS.toMillis(1);
        SafeModeService.setClockForTesting(
                () -> {
                    return duplicateConfigTimeMs;
                });
        setSafeMode(Arrays.asList(variationsActionId));

        // 30 days after the original timeout
        final long firstTimeLimitMs =
                initialStartTimeMs + SafeModeService.SAFE_MODE_ENABLED_TIME_LIMIT_MS;
        SafeModeService.setClockForTesting(
                () -> {
                    return firstTimeLimitMs;
                });

        Assert.assertEquals(
                "Querying the ContentProvider should yield the action we set (timeout extended)",
                asSet(variationsActionId),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));

        final long secondTimeLimitMs =
                duplicateConfigTimeMs + SafeModeService.SAFE_MODE_ENABLED_TIME_LIMIT_MS;
        SafeModeService.setClockForTesting(
                () -> {
                    return secondTimeLimitMs;
                });

        Assert.assertEquals(
                "ContentProvider should return empty set after timeout",
                asSet(),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_autoDisableIfMissingTimestamp() throws Throwable {
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        setSafeMode(Arrays.asList(variationsActionId));

        // If for some reason LAST_MODIFIED_TIME_KEY is unexpectedly missing, SafeMode should
        // disable itself.
        SafeModeService.removeSharedPrefKeyForTesting(SafeModeService.LAST_MODIFIED_TIME_KEY);

        Assert.assertTrue(
                "SafeMode should be enabled until querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals(
                "ContentProvider should return empty set after timeout",
                asSet(),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertFalse(
                "SafeMode should be disabled after querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_autoDisableIfMissingActions() throws Throwable {
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        setSafeMode(Arrays.asList(variationsActionId));

        // If for some reason SAFEMODE_ACTIONS_KEY is unexpectedly missing (or the empty set),
        // SafeMode should disable itself.
        SafeModeService.removeSharedPrefKeyForTesting(SafeModeService.SAFEMODE_ACTIONS_KEY);

        Assert.assertTrue(
                "SafeMode should be enabled until querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals(
                "ContentProvider should return empty set after timeout",
                asSet(),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertFalse(
                "SafeMode should be disabled after querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_emptyAction() throws Throwable {
        final String invalidWebViewPackageName = "org.chromium.android_webview.test";

        Assert.assertFalse(
                "SafeMode should be disabled",
                SafeModeController.getInstance().isSafeModeEnabled(invalidWebViewPackageName));
        Assert.assertEquals(
                "ContentProvider should return empty set when cursor is null",
                asSet(),
                SafeModeController.getInstance().queryActions(invalidWebViewPackageName));
    }

    private class TestSafeModeAction implements SafeModeAction {
        private int mCallCount;
        private int mExecutionOrder;
        private final String mId;
        private final boolean mSuccess;

        TestSafeModeAction(String id) {
            this(id, true);
        }

        TestSafeModeAction(String id, boolean success) {
            mId = id;
            mSuccess = success;
        }

        @Override
        @NonNull
        public String getId() {
            return mId;
        }

        @Override
        public boolean execute() {
            mCallCount++;
            mExecutionOrder = mTestSafeModeActionExecutionCounter.incrementAndGet();
            return mSuccess;
        }

        public int getCallCount() {
            return mCallCount;
        }

        public int getExecutionOrder() {
            return mExecutionOrder;
        }
    }

    private static class TestNonEmbeddedSafeModeAction implements NonEmbeddedSafeModeAction {
        private int mActivatedCount;
        private int mDeactivatedCount;
        private final String mId;
        private final boolean mSuccess;

        TestNonEmbeddedSafeModeAction(String id) {
            this(id, true);
        }

        TestNonEmbeddedSafeModeAction(String id, boolean success) {
            mId = id;
            mSuccess = success;
        }

        @Override
        @NonNull
        public String getId() {
            return mId;
        }

        @Override
        public boolean onActivate() {
            mActivatedCount++;
            return mSuccess;
        }

        @Override
        public boolean onDeactivate() {
            mDeactivatedCount++;
            return mSuccess;
        }

        public int getActivatedCallCount() {
            return mActivatedCount;
        }

        public int getDeactivatedCallCount() {
            return mDeactivatedCount;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_cannotRegisterActionsTwice() throws Throwable {
        TestSafeModeAction testAction1 = new TestSafeModeAction("test1");
        TestSafeModeAction testAction2 = new TestSafeModeAction("test2");
        SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction1});
        try {
            SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction2});
            Assert.fail(
                    "SafeModeController should have thrown an exception when "
                            + "re-registering actions");
        } catch (IllegalStateException e) {
            // Expected
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_cannotRegisterDuplicateActionId() throws Throwable {
        Assume.assumeTrue(
                "This behavior is only in debug builds for performance reasons",
                BuildConfig.ENABLE_ASSERTS);
        TestSafeModeAction testAction1 = new TestSafeModeAction("test1");
        TestSafeModeAction testAction2 = new TestSafeModeAction("test1");
        try {
            SafeModeController.getInstance()
                    .registerActions(new SafeModeAction[] {testAction1, testAction2});
            Assert.fail(
                    "SafeModeController should have thrown an exception for " + "a duplicate ID");
        } catch (IllegalArgumentException e) {
            // Expected
        }
    }

    private static <T> Set<T> asSet(T... values) {
        Set<T> set = new HashSet<>();
        for (T value : values) {
            set.add(value);
        }
        return set;
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_mustRegisterBeforeExecuting() throws Throwable {
        try {
            Set<String> actions = asSet("test");
            SafeModeController.getInstance().executeActions(actions);
            Assert.fail(
                    "SafeModeController should have thrown an exception when "
                            + "executing without registering");
        } catch (IllegalStateException e) {
            // Expected
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_executesRegisteredAction() throws Throwable {
        TestSafeModeAction testAction = new TestSafeModeAction("test");
        SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction});

        Set<String> actions = asSet("test");
        SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals(
                "TestSafeModeAction should have been executed exactly 1 time",
                1,
                testAction.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_doesNotExecuteUnregisteredActions() throws Throwable {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.WebView.SafeMode.ExecutionResult",
                        SafeModeController.SafeModeExecutionResult.ACTION_UNKNOWN);
        TestSafeModeAction testAction = new TestSafeModeAction("test");
        SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction});

        Set<String> actions = asSet(testAction.getId(), "unregistered1", "unregistered2");
        @SafeModeController.SafeModeExecutionResult
        int success = SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals(
                "TestSafeModeAction should have been executed exactly 1 time",
                1,
                testAction.getCallCount());
        Assert.assertEquals(
                "Overall status should be unknown if at least one action is unrecognized and no"
                        + " actions failed",
                success,
                SafeModeController.SafeModeExecutionResult.ACTION_UNKNOWN);
        histogramExpectation.assertExpected("Unregistered safemode actions should be logged");
        // If we got this far without crashing, we assume SafeModeController correctly ignored the
        // unregistered actions.
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_testStatusHierarchyEnforcedCorrectly() throws Throwable {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.WebView.SafeMode.ExecutionResult",
                        SafeModeController.SafeModeExecutionResult.ACTION_FAILED);
        TestSafeModeAction testActionFailed = new TestSafeModeAction("testFail", false);
        TestSafeModeAction testActionSuccess = new TestSafeModeAction("testSuccess");
        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {testActionSuccess, testActionFailed});

        // The possible execution statuses include SUCCESS, ACTION_FAILED, and ACTION_UNKNOWN.
        // The precedence is ACTION_FAILED, ACTION_UNKNOWN, and then SUCCESS in descending order.
        Set<String> actions =
                asSet(
                        testActionSuccess.getId(),
                        testActionFailed.getId(),
                        "unregistered1",
                        "unregistered2");
        @SafeModeController.SafeModeExecutionResult
        int success = SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals(
                testActionFailed.getId() + " should have been executed exactly 1 time",
                1,
                testActionFailed.getCallCount());
        Assert.assertEquals(
                testActionSuccess.getId() + " should have been executed exactly 1 time",
                1,
                testActionSuccess.getCallCount());
        Assert.assertEquals(
                "Overall status should be failure if at least one"
                        + " action is unrecognized and at least one action is a failure",
                success,
                SafeModeController.SafeModeExecutionResult.ACTION_FAILED);
        histogramExpectation.assertExpected("Failed safemode actions should be logged");
        // If we got this far without crashing, we assume SafeModeController correctly ignored the
        // unregistered actions.
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_onlyExecutesSpecifiedActions() throws Throwable {
        TestSafeModeAction testAction1 = new TestSafeModeAction("test1");
        TestSafeModeAction testAction2 = new TestSafeModeAction("test2");
        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {testAction1, testAction2});

        Set<String> actions = asSet("test1");
        SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals(
                "testAction1 should have been executed exactly 1 time",
                1,
                testAction1.getCallCount());
        Assert.assertEquals(
                "testAction2 should not have been executed", 0, testAction2.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_executesActionsInOrder() throws Throwable {
        TestSafeModeAction testAction1 = new TestSafeModeAction("test1");
        TestSafeModeAction testAction2 = new TestSafeModeAction("test2");
        TestSafeModeAction testAction3 = new TestSafeModeAction("test3");

        Set<String> actions = asSet(testAction1.getId(), testAction2.getId(), testAction3.getId());

        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {testAction1, testAction2, testAction3});
        SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals(
                "testAction1 should be executed first", 1, testAction1.getExecutionOrder());
        Assert.assertEquals(
                "testAction2 should be executed second", 2, testAction2.getExecutionOrder());
        Assert.assertEquals(
                "testAction3 should be executed third", 3, testAction3.getExecutionOrder());

        // Unregister and re-register in the opposite order. Verify that they're executed in the new
        // registration order.
        SafeModeController.getInstance().unregisterActionsForTesting();
        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {testAction3, testAction2, testAction1});
        SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals(
                "testAction3 should be executed first the next time",
                4,
                testAction3.getExecutionOrder());
        Assert.assertEquals(
                "testAction2 should be executed second the next time",
                5,
                testAction2.getExecutionOrder());
        Assert.assertEquals(
                "testAction1 should be executed third the next time",
                6,
                testAction1.getExecutionOrder());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_overallSuccessStatus() throws Throwable {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.WebView.SafeMode.ExecutionResult",
                        SafeModeController.SafeModeExecutionResult.SUCCESS);
        TestSafeModeAction successAction1 = new TestSafeModeAction("successAction1");
        TestSafeModeAction successAction2 = new TestSafeModeAction("successAction2");
        Set<String> allSuccessful = asSet(successAction1.getId(), successAction2.getId());
        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {successAction1, successAction2});
        @SafeModeController.SafeModeExecutionResult
        int success = SafeModeController.getInstance().executeActions(allSuccessful);
        Assert.assertEquals(
                "Overall status should be successful if all actions are successful",
                success,
                SafeModeController.SafeModeExecutionResult.SUCCESS);
        histogramExpectation.assertExpected(
                "Overall status should be successful if all actions are successful");
        Assert.assertEquals(
                "successAction1 should have been executed exactly 1 time",
                1,
                successAction1.getCallCount());
        Assert.assertEquals(
                "successAction2 should have been executed exactly 1 time",
                1,
                successAction2.getCallCount());

        // Register a new set of actions where at least one indicates failure.
        histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.WebView.SafeMode.ExecutionResult",
                        SafeModeController.SafeModeExecutionResult.ACTION_FAILED);
        SafeModeController.getInstance().unregisterActionsForTesting();
        TestSafeModeAction failAction = new TestSafeModeAction("failAction", false);
        Set<String> oneFailure =
                asSet(successAction1.getId(), failAction.getId(), successAction2.getId());
        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {successAction1, failAction, successAction2});
        success = SafeModeController.getInstance().executeActions(oneFailure);
        Assert.assertEquals(
                "Overall status should be failure if at least one action fails",
                success,
                SafeModeController.SafeModeExecutionResult.ACTION_FAILED);
        histogramExpectation.assertExpected(
                "Overall status should be failure if at least one action fails");
        Assert.assertEquals(
                "successAction1 should have been executed again", 2, successAction1.getCallCount());
        Assert.assertEquals(
                "failAction should have been executed exactly 1 time",
                1,
                failAction.getCallCount());
        Assert.assertEquals(
                "successAction2 should have been executed again", 2, successAction2.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_namesRecordedAsExpected() throws Throwable {
        HistogramWatcher.Builder histogramWatcherBuilder = HistogramWatcher.newBuilder();
        histogramWatcherBuilder.expectIntRecords(
                "Android.WebView.SafeMode.ActionName",
                SafeModeController.sSafeModeActionLoggingMap.get(
                        SafeModeActionIds.DELETE_VARIATIONS_SEED),
                SafeModeController.sSafeModeActionLoggingMap.get(
                        SafeModeActionIds.FAST_VARIATIONS_SEED));
        HistogramWatcher watcher = histogramWatcherBuilder.build();

        TestSafeModeAction successAction1 =
                new TestSafeModeAction(SafeModeActionIds.DELETE_VARIATIONS_SEED);
        TestSafeModeAction successAction2 =
                new TestSafeModeAction(SafeModeActionIds.FAST_VARIATIONS_SEED);
        Set<String> allSuccessful = asSet(successAction1.getId(), successAction2.getId());
        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {successAction1, successAction2});
        SafeModeController.getInstance().executeActions(allSuccessful);
        watcher.assertExpected(
                "SafeModeAction names should be recorded: ["
                        + successAction1.getId()
                        + ", "
                        + successAction2.getId()
                        + "]");
        Assert.assertEquals(
                "successAction1 should have been executed exactly 1 time",
                1,
                successAction1.getCallCount());
        Assert.assertEquals(
                "successAction2 should have been executed exactly 1 time",
                1,
                successAction2.getCallCount());
        SafeModeController.getInstance().unregisterActionsForTesting();

        histogramWatcherBuilder = HistogramWatcher.newBuilder();
        histogramWatcherBuilder.expectIntRecords(
                "Android.WebView.SafeMode.ActionName",
                SafeModeController.sSafeModeActionLoggingMap.get(SafeModeActionIds.NOOP),
                SafeModeController.sSafeModeActionLoggingMap.get(
                        SafeModeActionIds.DISABLE_ANDROID_AUTOFILL));
        watcher = histogramWatcherBuilder.build();
        successAction1 = new TestSafeModeAction(SafeModeActionIds.NOOP);
        successAction2 = new TestSafeModeAction(SafeModeActionIds.DISABLE_ANDROID_AUTOFILL);
        allSuccessful = asSet(successAction1.getId(), successAction2.getId());
        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {successAction1, successAction2});
        SafeModeController.getInstance().executeActions(allSuccessful);
        watcher.assertExpected(
                "SafeModeAction names should be recorded: ["
                        + successAction1.getId()
                        + ", "
                        + successAction2.getId()
                        + "]");
        Assert.assertEquals(
                "successAction1 should have been executed exactly 1 time",
                1,
                successAction1.getCallCount());
        Assert.assertEquals(
                "successAction2 should have been executed exactly 1 time",
                1,
                successAction2.getCallCount());
        SafeModeController.getInstance().unregisterActionsForTesting();

        histogramWatcherBuilder = HistogramWatcher.newBuilder();
        histogramWatcherBuilder.expectIntRecords(
                "Android.WebView.SafeMode.ActionName",
                SafeModeController.sSafeModeActionLoggingMap.get(
                        SafeModeActionIds.DISABLE_AW_SAFE_BROWSING),
                SafeModeController.sSafeModeActionLoggingMap.get(
                        SafeModeActionIds.DISABLE_ORIGIN_TRIALS));
        watcher = histogramWatcherBuilder.build();
        successAction1 = new TestSafeModeAction(SafeModeActionIds.DISABLE_AW_SAFE_BROWSING);
        successAction2 = new TestSafeModeAction(SafeModeActionIds.DISABLE_ORIGIN_TRIALS);
        allSuccessful = asSet(successAction1.getId(), successAction2.getId());
        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {successAction1, successAction2});
        SafeModeController.getInstance().executeActions(allSuccessful);
        watcher.assertExpected(
                "SafeModeAction names should be recorded: ["
                        + successAction1.getId()
                        + ", "
                        + successAction2.getId()
                        + "]");
        Assert.assertEquals(
                "successAction1 should have been executed exactly 1 time",
                1,
                successAction1.getCallCount());
        Assert.assertEquals(
                "successAction2 should have been executed exactly 1 time",
                1,
                successAction2.getCallCount());
    }

    @Test
    @MediumTest
    public void testSafeModeAction_canRegisterBrowserActions() throws Exception {
        // Validity check: verify we can register the production SafeModeAction list. As long as
        // this finishes without throwing, assume the list is in good shape (e.g., no duplicate
        // SafeModeAction IDs).
        SafeModeController.getInstance().registerActions(BrowserSafeModeActionList.sList);
    }

    @Test
    @MediumTest
    public void testFastVariations_executesSuccessWithLocalSeed() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            File newFile = VariationsUtils.getNewSeedFile();
            Assert.assertTrue("Seed file already exists", oldFile.createNewFile());
            Assert.assertTrue("New seed file already exists", newFile.createNewFile());
            VariationsTestUtils.writeMockSeed(oldFile);
            VariationsTestUtils.writeMockSeed(newFile);
            // Create time stamp file so the mitigation will use the local app directory instead
            // of waiting for the ContentProvider to provide a seed for it.
            VariationsUtils.updateStampTime();
            FastVariationsSeedSafeModeAction action =
                    new FastVariationsSeedSafeModeAction(TEST_WEBVIEW_PACKAGE_NAME);
            boolean success = action.execute();
            Assert.assertTrue("VariationsSeedSafeModeAction should indicate success", success);

            TestLoader loader = new TestLoader(new TestLoaderResult());
            loader.startVariationsInit();
            boolean loadedSeed = loader.finishVariationsInit();
            Assert.assertTrue(
                    "Did not load a variations seed even though it should have been available",
                    loadedSeed);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    @Test
    @MediumTest
    public void testFastVariations_executesFailureWithLocalSeedExpiredAndNoSeedFromContentProvider()
            throws Exception {
        long startingTime = 54000L;
        AwVariationsSeedFetcher.setMocks(mScheduler, mDownloader);
        AwVariationsSeedFetcher.setUseSmallJitterForTesting();
        FastVariationsSeedSafeModeAction action =
                new FastVariationsSeedSafeModeAction(TEST_WEBVIEW_PACKAGE_NAME);

        // Mimic embedded data directory that lives separately from nonembedded data directory for
        // testing
        File seedFileDirectory =
                new File(
                        PathUtils.getDataDirectory() + File.separator,
                        "embedded-data-directory-for-test");
        Assert.assertTrue("Seed file directory already exists", seedFileDirectory.mkdir());
        File embeddedSeedFile =
                new File(seedFileDirectory.getPath() + File.separator, "variations_seed");
        Assert.assertTrue("Seed file already exists", embeddedSeedFile.createNewFile());
        // mark the embedded seed file as expired
        embeddedSeedFile.setLastModified(
                startingTime + VariationsFastFetchModeUtils.MAX_ALLOWABLE_SEED_AGE_MS + 1L);
        FastVariationsSeedSafeModeAction.setAlternateSeedFilePath(embeddedSeedFile);

        try {
            boolean success = action.execute();
            Assert.assertFalse(
                    "FastVariationsSeedSafeModeAction should indicate"
                            + " failure with no variations seed",
                    success);
        } finally {
            VariationsTestUtils.deleteSeeds();
            FileUtils.recursivelyDeleteFile(seedFileDirectory, FileUtils.DELETE_ALL);
        }
    }

    @Test
    @MediumTest
    public void testFastVariations_executesSuccessWithLocalSeedExpiredAndSeedFromContentProvider()
            throws Exception {
        long startingTime = 54000L;
        AwVariationsSeedFetcher.setMocks(mScheduler, mDownloader);
        AwVariationsSeedFetcher.setUseSmallJitterForTesting();
        FastVariationsSeedSafeModeAction action =
                new FastVariationsSeedSafeModeAction(TEST_WEBVIEW_PACKAGE_NAME);

        File nonEmbeddedSeedFile = VariationsUtils.getSeedFile();
        Assert.assertTrue("Seed file already exists", nonEmbeddedSeedFile.createNewFile());
        VariationsTestUtils.writeMockSeed(nonEmbeddedSeedFile);
        // Mark the nonembedded seed file as unexpired
        final Date date = mock(Date.class);
        when(date.getTime()).thenReturn(startingTime + 1L);
        AwVariationsSeedFetcher.setDateForTesting(date);
        nonEmbeddedSeedFile.setLastModified(startingTime);

        // Mimic embedded data directory that lives separately from nonembedded data directory for
        // testing
        File seedFileDirectory =
                new File(
                        PathUtils.getDataDirectory() + File.separator,
                        "embedded-data-directory-for-test");
        Assert.assertTrue("Seed file directory already exists", seedFileDirectory.mkdir());
        File embeddedSeedFile =
                new File(seedFileDirectory.getPath() + File.separator, "variations_seed");
        Assert.assertTrue("Seed file already exists", embeddedSeedFile.createNewFile());
        // mark the embedded seed file as expired
        embeddedSeedFile.setLastModified(
                startingTime + VariationsFastFetchModeUtils.MAX_ALLOWABLE_SEED_AGE_MS + 1L);
        FastVariationsSeedSafeModeAction.setAlternateSeedFilePath(embeddedSeedFile);

        try {
            File oldFile = VariationsUtils.getSeedFile();
            File newFile = VariationsUtils.getNewSeedFile();
            VariationsTestUtils.writeMockSeed(oldFile);
            VariationsTestUtils.writeMockSeed(newFile);
            setSafeMode(Arrays.asList(action.getId()));

            boolean success = action.execute();
            Assert.assertTrue(
                    "FastVariationsSeedSafeModeAction should not indicate"
                            + " failure with variations seed in ContentProvider's data directory",
                    success);
        } finally {
            VariationsTestUtils.deleteSeeds();
            FileUtils.recursivelyDeleteFile(seedFileDirectory, FileUtils.DELETE_ALL);
        }
    }

    @Test
    @MediumTest
    public void testFastVariations_executesSuccessWithLocalSeedAlmostExpired() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            File newFile = VariationsUtils.getNewSeedFile();
            Assert.assertTrue("Seed file already exists", oldFile.createNewFile());
            Assert.assertTrue("New seed file already exists", newFile.createNewFile());
            VariationsTestUtils.writeMockSeed(oldFile);
            VariationsTestUtils.writeMockSeed(newFile);
            // Create an almost expired time stamp file so the mitigation will not request a new
            // seed from the ContentProvider
            long now = new Date().getTime();
            VariationsUtils.updateStampTime(
                    now + VariationsFastFetchModeUtils.MAX_ALLOWABLE_SEED_AGE_MS - 1);
            FastVariationsSeedSafeModeAction action =
                    new FastVariationsSeedSafeModeAction(TEST_WEBVIEW_PACKAGE_NAME);
            boolean success = action.execute();
            Assert.assertTrue("VariationsSeedSafeModeAction should indicate success", success);

            TestLoader loader = new TestLoader(new TestLoaderResult());
            loader.startVariationsInit();
            boolean loadedSeed = loader.finishVariationsInit();
            Assert.assertTrue(
                    "Did not load a variations seed even though it should have been available",
                    loadedSeed);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    @Test
    @MediumTest
    public void testFastVariations_failsWithoutVariationsSeed() throws Exception {
        VariationsTestUtils.deleteSeeds(); // ensure no seed files exist
        FastVariationsSeedSafeModeAction action =
                new FastVariationsSeedSafeModeAction(TEST_WEBVIEW_PACKAGE_NAME);
        // Since no seed file exists in the embedding app directory, and the ContentProvider
        // does not have a valid seed to return to the FastVariationsSeedSafeModeAction,
        // it fails with no valid seed to load.
        boolean success = action.execute();
        Assert.assertFalse(
                "FastVariationsSeedSafeModeAction should indicate"
                        + " failure with no variations seed",
                success);
        TestLoader loader = new TestLoader(new TestLoaderResult());
        loader.startVariationsInit();
        boolean loadedSeed = loader.finishVariationsInit();
        Assert.assertFalse(
                "Loaded a variations seed even though it should not have one to load.", loadedSeed);
    }

    @Test
    @MediumTest
    public void testVariations_deletesSeedFiles() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            File newFile = VariationsUtils.getNewSeedFile();
            Assert.assertTrue("Seed file already exists", oldFile.createNewFile());
            Assert.assertTrue("New seed file already exists", newFile.createNewFile());
            VariationsTestUtils.writeMockSeed(oldFile);
            VariationsTestUtils.writeMockSeed(newFile);
            VariationsSeedSafeModeAction action = new VariationsSeedSafeModeAction();
            boolean success = action.execute();
            Assert.assertTrue("VariationsSeedSafeModeAction should indicate success", success);
            Assert.assertFalse(
                    "Old seed should have been deleted but it still exists", oldFile.exists());
            Assert.assertFalse(
                    "New seed should have been deleted but it still exists", newFile.exists());
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    @Test
    @MediumTest
    public void testVariations_doesNotLoadExperiments() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            File newFile = VariationsUtils.getNewSeedFile();
            Assert.assertTrue("Seed file already exists", oldFile.createNewFile());
            Assert.assertTrue("New seed file already exists", newFile.createNewFile());
            VariationsTestUtils.writeMockSeed(oldFile);
            VariationsTestUtils.writeMockSeed(newFile);
            VariationsSeedSafeModeAction action = new VariationsSeedSafeModeAction();
            boolean success = action.execute();
            Assert.assertTrue("VariationsSeedSafeModeAction should indicate success", success);

            TestLoader loader = new TestLoader(new TestLoaderResult());
            loader.startVariationsInit();
            boolean loadedSeed = loader.finishVariationsInit();
            Assert.assertFalse(
                    "Loaded a variations seed even though it should have been deleted by SafeMode",
                    loadedSeed);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    @Test
    @MediumTest
    public void testVariations_doesNothingIfSeedDoesNotExist() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            File newFile = VariationsUtils.getNewSeedFile();
            VariationsSeedSafeModeAction action = new VariationsSeedSafeModeAction();
            boolean success = action.execute();
            Assert.assertTrue("VariationsSeedSafeModeAction should indicate success", success);
            Assert.assertFalse("Old seed should never have existed", oldFile.exists());
            Assert.assertFalse("New seed should never have existed", newFile.exists());
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    @Test
    @MediumTest
    public void testTrustedPackage_invalidCert() throws Exception {
        TrustedPackage invalidPackage =
                new TrustedPackage(TEST_WEBVIEW_PACKAGE_NAME, FAKE_CERT_HASH, null);
        Assert.assertFalse(
                "wrong certificate should not verify",
                invalidPackage.verify(TEST_WEBVIEW_PACKAGE_NAME));
    }

    private static class TestTrustedPackage extends TrustedPackage {
        private boolean mIsDebug = true;

        TestTrustedPackage(String packageName, byte[] release, byte[] debug) {
            super(packageName, release, debug);
        }

        @Override
        protected boolean isDebugAndroid() {
            return mIsDebug;
        }

        void setDebugBuildForTesting(boolean debug) {
            mIsDebug = debug;
        }
    }

    @Test
    @MediumTest
    public void testTrustedPackage_wrongPackageName() throws Exception {
        TrustedPackage webviewTestShell =
                new TestTrustedPackage(TEST_WEBVIEW_PACKAGE_NAME, TEST_WEBVIEW_CERT_HASH, null);
        Assert.assertFalse(
                "Wrong pacakge name should not verify",
                webviewTestShell.verify("com.fake.package.name"));
    }

    @Test
    @MediumTest
    public void testTrustedPackage_eitherCertCanMatchOnDebugAndroid() throws Exception {
        TrustedPackage webviewTestShell =
                new TrustedPackage(TEST_WEBVIEW_PACKAGE_NAME, TEST_WEBVIEW_CERT_HASH, null);
        Assert.assertTrue(
                "The WebView test shell should match itself",
                webviewTestShell.verify(TEST_WEBVIEW_PACKAGE_NAME));
        // Adding a non-matching certificate should not change anything (we should still trust
        // this).
        webviewTestShell =
                new TestTrustedPackage(
                        TEST_WEBVIEW_PACKAGE_NAME, TEST_WEBVIEW_CERT_HASH, FAKE_CERT_HASH);
        Assert.assertTrue(
                "The WebView test shell should match itself",
                webviewTestShell.verify(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    public void testTrustedPackage_debugCertsOnlyTrustedOnDebugAndroid() throws Exception {
        // Use a fake release cert and real debug cert so this package can only be trusted on a
        // debug build.
        TestTrustedPackage webviewTestShell =
                new TestTrustedPackage(
                        TEST_WEBVIEW_PACKAGE_NAME, FAKE_CERT_HASH, TEST_WEBVIEW_CERT_HASH);

        webviewTestShell.setDebugBuildForTesting(true);
        Assert.assertTrue(
                "Debug cert should be trusted on debug Android build",
                webviewTestShell.verify(TEST_WEBVIEW_PACKAGE_NAME));

        webviewTestShell.setDebugBuildForTesting(false);
        Assert.assertFalse(
                "Debug cert should not be trusted on release Android build",
                webviewTestShell.verify(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    public void testTrustedPackage_verifyWebViewTestShell() throws Exception {
        TrustedPackage webviewTestShell =
                new TestTrustedPackage(TEST_WEBVIEW_PACKAGE_NAME, TEST_WEBVIEW_CERT_HASH, null);
        Assert.assertTrue(
                "The WebView test shell should match itself",
                webviewTestShell.verify(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNonEmbeddedSafeModeActionList_executeNonEmbeddedActionWhenRegisteredAndEnabled()
            throws Throwable {
        TestNonEmbeddedSafeModeAction testAction = new TestNonEmbeddedSafeModeAction("test");

        // Register test action
        SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction});
        // Enable test action
        setSafeMode(Arrays.asList(testAction.getId()));
        Assert.assertEquals(
                "Test action should be executed once", 1, testAction.getActivatedCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void
            testNonEmbeddedSafeModeActionList_executeNonEmbeddedActionWhenRegisteredAndDisabled()
                    throws Throwable {
        TestNonEmbeddedSafeModeAction testAction = new TestNonEmbeddedSafeModeAction("test");
        SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction});

        // Enable test action
        setSafeMode(Arrays.asList(testAction.getId()));
        Assert.assertEquals(
                "Test action should be executed once when enabled",
                1,
                testAction.getActivatedCallCount());

        // Disable test action
        setSafeMode(Arrays.asList());
        Assert.assertEquals(
                "Test action should be executed when previously "
                        + "enabled and current command is to disable it",
                1,
                testAction.getDeactivatedCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNonEmbeddedSafeModeActionList_doNotExecuteNonEmbeddedActionAlreadyEnabled()
            throws Throwable {
        TestNonEmbeddedSafeModeAction testAction = new TestNonEmbeddedSafeModeAction("test");
        SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction});

        // Enable test action
        setSafeMode(Arrays.asList(testAction.getId()));
        Assert.assertEquals(
                "Test action should be executed once when enabled",
                1,
                testAction.getActivatedCallCount());

        // Enable test action again Since test action was previously enabled and it is being
        // enabled again, it will not execute again since there is no state change
        setSafeMode(Arrays.asList(testAction.getId()));
        Assert.assertEquals(
                "Test action should not be executed when previously "
                        + "enabled and current command is to enable it",
                1,
                testAction.getActivatedCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNonEmbeddedSafeModeActionList_doNotExecuteNonEmbeddedActionAlreadyDisabled()
            throws Throwable {
        TestNonEmbeddedSafeModeAction testAction = new TestNonEmbeddedSafeModeAction("test");
        SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction});

        // Enable test action
        setSafeMode(Arrays.asList(testAction.getId()));
        Assert.assertEquals(
                "Test action should be executed once when enabled",
                1,
                testAction.getActivatedCallCount());

        // Disable test action
        setSafeMode(Arrays.asList());
        Assert.assertEquals(
                "Test action should be executed when previously "
                        + "enabled and current command is to disable it",
                1,
                testAction.getDeactivatedCallCount());

        // Disable test action again
        setSafeMode(Arrays.asList());
        Assert.assertEquals(
                "Test action should not be executed when previously"
                        + " disabled and current command is to disable it",
                1,
                testAction.getDeactivatedCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNonEmbeddedSafeModeActionList_doNotExecuteNonEmbeddedActionWhenUnregistered()
            throws Throwable {
        TestNonEmbeddedSafeModeAction testAction = new TestNonEmbeddedSafeModeAction("test");
        // Register empty action list
        SafeModeController.getInstance().registerActions(new SafeModeAction[] {});

        // Enable test action
        setSafeMode(Arrays.asList(testAction.getId()));
        Assert.assertEquals(
                "Test action should not be executed when enabled and not registered",
                0,
                testAction.getActivatedCallCount());

        // Disable test action
        setSafeMode(Arrays.asList());
        Assert.assertEquals(
                "Test action should not be executed when disabled and not registered",
                0,
                testAction.getDeactivatedCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNonEmbeddedSafeModeActionList_nonEmbeddedActionsFailureIsCaptured()
            throws Throwable {
        TestNonEmbeddedSafeModeAction failingTestAction =
                new TestNonEmbeddedSafeModeAction("test", false);
        TestNonEmbeddedSafeModeAction passingTestAction =
                new TestNonEmbeddedSafeModeAction("passingtest");
        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {failingTestAction, passingTestAction});

        Assert.assertFalse(
                "Overall status is failure if mitigations indicate"
                        + "failure when SafeMode is activated",
                NonEmbeddedSafeModeActionsSetupCleanup.executeNonEmbeddedActionsOnStateChange(
                        new HashSet<>(Arrays.asList()),
                        new HashSet<>(Arrays.asList(failingTestAction.getId()))));
        Assert.assertFalse(
                "Overall status is failure if mitigations indicate"
                        + "failure when SafeMode is deactivated",
                NonEmbeddedSafeModeActionsSetupCleanup.executeNonEmbeddedActionsOnStateChange(
                        new HashSet<>(Arrays.asList(failingTestAction.getId())),
                        new HashSet<>(Arrays.asList())));
        Assert.assertFalse(
                "Overall status is failure if at least one mitigation indicates "
                        + "failure when SafeMode is activated",
                NonEmbeddedSafeModeActionsSetupCleanup.executeNonEmbeddedActionsOnStateChange(
                        new HashSet<>(Arrays.asList()),
                        new HashSet<>(
                                Arrays.asList(
                                        failingTestAction.getId(), passingTestAction.getId()))));

        setSafeMode(Arrays.asList(passingTestAction.getId()));
        Assert.assertTrue(
                "Overall status is success if all mitigations success",
                NonEmbeddedSafeModeActionsSetupCleanup.executeNonEmbeddedActionsOnStateChange(
                        new HashSet<>(Arrays.asList()),
                        new HashSet<>(Arrays.asList(passingTestAction.getId()))));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void
            testNonEmbeddedSafeModeActionList_multipleNonEmbeddedActionsNotExecutedWhenNotRegistered()
                    throws Throwable {
        TestNonEmbeddedSafeModeAction testAction1 = new TestNonEmbeddedSafeModeAction("test1");
        TestNonEmbeddedSafeModeAction testAction2 = new TestNonEmbeddedSafeModeAction("test2");
        TestNonEmbeddedSafeModeAction testAction3 =
                new TestNonEmbeddedSafeModeAction("test3", false);

        // Actions are not yet registered, activating them
        setSafeMode(Arrays.asList(testAction1.getId(), testAction2.getId(), testAction3.getId()));
        Assert.assertEquals(
                "Test action 1 should not be executed", 0, testAction1.getActivatedCallCount());
        Assert.assertEquals(
                "Test action 2 should not be executed", 0, testAction2.getActivatedCallCount());
        Assert.assertEquals(
                "Test action 3 should not be executed", 0, testAction3.getActivatedCallCount());

        // Register all three actions
        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {testAction1, testAction2, testAction3});

        // turn off
        setSafeMode(Arrays.asList());
        Assert.assertEquals(
                "Test action 1 should be executed once", 1, testAction1.getDeactivatedCallCount());
        Assert.assertEquals(
                "Test action 2 should be executed once", 1, testAction2.getDeactivatedCallCount());
        Assert.assertEquals(
                "Test action 3 should be executed once", 1, testAction3.getDeactivatedCallCount());

        // Unregister test action 2
        SafeModeController.getInstance().unregisterActionsForTesting();
        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {testAction1, testAction3});

        // turn on
        setSafeMode(Arrays.asList(testAction1.getId(), testAction2.getId(), testAction3.getId()));
        Assert.assertEquals(
                "Test action 1 should be activated once", 1, testAction1.getActivatedCallCount());
        Assert.assertEquals(
                "Test action 2 should not be activated since it's not in the list",
                0,
                testAction2.getActivatedCallCount());
        Assert.assertEquals(
                "Test action 3 should be activated once", 1, testAction3.getActivatedCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNonEmbeddedSafeModeActionList_turnOffMultipleNonEmbeddedActions()
            throws Throwable {
        TestNonEmbeddedSafeModeAction testAction1 = new TestNonEmbeddedSafeModeAction("test1");
        TestNonEmbeddedSafeModeAction testAction2 = new TestNonEmbeddedSafeModeAction("test2");
        TestNonEmbeddedSafeModeAction testAction3 =
                new TestNonEmbeddedSafeModeAction("test3", false);
        SafeModeController.getInstance()
                .registerActions(new SafeModeAction[] {testAction1, testAction2, testAction3});

        Assert.assertEquals(
                "Test action 1 should not be executed", 0, testAction1.getActivatedCallCount());
        Assert.assertEquals(
                "Test action 2 should not be executed", 0, testAction2.getActivatedCallCount());
        Assert.assertEquals(
                "Test action 3 should not be executed", 0, testAction3.getActivatedCallCount());
        setSafeMode(Arrays.asList(testAction1.getId(), testAction2.getId(), testAction3.getId()));

        setSafeMode(Arrays.asList(testAction2.getId()));
        Assert.assertEquals(
                "Test action 1 should be activated once", 1, testAction1.getActivatedCallCount());
        Assert.assertEquals(
                "Test action 1 should be deactivated once",
                1,
                testAction1.getDeactivatedCallCount());
        Assert.assertEquals(
                "Test action 2 should be activated once", 1, testAction2.getActivatedCallCount());
        Assert.assertEquals(
                "Test action 2 should not be deactivated",
                0,
                testAction2.getDeactivatedCallCount());
        Assert.assertEquals(
                "Test action 3 should be activated once", 1, testAction3.getActivatedCallCount());
        Assert.assertEquals(
                "Test action 3 should be deactivated once",
                1,
                testAction3.getDeactivatedCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeActionList_turnOffSafeModeSeedFetch() throws Throwable {
        AwVariationsSeedFetcher.setUseSmallJitterForTesting();
        NonEmbeddedFastVariationsSeedSafeModeAction testAction =
                new NonEmbeddedFastVariationsSeedSafeModeAction();
        AwVariationsSeedFetcher.setMocks(mScheduler, mDownloader);

        SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction});
        SafeModeAction[] actions = SafeModeController.getInstance().getRegisteredActions();
        Assert.assertThat("Actions list should not be empty", actions.length, greaterThan(0));

        mScheduler.assertNotScheduled();
        setSafeMode(Arrays.asList(testAction.getId()));
        mScheduler.waitForMessageCallback();
        mScheduler.assertScheduled(JOB_ID);

        setSafeMode(Arrays.asList());
        mScheduler.assertNotScheduled();
    }

    private void setSafeMode(List<String> actions) throws RemoteException {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
        try (ServiceConnectionHelper helper =
                new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(actions);
        }
    }
}
