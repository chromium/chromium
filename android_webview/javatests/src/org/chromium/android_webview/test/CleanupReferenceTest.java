// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.CleanupReference;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.concurrent.atomic.AtomicInteger;

/** Test suite for {@link CleanupReference}. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class CleanupReferenceTest {
    private static AtomicInteger sObjectCount = new AtomicInteger();

    private static class ReferredObject {

        private CleanupReference mRef;

        // Remember: this MUST be a static class, to avoid an implicit ref back to the
        // owning ReferredObject instance which would defeat GC of that object.
        private static class DestroyRunnable implements Runnable {
            @Override
            public void run() {
                sObjectCount.decrementAndGet();
            }
        };

        public ReferredObject() {
            sObjectCount.incrementAndGet();
            mRef = new CleanupReference(this, new DestroyRunnable());
        }
    }

    @Before
    public void setUp() {
        sObjectCount.set(0);
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
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, () -> sObjectCount.get()));
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
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, () -> sObjectCount.get()));
    }
}
