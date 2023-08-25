// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import androidx.test.core.content.pm.PackageInfoBuilder;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowPackageManager;
import org.robolectric.shadows.ShadowPausedAsyncTask;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.customtabs.features.branding.BrandingChecker.BrandingAppIdType;
import org.chromium.chrome.browser.customtabs.features.branding.BrandingChecker.BrandingLaunchTimeStorage;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/** Unit test for {@link BrandingChecker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPackageManager.class, ShadowSystemClock.class, ShadowPostTask.class})
@LooperMode(Mode.PAUSED)
public class BrandingCheckerUnitTest {
    private static final String PACKAGE_1 = "com.example.myapplication";
    private static final String PACKAGE_2 = "org.foo.bar";
    private static final String NEW_APPLICATION = "com.example.new.application";
    private static final String INVALID_ID = "";
    private static final long TEST_BRANDING_CADENCE = 10;
    private static final long PACKAGE_1_BRANDING_SHOWN_SINCE_START = 2;
    private static final long PACKAGE_2_BRANDING_SHOWN_SINCE_START = 5;

    TestBrandingStorage mStorage = new TestBrandingStorage();
    private Context mContext;
    long mStartTime;

    @Before
    public void setup() {
        mContext = ContextUtils.getApplicationContext();
        mStartTime = SystemClock.elapsedRealtime();

        mStorage.put(PACKAGE_1, PACKAGE_1_BRANDING_SHOWN_SINCE_START + mStartTime);
        mStorage.put(PACKAGE_2, PACKAGE_2_BRANDING_SHOWN_SINCE_START + mStartTime);

        ShadowPackageManager pm = Shadows.shadowOf(mContext.getPackageManager());
        pm.installPackage(PackageInfoBuilder.newBuilder().setPackageName(PACKAGE_1).build());
        pm.installPackage(PackageInfoBuilder.newBuilder().setPackageName(PACKAGE_2).build());
        pm.installPackage(PackageInfoBuilder.newBuilder().setPackageName(NEW_APPLICATION).build());

        ShadowPostTask.setTestImpl(new TestImpl() {
            final Handler mHandler = new Handler(Looper.getMainLooper());

            @Override
            public void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay) {
                mHandler.postDelayed(task, delay);
            }
        });
    }

    @After
    public void tearDown() {
        UmaRecorderHolder.resetForTesting();
        ShadowPackageManager.reset();
        ShadowSystemClock.reset();
        ShadowPausedAsyncTask.reset();
    }

    @Test
    public void testKnownPackage() {
        CallbackDelegate callbackDelegate = new CallbackDelegate();
        advanceTimeMs(20);
        long showBrandingTime = SystemClock.elapsedRealtime();

        BrandingChecker checker = createBrandingChecker(PACKAGE_1, callbackDelegate);
        checker.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        mainLooper().idle();
        assertEquals("Branding is checked after cadence, BrandingDecision should be TOOLBAR. ",
                BrandingDecision.TOOLBAR, callbackDelegate.getBrandingDecision());

        assertEquals("Show branding time is different.", showBrandingTime, mStorage.get(PACKAGE_1));

        assertHistogramRecorded(/*decision*/ BrandingDecision.TOOLBAR, /*isPackageValid*/ true,
                /*isTaskCanceled*/ false);
    }

    @Test
    public void testNewPackage() {
        CallbackDelegate callbackDelegate = new CallbackDelegate();
        BrandingChecker checker = createBrandingChecker(NEW_APPLICATION, callbackDelegate);
        checker.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        mainLooper().idle();
        long showBrandingTime = SystemClock.elapsedRealtime();
        assertEquals("Branding is checked for new package, BrandingDecision should be TOAST. ",
                BrandingDecision.TOAST, callbackDelegate.getBrandingDecision());
        assertEquals("Show branding time is different.", showBrandingTime,
                mStorage.get(NEW_APPLICATION));

        assertHistogramRecorded(/*decision*/ BrandingDecision.TOAST, /*isPackageValid*/ true,
                /*isTaskCanceled*/ false);
    }

    @Test
    public void testCancel() {
        CallbackDelegate callbackDelegate = new CallbackDelegate();
        advanceTimeMs(20);

        BrandingChecker checker = createBrandingChecker(PACKAGE_1, callbackDelegate);
        checker.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        // Run looper for #doInBackground
        mainLooper().runOneTask();
        assertEquals("BrandingDecision is not set yet.", 0, callbackDelegate.getCallCount());

        // Cancel before the result gets back.
        checker.cancel(true);
        mainLooper().idle();
        long showBrandingTime = SystemClock.elapsedRealtime();
        assertEquals("Branding check canceled, BrandingDecision should be the test default. ",
                BrandingDecision.TOAST, callbackDelegate.getBrandingDecision());
        assertEquals("Show branding time is different.", showBrandingTime, mStorage.get(PACKAGE_1));

        assertHistogramRecorded(/*decision*/ BrandingDecision.TOAST, /*isPackageValid*/ true,
                /*isTaskCanceled*/ true);
    }

    @Test
    public void testInvalidPackage() {
        CallbackDelegate callbackDelegate = new CallbackDelegate();
        BrandingChecker checker = createBrandingChecker(INVALID_ID, callbackDelegate);
        checker.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        mainLooper().idle();
        assertEquals("Package is invalid, BrandingDecision should be the test default. ",
                BrandingDecision.TOAST, callbackDelegate.getBrandingDecision());
        assertEquals(
                "Branding time should not record for invalid id.", -1, mStorage.get(INVALID_ID));

        assertHistogramRecorded(/*decision*/ BrandingDecision.TOAST, /*isPackageValid*/ false,
                /*isTaskCanceled*/ false);
    }

    @Test
    public void testBrandingAppIdType() {
        assertEquals(BrandingAppIdType.PACKAGE_NAME, BrandingChecker.getAppIdType(PACKAGE_1));
        assertEquals(BrandingAppIdType.PACKAGE_NAME, BrandingChecker.getAppIdType(PACKAGE_2));

        assertEquals(BrandingAppIdType.INVALID, BrandingChecker.getAppIdType(""));

        // Package not installed. Regarded as referrer string.
        assertEquals(BrandingAppIdType.REFERRER, BrandingChecker.getAppIdType("com.not.installed"));
        assertEquals(BrandingAppIdType.REFERRER, BrandingChecker.getAppIdType("2//com.seedly"));
    }

    @Test
    public void testLongWaitingOnStorage() {
        CallbackDelegate callbackDelegate1 = new CallbackDelegate();
        CallbackDelegate callbackDelegate2 = new CallbackDelegate();
        BrandingChecker checker1 = createBrandingChecker(PACKAGE_1, callbackDelegate1);
        BrandingChecker checker2 = createBrandingChecker(PACKAGE_2, callbackDelegate2);

        advanceTimeMs(12);

        checker1.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        checker2.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        mainLooper().runOneTask();
        mainLooper().runOneTask();

        // Advance the time assuming, simulating time elapse for reading storage.
        advanceTimeMs(10);
        long showBrandingTime = SystemClock.elapsedRealtime();
        mainLooper().idle();
        assertEquals("Branding is checked after cadence, BrandingDecision should be TOOLBAR. ",
                BrandingDecision.TOOLBAR, callbackDelegate1.getBrandingDecision());
        assertEquals("Branding is checked within cadence, BrandingDecision should be EMPTY. ",
                BrandingDecision.NONE, callbackDelegate2.getBrandingDecision());
        assertEquals("Branding time should update for package 1.", showBrandingTime,
                mStorage.get(PACKAGE_1));
        assertEquals("Branding time should not record for package 2.",
                mStartTime + PACKAGE_2_BRANDING_SHOWN_SINCE_START, mStorage.get(PACKAGE_2));
    }

    private BrandingChecker createBrandingChecker(
            String packageName, CallbackDelegate callbackDelegate) {
        return new BrandingChecker(packageName, mStorage, callbackDelegate::notifyCalled,
                TEST_BRANDING_CADENCE, BrandingDecision.TOAST);
    }

    private ShadowLooper mainLooper() {
        return Shadows.shadowOf(Looper.getMainLooper());
    }

    private void advanceTimeMs(long increments) {
        ShadowSystemClock.advanceBy(increments, TimeUnit.MILLISECONDS);
    }

    private void assertHistogramRecorded(
            @BrandingDecision int decision, boolean isPackageValid, boolean isCanceled) {
        assertHistogramSampleRecorded(
                "CustomTabs.Branding.BrandingCheckCanceled", isCanceled ? 1 : 0);
        assertHistogramSampleRecorded("CustomTabs.Branding.BrandingDecision", decision);
        assertHistogramSampleRecorded(
                "CustomTabs.Branding.IsPackageNameValid", isPackageValid ? 1 : 0);

        assertEquals("<CustomTabs.Branding.BrandingCheckDuration> not recorded.", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "CustomTabs.Branding.BrandingCheckDuration"));
    }

    private void assertHistogramSampleRecorded(String name, int sample) {
        assertEquals("<" + name + "> not recorded.", 1,
                RecordHistogram.getHistogramTotalCountForTesting(name));
        assertEquals("<" + name + "> sample <" + sample + "> count is different.", 1,
                RecordHistogram.getHistogramValueCountForTesting(name, sample));
    }

    private static class CallbackDelegate extends CallbackHelper {
        private @BrandingDecision int mBrandingDecision;

        public void notifyCalled(@BrandingDecision int decision) {
            mBrandingDecision = decision;
            notifyCalled();
        }

        public @BrandingDecision int getBrandingDecision() {
            assert getCallCount() > 0;
            return mBrandingDecision;
        }
    }

    /**
     * BrandingLaunchTimeStorage that implemented with static map.
     */
    static class TestBrandingStorage implements BrandingLaunchTimeStorage {
        private final Map<String, Long> mLastBrandingTime = new HashMap<>();

        @Override
        public long get(String packageName) {
            Long lastBrandingShownTime = mLastBrandingTime.get(packageName);
            return lastBrandingShownTime != null ? lastBrandingShownTime
                                                 : BrandingChecker.BRANDING_TIME_NOT_FOUND;
        }

        @Override
        public void put(String packageName, long brandingLaunchTime) {
            mLastBrandingTime.put(packageName, brandingLaunchTime);
        }
    }
}
