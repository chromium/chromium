// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.chromium.base.EarlyTraceEvent.AsyncEvent;
import static org.chromium.base.EarlyTraceEvent.Event;

import android.os.Process;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link EarlyTraceEvent}.
 *
 * TODO(lizeb): Move to roboelectric tests.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class EarlyTraceEventTest {
    private static final String EVENT_NAME = "MyEvent";
    private static final String EVENT_NAME2 = "MyOtherEvent";
    private static final long EVENT_ID = 1;
    private static final long EVENT_ID2 = 2;

    int getMatchingCompletedEventCount(String eventName) {
        int count = 0;
        for (Event evt : EarlyTraceEvent.sCompletedEvents) {
            if (evt.mName.equals(eventName)) {
                count++;
            }
        }
        return count;
    }

    Event getMatchingCompletedEvent(int idx, String eventName) {
        int currIdx = idx;
        for (Event evt : EarlyTraceEvent.sCompletedEvents) {
            if (currIdx == 0 && evt.mName.equals(eventName)) {
                return evt;
            }
            currIdx--;
        }
        Assert.fail("No event " + eventName + " at index " + idx);
        return null;
    }

    boolean pendingEventsContain(String eventName) {
        return EarlyTraceEvent.sPendingEventByKey.containsKey(
                EarlyTraceEvent.makeEventKeyForCurrentThread(eventName));
    }

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        EarlyTraceEvent.resetForTesting();
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCanRecordEvent() {
        EarlyTraceEvent.enable();
        long myThreadId = Process.myTid();
        long beforeNanos = Event.elapsedRealtimeNanos();
        EarlyTraceEvent.begin(EVENT_NAME);
        EarlyTraceEvent.end(EVENT_NAME);
        long afterNanos = Event.elapsedRealtimeNanos();

        Assert.assertEquals(1, getMatchingCompletedEventCount(EVENT_NAME));
        Assert.assertFalse(pendingEventsContain(EVENT_NAME));
        Event event = getMatchingCompletedEvent(0, EVENT_NAME);
        Assert.assertEquals(EVENT_NAME, event.mName);
        Assert.assertEquals(myThreadId, event.mThreadId);
        Assert.assertTrue(
                beforeNanos <= event.mBeginTimeNanos && event.mBeginTimeNanos <= afterNanos);
        Assert.assertTrue(event.mBeginTimeNanos <= event.mEndTimeNanos);
        Assert.assertTrue(beforeNanos <= event.mEndTimeNanos && event.mEndTimeNanos <= afterNanos);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCanRecordAsyncEvent() {
        EarlyTraceEvent.enable();
        long beforeNanos = Event.elapsedRealtimeNanos();
        EarlyTraceEvent.startAsync(EVENT_NAME, EVENT_ID);
        EarlyTraceEvent.finishAsync(EVENT_NAME, EVENT_ID);
        long afterNanos = Event.elapsedRealtimeNanos();

        List<AsyncEvent> matchingEvents = new ArrayList<AsyncEvent>();
        for (AsyncEvent evt : EarlyTraceEvent.sAsyncEvents) {
            if (evt.mName.equals(EVENT_NAME)) {
                matchingEvents.add(evt);
            }
        }
        Assert.assertEquals(2, matchingEvents.size());
        Assert.assertFalse(pendingEventsContain(EVENT_NAME));
        AsyncEvent eventStart = matchingEvents.get(0);
        AsyncEvent eventEnd = matchingEvents.get(1);
        Assert.assertEquals(EVENT_NAME, eventStart.mName);
        Assert.assertEquals(EVENT_ID, eventStart.mId);
        Assert.assertEquals(EVENT_NAME, eventEnd.mName);
        Assert.assertEquals(EVENT_ID, eventEnd.mId);
        Assert.assertTrue(beforeNanos <= eventStart.mTimestampNanos
                && eventEnd.mTimestampNanos <= afterNanos);
        Assert.assertTrue(eventStart.mTimestampNanos <= eventEnd.mTimestampNanos);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRecordAsyncFinishEventWhenFinishing() {
        EarlyTraceEvent.enable();
        EarlyTraceEvent.startAsync(EVENT_NAME, EVENT_ID);
        EarlyTraceEvent.disable();

        Assert.assertEquals(EarlyTraceEvent.STATE_FINISHING, EarlyTraceEvent.sState);
        for (AsyncEvent evt : EarlyTraceEvent.sAsyncEvents) {
            Assert.assertNotEquals(EVENT_NAME, evt.mName);
        }
        int pendingCount = 0;
        for (String name : EarlyTraceEvent.sPendingAsyncEvents) {
            if (name.equals(EVENT_NAME)) {
                ++pendingCount;
            }
        }
        Assert.assertEquals(1, pendingCount);
        EarlyTraceEvent.finishAsync(EVENT_NAME, EVENT_ID);
        Assert.assertEquals(EarlyTraceEvent.STATE_FINISHED, EarlyTraceEvent.sState);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCanRecordEventUsingTryWith() {
        EarlyTraceEvent.enable();
        long myThreadId = Process.myTid();
        long beforeNanos = Event.elapsedRealtimeNanos();
        try (TraceEvent e = TraceEvent.scoped(EVENT_NAME)) {
            // Required comment to pass presubmit checks.
        }
        long afterNanos = Event.elapsedRealtimeNanos();

        Assert.assertEquals(1, getMatchingCompletedEventCount(EVENT_NAME));
        Assert.assertFalse(pendingEventsContain(EVENT_NAME));
        Event event = getMatchingCompletedEvent(0, EVENT_NAME);
        Assert.assertEquals(EVENT_NAME, event.mName);
        Assert.assertEquals(myThreadId, event.mThreadId);
        Assert.assertTrue(
                beforeNanos <= event.mBeginTimeNanos && event.mBeginTimeNanos <= afterNanos);
        Assert.assertTrue(event.mBeginTimeNanos <= event.mEndTimeNanos);
        Assert.assertTrue(beforeNanos <= event.mEndTimeNanos && event.mEndTimeNanos <= afterNanos);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testIncompleteEvent() {
        EarlyTraceEvent.enable();
        EarlyTraceEvent.begin(EVENT_NAME);

        Assert.assertEquals(0, getMatchingCompletedEventCount(EVENT_NAME));
        Assert.assertTrue(pendingEventsContain(EVENT_NAME));
        EarlyTraceEvent.Event event = EarlyTraceEvent.sPendingEventByKey.get(
                EarlyTraceEvent.makeEventKeyForCurrentThread(EVENT_NAME));
        Assert.assertEquals(EVENT_NAME, event.mName);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testNoDuplicatePendingEventsFromSameThread() {
        EarlyTraceEvent.enable();
        EarlyTraceEvent.begin(EVENT_NAME);
        try {
            EarlyTraceEvent.begin(EVENT_NAME);
        } catch (IllegalArgumentException e) {
            // Expected.
            return;
        }
        Assert.fail();
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testDuplicatePendingEventsFromDifferentThreads() throws Exception {
        EarlyTraceEvent.enable();

        Thread otherThread = new Thread(() -> { EarlyTraceEvent.begin(EVENT_NAME); });
        otherThread.start();
        otherThread.join();

        // At this point we have a pending event with EVENT_NAME name. But events are per
        // thread, so we should be able to start EVENT_NAME event in a different thread.
        EarlyTraceEvent.begin(EVENT_NAME);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testIgnoreEventsWhenDisabled() {
        EarlyTraceEvent.begin(EVENT_NAME);
        EarlyTraceEvent.end(EVENT_NAME);
        try (TraceEvent e = TraceEvent.scoped(EVENT_NAME2)) {
            // Required comment to pass presubmit checks.
        }
        Assert.assertNull(EarlyTraceEvent.sCompletedEvents);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testIgnoreAsyncEventsWhenDisabled() {
        EarlyTraceEvent.startAsync(EVENT_NAME, EVENT_ID);
        EarlyTraceEvent.finishAsync(EVENT_NAME, EVENT_ID);
        Assert.assertNull(EarlyTraceEvent.sAsyncEvents);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testIgnoreNewEventsWhenFinishing() {
        EarlyTraceEvent.enable();
        EarlyTraceEvent.begin(EVENT_NAME);
        EarlyTraceEvent.disable();

        Assert.assertEquals(EarlyTraceEvent.STATE_FINISHING, EarlyTraceEvent.sState);
        EarlyTraceEvent.begin(EVENT_NAME2);
        EarlyTraceEvent.end(EVENT_NAME2);

        Assert.assertTrue(pendingEventsContain(EVENT_NAME));
        Assert.assertFalse(pendingEventsContain(EVENT_NAME2));
        Assert.assertEquals(0, getMatchingCompletedEventCount(EVENT_NAME));
        Assert.assertEquals(0, getMatchingCompletedEventCount(EVENT_NAME2));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testIgnoreNewAsyncEventsWhenFinishing() {
        EarlyTraceEvent.enable();
        EarlyTraceEvent.startAsync(EVENT_NAME, EVENT_ID);
        EarlyTraceEvent.disable();

        Assert.assertEquals(EarlyTraceEvent.STATE_FINISHING, EarlyTraceEvent.sState);
        EarlyTraceEvent.startAsync(EVENT_NAME2, EVENT_ID2);

        int pendingCount = 0;
        for (String name : EarlyTraceEvent.sPendingAsyncEvents) {
            if (name.equals(EVENT_NAME)) {
                ++pendingCount;
            }
        }
        Assert.assertEquals(1, pendingCount);

        for (AsyncEvent evt : EarlyTraceEvent.sAsyncEvents) {
            Assert.assertNotEquals(EVENT_NAME, evt.mName);
        }
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testFinishingToFinished() {
        EarlyTraceEvent.enable();
        EarlyTraceEvent.begin(EVENT_NAME);
        EarlyTraceEvent.disable();

        Assert.assertEquals(EarlyTraceEvent.STATE_FINISHING, EarlyTraceEvent.sState);
        EarlyTraceEvent.begin(EVENT_NAME2);
        EarlyTraceEvent.end(EVENT_NAME2);
        EarlyTraceEvent.end(EVENT_NAME);

        Assert.assertEquals(EarlyTraceEvent.STATE_FINISHED, EarlyTraceEvent.sState);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCannotBeReenabledOnceFinished() {
        EarlyTraceEvent.enable();
        EarlyTraceEvent.begin(EVENT_NAME);
        EarlyTraceEvent.end(EVENT_NAME);
        EarlyTraceEvent.disable();
        Assert.assertEquals(EarlyTraceEvent.STATE_FINISHED, EarlyTraceEvent.sState);

        EarlyTraceEvent.enable();
        Assert.assertEquals(EarlyTraceEvent.STATE_FINISHED, EarlyTraceEvent.sState);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testThreadIdIsRecorded() throws Exception {
        EarlyTraceEvent.enable();
        final long[] threadId = {0};

        Thread thread = new Thread() {
            @Override
            public void run() {
                TraceEvent.begin(EVENT_NAME);
                threadId[0] = Process.myTid();
                TraceEvent.end(EVENT_NAME);
            }
        };
        thread.start();
        thread.join();

        Assert.assertEquals(1, getMatchingCompletedEventCount(EVENT_NAME));
        EarlyTraceEvent.Event event = getMatchingCompletedEvent(0, EVENT_NAME);
        Assert.assertEquals(threadId[0], event.mThreadId);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testEnableAtStartup() {
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        EarlyTraceEvent.maybeEnable();
        Assert.assertFalse(EarlyTraceEvent.enabled());
        EarlyTraceEvent.setBackgroundStartupTracingFlag(false);
        Assert.assertFalse(EarlyTraceEvent.enabled());

        EarlyTraceEvent.setBackgroundStartupTracingFlag(true);
        EarlyTraceEvent.maybeEnable();
        Assert.assertTrue(EarlyTraceEvent.getBackgroundStartupTracingFlag());
        Assert.assertTrue(EarlyTraceEvent.enabled());
        EarlyTraceEvent.disable();
        EarlyTraceEvent.setBackgroundStartupTracingFlag(false);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUserOverrideBackgroundTracing() {
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        // Setting command line should disable the background tracing flag.
        CommandLine.getInstance().appendSwitch("trace-startup");
        EarlyTraceEvent.setBackgroundStartupTracingFlag(true);
        EarlyTraceEvent.maybeEnable();
        Assert.assertFalse(EarlyTraceEvent.getBackgroundStartupTracingFlag());
        Assert.assertTrue(EarlyTraceEvent.enabled());
        EarlyTraceEvent.disable();
        EarlyTraceEvent.setBackgroundStartupTracingFlag(false);
    }
}
