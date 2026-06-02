// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.CleanupReference;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestThreadUtils;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicInteger;

/** Test suite for {@link CleanupReference}. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These are unit tests
@Batch(Batch.UNIT_TESTS)
public class CleanupReferenceTest {
    private static final AtomicInteger sObjectCount = new AtomicInteger(0);

    private static class ReferredObject {

        private final CleanupReference mRef;

        public ReferredObject() {
            sObjectCount.incrementAndGet();
            // This cleanup lambda MUST NOT refer to the ReferredObject instance, as that would
            // create a reference cycle and defeat GC of this object.
            mRef = new CleanupReference(this, (e) -> sObjectCount.decrementAndGet());
        }

        public void cleanupNow() {
            mRef.cleanupNow();
        }
    }

    @Before
    public void setUp() {
        // Force garbage collection and wait for the count to drop to 0, in case there's anything
        // left over from another test run. (Do it in setup rather than teardown to avoid
        // clobbering any test failures.)
        collectGarbage();
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(sObjectCount.get(), Matchers.is(0)));
        Assert.assertEquals(
                "Expected sObjectCount to be 0 before setUp. This probably means there are"
                        + " unprocessed objects carried over from another test.",
                0,
                sObjectCount.get());
    }

    private void collectGarbage() {
        // While this is only a 'hint' to the VM, it's generally effective and sufficient on
        // dalvik. If this changes in future, maybe try allocating a few gargantuan objects
        // too, to force the GC to work.
        Runtime.getRuntime().gc();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateSingle() {
        Assert.assertEquals(0, sObjectCount.get());

        ReferredObject instance = new ReferredObject();
        Assert.assertEquals(1, sObjectCount.get());

        instance = null;
        // Ensure compiler / instrumentation does not strip out the assignment.
        Assert.assertNull(instance);
        collectGarbage();
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(sObjectCount.get(), Matchers.is(0)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPartialCleanup() {
        Assert.assertEquals(0, sObjectCount.get());

        // Verify that unrelated objects are not affected by cleanup of the target object.
        ReferredObject unrelatedInstance = new ReferredObject();

        Assert.assertEquals(1, sObjectCount.get());

        ReferredObject instance = new ReferredObject();
        Assert.assertEquals(2, sObjectCount.get());

        instance = null;
        // Ensure compiler / instrumentation does not strip out the assignment.
        Assert.assertNull(instance);
        collectGarbage();
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(sObjectCount.get(), Matchers.is(1)));

        TestThreadUtils.flushNonDelayedLooperTasks();
        Assert.assertEquals(1, sObjectCount.get());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateMany() {
        Assert.assertEquals(0, sObjectCount.get());

        final int instanceCount = 20;
        ReferredObject[] instances = new ReferredObject[instanceCount];

        for (int i = 0; i < instanceCount; ++i) {
            instances[i] = new ReferredObject();
            Assert.assertEquals(i + 1, sObjectCount.get());
        }

        instances = null;
        // Ensure compiler / instrumentation does not strip out the assignment.
        Assert.assertNull(instances);
        // Calling sObjectCount.get() before collectGarbage() seems to be required for the objects
        // to be GC'ed only when building using GN.
        Assert.assertNotEquals(sObjectCount.get(), -1);
        collectGarbage();
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(sObjectCount.get(), Matchers.is(0)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCleanupNowOffMainThread() {
        Assert.assertEquals(0, sObjectCount.get());

        // Verify that unrelated objects are not affected by cleanup of the target object.
        ReferredObject unrelatedInstance = new ReferredObject();

        Assert.assertEquals(1, sObjectCount.get());

        ReferredObject instance = new ReferredObject();
        Assert.assertEquals(2, sObjectCount.get());

        // Ensure the UI thread can't process our message immediately.
        CountDownLatch latch = new CountDownLatch(1);
        ThreadUtils.postOnUiThread(
                () -> {
                    try {
                        latch.await();
                    } catch (InterruptedException e) {
                        throw new RuntimeException(e);
                    }
                });
        // Cleanup always happens on the UI thread, so should not happen from this instrumentation
        // thread.
        instance.cleanupNow();
        Assert.assertEquals(2, sObjectCount.get());
        latch.countDown();

        // The UI thread should now get to it.
        collectGarbage();
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(sObjectCount.get(), Matchers.is(1)));

        // Best effort check that cleanup doesn't happen twice.
        instance.cleanupNow();
        TestThreadUtils.flushNonDelayedLooperTasks();
        Assert.assertEquals(1, sObjectCount.get());

        // Best effort check that cleanup also doesn't happen again after dropping the reference.
        instance = null;
        // Ensure compiler / instrumentation does not strip out the assignment.
        Assert.assertNull(instance);
        collectGarbage();
        Assert.assertEquals(1, sObjectCount.get());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCleanupNowOnMainThread() {
        Assert.assertEquals(0, sObjectCount.get());

        ReferredObject unrelatedInstance = new ReferredObject();
        Assert.assertEquals(1, sObjectCount.get());

        ReferredObject instance = new ReferredObject();
        Assert.assertEquals(2, sObjectCount.get());

        CountDownLatch latch = new CountDownLatch(1);
        ThreadUtils.postOnUiThread(
                () -> {
                    Assert.assertEquals(2, sObjectCount.get());

                    instance.cleanupNow();

                    // cleanupNow called on the UI thread should be synchronous, but not trigger
                    // cleanup of unrelated objects.
                    Assert.assertEquals(1, sObjectCount.get());
                });

        // Ensure cleanup also doesn't happen again after dropping the reference.
        unrelatedInstance = null;
        // Ensure compiler / instrumentation does not strip out the assignment.
        Assert.assertNull(unrelatedInstance);
        collectGarbage();
        latch.countDown();

        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(sObjectCount.get(), Matchers.is(0)));
    }
}
