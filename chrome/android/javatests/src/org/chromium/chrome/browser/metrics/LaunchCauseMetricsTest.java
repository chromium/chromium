// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

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
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @After
    public void tearDown() {
        ApplicationStatus.destroyForJUnitTests();
        ThreadUtils.runOnUiThreadBlocking(() -> LaunchCauseMetrics.resetForTests());
    }

    private static int histogramCountForValue(int value) {
        return RecordHistogram.getHistogramValueCountForTesting(
                LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM, value);
    }

    private static class TestLaunchCauseMetrics extends LaunchCauseMetrics {
        @Override
        protected @LaunchCause int computeLaunchCause() {
            return LaunchCause.OTHER;
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testRecordsOncePerLaunch() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER);
        TestLaunchCauseMetrics metrics = new TestLaunchCauseMetrics();
        metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER));

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        metrics.recordLaunchCause();
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER));

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        metrics.recordLaunchCause();
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER));

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testRecordsOnceWithMultipleInstances() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER);
        TestLaunchCauseMetrics metrics = new TestLaunchCauseMetrics();
        metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER));
        new TestLaunchCauseMetrics().recordLaunchCause();
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER));
    }
}
