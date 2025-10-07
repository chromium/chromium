// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.TraceEvent;
import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class LocationTest {
    /**
     * Test that Location.from() returns null if Chome is not being traced as an optimization to
     * reduce throwaway work when posting tasks.
     */
    @Test
    public void testLocationFromIsNullIfTracingDisabled() {
        final String file = "test.java";
        final String func = "testMethod";
        final int line = 10101010;

        TraceEvent.setEnabled(false);
        Assert.assertNull(Location.from(file, func, line));

        TraceEvent.setEnabled(true);
        Assert.assertEquals(Location.from(file, func, line), new Location(file, func, line));

        // Cleanup step
        TraceEvent.setEnabled(false);
    }
}
