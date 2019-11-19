// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric.services;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.services.AwVariationsSeedFetcher;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.concurrent.TimeUnit;

@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AwVariationsSeedFetcherTest {
    // Jan 1, 2019 12:00AM GMT
    private static final long FAKE_NOW_MS = 1546300800000L;

    @Before
    public void setUp() {
        AwVariationsSeedFetcher.setMinJobPeriodMillisForTesting(TimeUnit.DAYS.toMillis(1));
    }

    @Test
    @SmallTest
    public void testJobNotDelayedIfNotPreviouslyRun() {
        long delayMs = AwVariationsSeedFetcher.computeJobDelay(FAKE_NOW_MS, 0);
        Assert.assertEquals("Job should have no delay on first run", 0, delayMs);
    }

    @Test
    @SmallTest
    public void testJobNotDelayedOutwideThrottleWindow() {
        long oldRequestTimeMs = FAKE_NOW_MS - TimeUnit.DAYS.toMillis(4);
        long delayMs = AwVariationsSeedFetcher.computeJobDelay(FAKE_NOW_MS, oldRequestTimeMs);
        Assert.assertEquals("Job should have no delay when run after throttle window", 0, delayMs);
    }

    @Test
    @SmallTest
    public void testJobDelayedIfRunWithinThrottleWindow() {
        long recentRequestTimeMs = FAKE_NOW_MS - TimeUnit.HOURS.toMillis(8);
        long delayMs = AwVariationsSeedFetcher.computeJobDelay(FAKE_NOW_MS, recentRequestTimeMs);
        Assert.assertEquals("Job should be delayed if run within throttle window",
                TimeUnit.HOURS.toMillis(16), delayMs);
    }
}
