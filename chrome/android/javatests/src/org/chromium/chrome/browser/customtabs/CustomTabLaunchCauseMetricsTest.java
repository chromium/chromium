// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Tests basic functionality of CustomTabLaunchCauseMetrics. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class CustomTabLaunchCauseMetricsTest {
    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.resetActivitiesForInstrumentationTests();
                    LaunchCauseMetrics.resetForTests();
                });
    }

    private static int histogramCountForValue(int value) {
        return RecordHistogram.getHistogramValueCountForTesting(
                LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM, value);
    }

    private CustomTabLaunchCauseMetrics makeLaunchCauseMetrics(boolean twa) {
        // CustomTabActivity can't be mocked, because Mockito can't handle @ApiLevel annotations,
        // and so can't mock classes that use them because classes can't be found on older API
        // levels.
        CustomTabActivity activity =
                new CustomTabActivity() {
                    @Override
                    public int getActivityType() {
                        return twa ? ActivityType.TRUSTED_WEB_ACTIVITY : ActivityType.CUSTOM_TAB;
                    }
                };
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        return new CustomTabLaunchCauseMetrics(activity) {
            @Override
            protected boolean isDisplayOff(Activity context) {
                return false;
            }
        };
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testCCTLaunch() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.CUSTOM_TAB);
        CustomTabLaunchCauseMetrics metrics = makeLaunchCauseMetrics(false);
        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.CUSTOM_TAB, launchCause);
        Assert.assertEquals(
                count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.CUSTOM_TAB));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTWALaunch() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.TWA);
        CustomTabLaunchCauseMetrics metrics = makeLaunchCauseMetrics(true);
        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.TWA, launchCause);
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.TWA));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testNoIntent() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS);
        CustomTabLaunchCauseMetrics metrics = makeLaunchCauseMetrics(true);
        int launchCause = metrics.recordLaunchCause();
        count++;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.RECENTS, launchCause);
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.RECENTS));
    }
}
