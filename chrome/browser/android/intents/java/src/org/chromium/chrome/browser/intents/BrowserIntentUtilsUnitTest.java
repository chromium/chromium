// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.intents;

import android.content.Intent;
import android.os.SystemClock;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class BrowserIntentUtilsUnitTest {
    @Test
    @SmallTest
    public void testAddTimestampToIntent() {
        Intent intent = new Intent();
        Assert.assertEquals(-1, BrowserIntentUtils.getLaunchedRealtimeMillis(intent));
        Assert.assertEquals(-1, BrowserIntentUtils.getLaunchedUptimeMillis(intent));
        // Check both before and after to make sure that the returned value is
        // really from {@link SystemClock#elapsedRealtime()}.
        long before = SystemClock.elapsedRealtime();
        BrowserIntentUtils.addLauncherTimestampsToIntent(intent);
        long after = SystemClock.elapsedRealtime();
        Assert.assertTrue(
                "Time should be increasing",
                before <= BrowserIntentUtils.getLaunchedRealtimeMillis(intent));
        Assert.assertTrue(
                "Time should be increasing",
                BrowserIntentUtils.getLaunchedRealtimeMillis(intent) <= after);

        // Check both before and after to make sure that the returned value is
        // really from {@link SystemClock#uptimeMillis()}.
        before = SystemClock.uptimeMillis();
        BrowserIntentUtils.addLauncherTimestampsToIntent(intent);
        after = SystemClock.uptimeMillis();
        Assert.assertTrue(
                "Time should be increasing",
                before <= BrowserIntentUtils.getLaunchedUptimeMillis(intent));
        Assert.assertTrue(
                "Time should be increasing",
                BrowserIntentUtils.getLaunchedUptimeMillis(intent) <= after);
    }
}
