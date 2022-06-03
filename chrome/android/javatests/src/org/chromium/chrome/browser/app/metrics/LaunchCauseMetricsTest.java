// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.metrics;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/**
 * Tests basic functionality of LaunchCauseMetrics.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class LaunchCauseMetricsTest {
    @Mock
    private Activity mActivity;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        });
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ApplicationStatus.resetActivitiesForInstrumentationTests();
            LaunchCauseMetrics.resetForTests();
        });
    }

    private static int histogramCountForValue(int value) {
        return RecordHistogram.getHistogramValueCountForTesting(
                LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM, value);
    }

    private static class TestLaunchCauseMetrics extends LaunchCauseMetrics {
        private boolean mDisplayOff;

        public TestLaunchCauseMetrics(Activity activity) {
            super(activity);
        }

        @Override
        protected @LaunchCause int computeIntentLaunchCause() {
            return LaunchCause.OTHER;
        }

        @Override
        protected boolean isDisplayOff(Activity activity) {
            return mDisplayOff;
        }

        public void setDisplayOff(boolean off) {
            mDisplayOff = off;
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testRecordsOncePerLaunch() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER);
        TestLaunchCauseMetrics metrics = new TestLaunchCauseMetrics(mActivity);
        metrics.onReceivedIntent();
        metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER));

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        metrics.recordLaunchCause();
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER));

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        metrics.recordLaunchCause();
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER));

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        metrics.onReceivedIntent();
        metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testRecordsOnceWithMultipleInstances() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER);
        TestLaunchCauseMetrics metrics = new TestLaunchCauseMetrics(mActivity);
        metrics.onReceivedIntent();
        metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER));
        new TestLaunchCauseMetrics(mActivity).recordLaunchCause();
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testLaunchedFromRecents() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS);
        TestLaunchCauseMetrics metrics = new TestLaunchCauseMetrics(mActivity);
        metrics.onLaunchFromRecents();
        metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS));
        LaunchCauseMetrics.resetForTests();

        metrics.onLaunchFromRecents();
        metrics.onUserLeaveHint();
        metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testResumedFromRecents() throws Throwable {
        int recentsCount = histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS);
        int backCount = histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS_OR_BACK);
        TestLaunchCauseMetrics metrics = new TestLaunchCauseMetrics(mActivity);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        metrics.recordLaunchCause();
        recentsCount++;
        Assert.assertEquals(
                recentsCount, histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS));

        metrics.onUserLeaveHint();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        metrics.recordLaunchCause();
        backCount++;
        Assert.assertEquals(
                backCount, histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS_OR_BACK));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testResumedFromScreenOn() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.FOREGROUND_WHEN_LOCKED);
        TestLaunchCauseMetrics metrics = new TestLaunchCauseMetrics(mActivity);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);

        metrics.setDisplayOff(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(count,
                histogramCountForValue(LaunchCauseMetrics.LaunchCause.FOREGROUND_WHEN_LOCKED));

        metrics.setDisplayOff(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        metrics.recordLaunchCause();
        Assert.assertEquals(count,
                histogramCountForValue(LaunchCauseMetrics.LaunchCause.FOREGROUND_WHEN_LOCKED));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testLaunchAborted() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS);
        TestLaunchCauseMetrics metrics = new TestLaunchCauseMetrics(mActivity);
        metrics.onReceivedIntent();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        // Should clear the state that we received an intent.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS));
    }
}
