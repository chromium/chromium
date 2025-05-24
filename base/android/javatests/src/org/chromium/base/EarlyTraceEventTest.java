// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Process;
import android.os.SystemClock;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.EarlyTraceEvent.AsyncEvent;
import org.chromium.base.EarlyTraceEvent.Event;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link EarlyTraceEvent}.
 *
 * TODO(lizeb): Move to robolectric tests.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class EarlyTraceEventTest {
    private static final String EVENT_NAME = "MyEvent";
    private static final String EVENT_NAME2 = "MyOtherEvent";
    private static final long EVENT_ID = 1;

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized();
        EarlyTraceEvent.reset();
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCanRecordEvent() {
        EarlyTraceEvent.enable();
        long myThreadId = Process.myTid();
        long beforeNanos = System.nanoTime();
        long beforeThreadMillis = SystemClock.currentThreadTimeMillis();
        EarlyTraceEvent.begin(EVENT_NAME, /* isToplevel= */ false);
        EarlyTraceEvent.end(EVENT_NAME, /* isToplevel= */ false);
        Assert.assertTrue(EarlyTraceEvent.enabled());
        long afterNanos = System.nanoTime();
        long afterThreadMillis = SystemClock.currentThreadTimeMillis();

        List<Event> matchingEvents =
                EarlyTraceEvent.getMatchingCompletedEventsForTesting(EVENT_NAME);
        Assert.assertEquals(2, matchingEvents.size());
        Event beginEvent = matchingEvents.get(0);
        Event endEvent = matchingEvents.get(1);
        Assert.assertEquals(EVENT_NAME, beginEvent.mName);
        Assert.assertEquals(myThreadId, beginEvent.mThreadId);
        Assert.assertEquals(EVENT_NAME, endEvent.mName);
        Assert.assertEquals(myThreadId, endEvent.mThreadId);
        Assert.assertFalse(beginEvent.mIsToplevel);
        Assert.assertFalse(endEvent.mIsToplevel);
        Assert.assertTrue(beforeNanos <= beginEvent.mTimeNanos);
        Assert.assertTrue(endEvent.mTimeNanos <= afterNanos);
        Assert.assertTrue(beforeThreadMillis <= beginEvent.mThreadTimeMillis);
        Assert.assertTrue(endEvent.mThreadTimeMillis <= afterThreadMillis);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCanRecordAsyncEvent() {
        EarlyTraceEvent.enable();
        long beforeNanos = System.nanoTime();
        EarlyTraceEvent.startAsync(EVENT_NAME, EVENT_ID);
        EarlyTraceEvent.finishAsync(EVENT_NAME, EVENT_ID);
        long afterNanos = System.nanoTime();

        List<AsyncEvent> matchingEvents = new ArrayList<AsyncEvent>();
        synchronized (EarlyTraceEvent.sLock) {
            for (AsyncEvent evt : EarlyTraceEvent.sAsyncEvents) {
                if (evt.mName.equals(EVENT_NAME)) {
                    matchingEvents.add(evt);
                }
            }
        }
        Assert.assertEquals(2, matchingEvents.size());
        AsyncEvent eventStart = matchingEvents.get(0);
        AsyncEvent eventEnd = matchingEvents.get(1);
        Assert.assertEquals(EVENT_NAME, eventStart.mName);
        Assert.assertEquals(EVENT_ID, eventStart.mId);
        Assert.assertEquals(EVENT_NAME, eventEnd.mName);
        Assert.assertEquals(EVENT_ID, eventEnd.mId);
        Assert.assertTrue(
                beforeNanos <= eventStart.mTimeNanos && eventEnd.mTimeNanos <= afterNanos);
        Assert.assertTrue(eventStart.mTimeNanos <= eventEnd.mTimeNanos);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCanRecordEventUsingTryWith() {
        EarlyTraceEvent.enable();
        long myThreadId = Process.myTid();
        long beforeNanos = System.nanoTime();
        try (TraceEvent e = TraceEvent.scoped(EVENT_NAME)) {
            // Required comment to pass presubmit checks.
        }
        long afterNanos = System.nanoTime();

        List<Event> matchingEvents =
                EarlyTraceEvent.getMatchingCompletedEventsForTesting(EVENT_NAME);
        Assert.assertEquals(2, matchingEvents.size());
        Event beginEvent = matchingEvents.get(0);
        Event endEvent = matchingEvents.get(1);
        Assert.assertEquals(EVENT_NAME, beginEvent.mName);
        Assert.assertEquals(myThreadId, beginEvent.mThreadId);
        Assert.assertEquals(EVENT_NAME, endEvent.mName);
        Assert.assertEquals(myThreadId, endEvent.mThreadId);
        Assert.assertTrue(beforeNanos <= beginEvent.mTimeNanos);
        Assert.assertTrue(endEvent.mTimeNanos <= afterNanos);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testIncompleteEvent() {
        EarlyTraceEvent.enable();
        EarlyTraceEvent.begin(EVENT_NAME, /* isToplevel= */ true);

        List<Event> matchingEvents =
                EarlyTraceEvent.getMatchingCompletedEventsForTesting(EVENT_NAME);
        Assert.assertEquals(1, matchingEvents.size());
        Event beginEvent = matchingEvents.get(0);
        Assert.assertEquals(EVENT_NAME, beginEvent.mName);
        Assert.assertTrue(beginEvent.mIsToplevel);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testIgnoreEventsWhenDisabled() {
        EarlyTraceEvent.begin(EVENT_NAME, /* isToplevel= */ false);
        EarlyTraceEvent.end(EVENT_NAME, /* isToplevel= */ false);
        try (TraceEvent e = TraceEvent.scoped(EVENT_NAME2)) {
            // Required comment to pass presubmit checks.
        }
        synchronized (EarlyTraceEvent.sLock) {
            Assert.assertNull(EarlyTraceEvent.sEvents);
        }
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testIgnoreAsyncEventsWhenDisabled() {
        EarlyTraceEvent.startAsync(EVENT_NAME, EVENT_ID);
        EarlyTraceEvent.finishAsync(EVENT_NAME, EVENT_ID);
        synchronized (EarlyTraceEvent.sLock) {
            Assert.assertNull(EarlyTraceEvent.sAsyncEvents);
        }
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCannotBeReenabledOnceFinished() {
        EarlyTraceEvent.enable();
        EarlyTraceEvent.begin(EVENT_NAME, /* isToplevel= */ false);
        EarlyTraceEvent.end(EVENT_NAME, /* isToplevel= */ false);
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

        Thread thread =
                new Thread() {
                    @Override
                    public void run() {
                        TraceEvent.begin(EVENT_NAME);
                        threadId[0] = Process.myTid();
                        TraceEvent.end(EVENT_NAME);
                    }
                };
        thread.start();
        thread.join();

        List<Event> matchingEvents =
                EarlyTraceEvent.getMatchingCompletedEventsForTesting(EVENT_NAME);
        Assert.assertEquals(2, matchingEvents.size());
        Event beginEvent = matchingEvents.get(0);
        Event endEvent = matchingEvents.get(1);
        Assert.assertEquals(threadId[0], beginEvent.mThreadId);
        Assert.assertEquals(threadId[0], endEvent.mThreadId);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testEnableAtStartup() {
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        EarlyTraceEvent.maybeEnableInBrowserProcess();
        Assert.assertFalse(EarlyTraceEvent.enabled());
        EarlyTraceEvent.setBackgroundStartupTracingFlag(false);
        Assert.assertFalse(EarlyTraceEvent.enabled());

        EarlyTraceEvent.setBackgroundStartupTracingFlag(true);
        EarlyTraceEvent.maybeEnableInBrowserProcess();
        Assert.assertTrue(EarlyTraceEvent.getBackgroundStartupTracingFlag());
        Assert.assertTrue(EarlyTraceEvent.enabled());
        EarlyTraceEvent.disable();
        EarlyTraceEvent.setBackgroundStartupTracingFlag(false);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUserOverrideBackgroundTracing() {
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        // Setting command line should disable the background tracing flag.
        CommandLine.getInstance().appendSwitch("trace-startup");
        EarlyTraceEvent.setBackgroundStartupTracingFlag(true);
        EarlyTraceEvent.maybeEnableInBrowserProcess();
        Assert.assertFalse(EarlyTraceEvent.getBackgroundStartupTracingFlag());
        Assert.assertTrue(EarlyTraceEvent.enabled());
        EarlyTraceEvent.disable();
        EarlyTraceEvent.setBackgroundStartupTracingFlag(false);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testEnableInChildProcess() {
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        EarlyTraceEvent.earlyEnableInChildWithoutCommandLine();
        Assert.assertTrue(EarlyTraceEvent.enabled());
        CommandLine.getInstance().appendSwitch("trace-early-java-in-child");
        EarlyTraceEvent.onCommandLineAvailableInChildProcess();
        Assert.assertTrue(EarlyTraceEvent.enabled());

        // Eliminate side effects.
        CommandLine.getInstance().removeSwitch("trace-early-java-in-child");
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testEnableInChildProcessCommandLineLaterOverrides() {
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        EarlyTraceEvent.earlyEnableInChildWithoutCommandLine();
        Assert.assertTrue(EarlyTraceEvent.enabled());
        CommandLine.getInstance().removeSwitch("trace-early-java-in-child");
        EarlyTraceEvent.onCommandLineAvailableInChildProcess();
        Assert.assertFalse(EarlyTraceEvent.enabled());
        synchronized (EarlyTraceEvent.sLock) {
            Assert.assertNull(EarlyTraceEvent.sEvents);
        }
    }
}
