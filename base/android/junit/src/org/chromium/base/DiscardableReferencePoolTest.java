// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.DiscardableReferencePool.DiscardableReference;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.lang.ref.WeakReference;

/** Tests for {@link DiscardableReferencePool}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DiscardableReferencePoolTest {
    /**
     * Tests that draining the pool clears references and allows objects to be garbage collected.
     */
    @Test
    public void testDrain() {
        DiscardableReferencePool pool = new DiscardableReferencePool();

        Object object = new Object();
        WeakReference<Object> weakReference = new WeakReference<>(object);

        DiscardableReference<Object> discardableReference = pool.put(object);
        Assert.assertEquals(object, discardableReference.get());

        // Drop reference to the object itself, to allow it to be garbage-collected.
        object = null;

        pool.drain();

        // The discardable reference should be null now.
        Assert.assertNull(discardableReference.get());

        // The object is not (strongly) reachable anymore, so the weak reference may or may not be
        // null (it could be if a GC has happened since the pool was drained). It should be
        // eligible for garbage collection.
        Assert.assertTrue(GarbageCollectionTestUtils.canBeGarbageCollected(weakReference));
    }

    @Test
    public void testRemoveAfterDrainDoesNotThrow() {
        DiscardableReferencePool pool = new DiscardableReferencePool();

        Object object = new Object();
        WeakReference<Object> weakReference = new WeakReference<>(object);

        DiscardableReference<Object> discardableReference = pool.put(object);
        Assert.assertEquals(object, discardableReference.get());

        // Release the strong reference.
        object = null;

        pool.drain();

        // Shouldn't throw any exception.
        pool.remove(discardableReference);

        // The discardable reference should be null now.
        Assert.assertNull(discardableReference.get());

        // The object is not (strongly) reachable anymore, so the weak reference may or may not be
        // null (it could be if a GC has happened since the pool was drained). It should be
        // eligible for garbage collection.
        Assert.assertTrue(GarbageCollectionTestUtils.canBeGarbageCollected(weakReference));
    }

    @Test
    public void testDrainAfterRemoveDoesNotThrow() {
        DiscardableReferencePool pool = new DiscardableReferencePool();

        Object object = new Object();
        WeakReference<Object> weakReference = new WeakReference<>(object);

        DiscardableReference<Object> discardableReference = pool.put(object);
        Assert.assertEquals(object, discardableReference.get());

        // Release the strong reference.
        object = null;

        pool.remove(discardableReference);

        // Shouldn't throw any exception.
        pool.drain();

        // The discardable reference should be null now.
        Assert.assertNull(discardableReference.get());

        // The object is not (strongly) reachable anymore, so the weak reference may or may not be
        // null (it could be if a GC has happened since the pool was drained). It should be
        // eligible for garbage collection.
        Assert.assertTrue(GarbageCollectionTestUtils.canBeGarbageCollected(weakReference));
    }

    /**
     * Tests that dropping the (last) discardable reference to an object allows it to be regularly
     * garbage collected.
     */
    @Test
    public void testReferenceGCd() {
        DiscardableReferencePool pool = new DiscardableReferencePool();

        Object object = new Object();
        WeakReference<Object> weakReference = new WeakReference<>(object);

        DiscardableReference<Object> discardableReference = pool.put(object);
        Assert.assertEquals(object, discardableReference.get());

        // Drop reference to the object itself and to the discardable reference, allowing the object
        // to be garbage-collected.
        object = null;
        discardableReference = null;

        // The object is not (strongly) reachable anymore, so the weak reference may or may not be
        // null (it could be if a GC has happened since the pool was drained). It should be
        // eligible for garbage collection.
        Assert.assertTrue(GarbageCollectionTestUtils.canBeGarbageCollected(weakReference));
    }
}
