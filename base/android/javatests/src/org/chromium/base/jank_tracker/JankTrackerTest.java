// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DoNotBatch;

@RunWith(BaseJUnit4ClassRunner.class)
@DoNotBatch(reason = "Test spawns a new thread, ideally don't batch with other tests.")
public class JankTrackerTest {

    private int numThreadsWithName(String name) {
        int count = 0;
        for (Thread thread : Thread.getAllStackTraces().keySet()) {
            if (thread.getName().equals(name)) {
                count++;
            }
        }
        return count;
    }

    @Test
    @SmallTest
    public void testOneJankTrackerThreadForMultipleSchedulers() {
        Assert.assertEquals(0, numThreadsWithName("Jank-Tracker"));

        JankReportingScheduler scheduler = new JankReportingScheduler(new FrameMetricsStore());
        // To trigger static initialization.
        scheduler.getOrCreateHandler();
        Assert.assertEquals(1, numThreadsWithName("Jank-Tracker"));

        JankReportingScheduler scheduler2 = new JankReportingScheduler(new FrameMetricsStore());
        scheduler2.getOrCreateHandler();
        Assert.assertEquals(1, numThreadsWithName("Jank-Tracker"));
    }
}
