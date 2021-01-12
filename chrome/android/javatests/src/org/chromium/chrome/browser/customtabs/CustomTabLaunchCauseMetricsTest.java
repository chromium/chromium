// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.metrics.LaunchCauseMetrics;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/**
 * Tests basic functionality of CustomTabLaunchCauseMetricsTest.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class CustomTabLaunchCauseMetricsTest {
    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> LaunchCauseMetrics.resetForTests());
    }

    private static int histogramCountForValue(int value) {
        return RecordHistogram.getHistogramValueCountForTesting(
                LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM, value);
    }

    // CustomTabActivity can't be mocked, because Mockito can't handle @ApiLevel annotations, and so
    // can't mock classes that use them because classes can't be found on older API levels.
    private CustomTabActivity makeActivity(boolean twa) {
        return new CustomTabActivity() {
            @Override
            public int getActivityType() {
                return twa ? ActivityType.TRUSTED_WEB_ACTIVITY : ActivityType.CUSTOM_TAB;
            }
        };
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testCCTLaunch() throws Throwable {
        CustomTabActivity activity = makeActivity(false);
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.CUSTOM_TAB);

        CustomTabLaunchCauseMetrics metrics = new CustomTabLaunchCauseMetrics(activity);
        metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(
                count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.CUSTOM_TAB));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTWALaunch() throws Throwable {
        CustomTabActivity activity = makeActivity(true);
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.TWA);

        CustomTabLaunchCauseMetrics metrics = new CustomTabLaunchCauseMetrics(activity);
        metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.TWA));
    }
}
