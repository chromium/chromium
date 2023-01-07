// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/**
 * Tests for {@link TraceEvent}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TraceEventTest {
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testDisableEventNameFiltering() {
        TraceEvent.setEventNameFilteringEnabled(false);
        Assert.assertFalse(TraceEvent.eventNameFilteringEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testEnableEventNameFiltering() {
        TraceEvent.setEventNameFilteringEnabled(true);
        Assert.assertTrue(TraceEvent.eventNameFilteringEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testEventNameUnfiltered() {
        TraceEvent.setEventNameFilteringEnabled(false);
        Assert.assertFalse(TraceEvent.eventNameFilteringEnabled());

        // Input string format:
        // ">>>>> Finished to (TARGET) {HASH_CODE} TARGET_NAME: WHAT"
        String realEventName = ">>>>> Finished to (org.chromium.myClass.myMethod) "
                + "{HASH_CODE} org.chromium.myOtherClass.instance: message";

        // Output string format:
        // "{TraceEvent.BasicLooperMonitor.LOOPER_TASK_PREFIX} TARGET(TARGET_NAME)"
        String realEventNameExpected = TraceEvent.BasicLooperMonitor.LOOPER_TASK_PREFIX
                + "org.chromium.myClass.myMethod(org.chromium.myOtherClass.instance)";
        Assert.assertEquals(TraceEvent.BasicLooperMonitor.getTraceEventName(realEventName),
                realEventNameExpected);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testEventNameFiltered() {
        TraceEvent.setEventNameFilteringEnabled(true);
        Assert.assertTrue(TraceEvent.eventNameFilteringEnabled());

        String realEventName = TraceEvent.BasicLooperMonitor.LOOPER_TASK_PREFIX
                + "org.chromium.myClass.myMethod(org.chromium.myOtherClass.instance)";
        Assert.assertEquals(TraceEvent.BasicLooperMonitor.getTraceEventName(realEventName),
                TraceEvent.BasicLooperMonitor.FILTERED_EVENT_NAME);
    }
}