// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.memory_leaks;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;

import androidx.test.filters.LargeTest;

import leakcanary.AppWatcher;
import leakcanary.ObjectWatcher;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.EnableLeakChecks;
import org.chromium.base.test.util.LeakCanaryChecker;
import org.chromium.base.test.util.LeakCanaryChecker.LeakCheckHook;

/**
 * Tests that {@link LeakCanaryChecker} reports objects that stay reachable and does not report
 * objects that are collected while it waits for garbage collection.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableLeakChecks
public class LeakCanaryCheckerLeakDetectionTest {
    private static volatile Object sHeld;

    private final ObjectWatcher mObjectWatcher = AppWatcher.INSTANCE.getObjectWatcher();
    private final LeakCheckHook mLeakCheckHook = new LeakCheckHook();

    @Before
    public void setUp() {
        // Start from a clean state so objects watched by earlier tests don't affect the result.
        mObjectWatcher.clearWatchedObjects();
    }

    @After
    public void tearDown() {
        sHeld = null;
        mObjectWatcher.clearWatchedObjects();
    }

    /**
     * Creates the watched object in its own stack frame, so that the test method's frame doesn't
     * keep a stale reference to it.
     */
    private void watchNewHeldObject(String description) {
        sHeld = new Object();
        mObjectWatcher.expectWeaklyReachable(sHeld, description);
    }

    @Test
    @LargeTest
    public void testReportsObjectThatStaysReachable() {
        watchNewHeldObject("Object held in a static field on purpose");
        Class<?> testClass = getClass();

        long startMs = SystemClock.uptimeMillis();
        AssertionError e =
                assertThrows(
                        AssertionError.class, () -> mLeakCheckHook.onAfterTestClass(testClass));
        long elapsedMs = SystemClock.uptimeMillis() - startMs;

        assertNotNull("Expected the LeakCanary failure as the cause", e.getCause());
        assertTrue(
                "LeakCanaryChecker should wait at least "
                        + LeakCanaryChecker.GC_POLL_TIMEOUT_MS
                        + "ms before giving up, waited "
                        + elapsedMs
                        + "ms",
                elapsedMs >= LeakCanaryChecker.GC_POLL_TIMEOUT_MS);
    }

    @Test
    @LargeTest
    public void testDoesNotReportObjectCollectedWhileWaiting() {
        watchNewHeldObject("Object released shortly after");
        new Thread(
                        () -> {
                            SystemClock.sleep(300);
                            sHeld = null;
                        })
                .start();

        long startMs = SystemClock.uptimeMillis();
        mLeakCheckHook.onAfterTestClass(getClass());
        long elapsedMs = SystemClock.uptimeMillis() - startMs;

        assertFalse(mObjectWatcher.getHasWatchedObjects());
        // Without the GC polling in LeakCanaryChecker, LeakCanary only notices the object was
        // released at its final GC, 5 seconds in.
        assertTrue(
                "Expected LeakCanaryChecker to notice the collection before "
                        + LeakCanaryChecker.GC_POLL_TIMEOUT_MS
                        + "ms, took "
                        + elapsedMs
                        + "ms",
                elapsedMs < LeakCanaryChecker.GC_POLL_TIMEOUT_MS);
    }
}
