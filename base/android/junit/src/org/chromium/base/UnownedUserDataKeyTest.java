// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.os.Handler;
import android.os.Looper;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.BuildConfig;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.FutureTask;

/** Test class for {@link UnownedUserDataKey}, which also describes typical usage. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UnownedUserDataKeyTest {
    private static void forceGC() {
        try {
            // Run GC and finalizers a few times.
            for (int i = 0; i < 10; ++i) {
                System.gc();
                System.runFinalization();
            }
        } catch (Exception e) {
            // Do nothing.
        }
    }

    private static class TestUnownedUserData implements UnownedUserData {
        private List<UnownedUserDataHost> mDetachedHosts = new ArrayList<>();

        public boolean informOnDetachment = true;

        @Override
        public void onDetachedFromHost(UnownedUserDataHost host) {
            assertTrue(
                    "Should not detach when informOnDetachmentFromHost() return false.",
                    informOnDetachment);
            mDetachedHosts.add(host);
        }

        @Override
        public boolean informOnDetachmentFromHost() {
            return informOnDetachment;
        }

        public void assertDetachedHostsMatch(UnownedUserDataHost... hosts) {
            assertEquals(mDetachedHosts.size(), hosts.length);
            assertArrayEquals(mDetachedHosts.toArray(), hosts);
        }

        /**
         * Use this helper assert only when order of detachments can not be known, such as on
         * invocations of {@link UnownedUserDataKey#detachFromAllHosts(UnownedUserData)}.
         *
         * @param hosts Which hosts it is required that the UnownedUserData has been detached from.
         */
        public void assertDetachedHostsMatchAnyOrder(UnownedUserDataHost... hosts) {
            assertEquals(mDetachedHosts.size(), hosts.length);
            for (UnownedUserDataHost host : hosts) {
                assertTrue("Should have been detached from host", mDetachedHosts.contains(host));
            }
        }

        public void assertNoDetachedHosts() {
            assertDetachedHostsMatch();
        }
    }

    private static class Foo extends TestUnownedUserData {
        public static final UnownedUserDataKey<Foo> KEY = new UnownedUserDataKey<>(Foo.class);
    }

    private static class Bar extends TestUnownedUserData {
        public static final UnownedUserDataKey<Bar> KEY = new UnownedUserDataKey<>(Bar.class);
    }

    private final Foo mFoo = new Foo();
    private final Bar mBar = new Bar();

    private UnownedUserDataHost mHost1;
    private UnownedUserDataHost mHost2;

    @Before
    public void setUp() {
        ShadowLooper.pauseMainLooper();
        mHost1 = new UnownedUserDataHost(new Handler(Looper.getMainLooper()));
        mHost2 = new UnownedUserDataHost(new Handler(Looper.getMainLooper()));
    }

    @After
    public void tearDown() {
        if (!mHost1.isDestroyed()) {
            assertEquals(0, mHost1.getMapSize());
            mHost1.destroy();
        }
        mHost1 = null;
        if (!mHost2.isDestroyed()) {
            assertEquals(0, mHost2.getMapSize());
            mHost2.destroy();
        }
        mHost2 = null;
    }

    @SuppressWarnings({"SelfAssertion", "JUnitIncompatibleType"})
    @Test
    public void testKeyEquality() {
        assertEquals(Foo.KEY, Foo.KEY);
        assertNotEquals(Foo.KEY, new UnownedUserDataKey<>(Foo.class));
        assertNotEquals(Foo.KEY, Bar.KEY);
        assertNotEquals(Foo.KEY, null);
        assertNotEquals(Foo.KEY, new Object());
        assertNotEquals(Bar.KEY, new UnownedUserDataKey<>(Bar.class));
    }

    @Test
    public void testSingleItemSingleHost_retrievalReturnsNullBeforeAttachment() {
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_attachAndDetach() {
        Foo.KEY.attachToHost(mHost1, mFoo);

        assertTrue(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, Foo.KEY.retrieveDataFromHost(mHost1));

        Foo.KEY.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_attachAndGarbageCollectionReturnsNull() {
        Foo foo = new Foo();
        Foo.KEY.attachToHost(mHost1, foo);

        // Intentionally null out `foo` to make it eligible for garbage collection.
        foo = null;
        forceGC();
        runUntilIdle();

        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));

        // NOTE: We can not verify anything using the `foo` variable here, since it has been
        // garbage collected.
    }

    @Test
    public void testSingleItemSingleHost_attachAndDetachFromAllHosts() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_attachAndDetachDetachmentCallbackIsPosted() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.detachFromHost(mHost1);
        mFoo.assertNoDetachedHosts();

        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
    }

    @Test
    public void testSingleItemSingleHost_attachAndDetachNoDetachmentCallback() {
        mFoo.informOnDetachment = false;
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.detachFromHost(mHost1);
        runUntilIdle();

        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_attachAndDetachFromAllHostsNoDetachmentCallback() {
        mFoo.informOnDetachment = false;
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.detachFromAllHosts(mFoo);
        runUntilIdle();

        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_differentKeys() {
        UnownedUserDataKey<Foo> extraKey = new UnownedUserDataKey<>(Foo.class);
        UnownedUserDataKey<Foo> anotherExtraKey = new UnownedUserDataKey<>(Foo.class);

        Foo.KEY.attachToHost(mHost1, mFoo);
        extraKey.attachToHost(mHost1, mFoo);
        anotherExtraKey.attachToHost(mHost1, mFoo);
        runUntilIdle();

        mFoo.assertNoDetachedHosts();
        assertTrue(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, Foo.KEY.retrieveDataFromHost(mHost1));
        assertTrue(extraKey.isAttachedToHost(mHost1));
        assertTrue(extraKey.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, extraKey.retrieveDataFromHost(mHost1));
        assertTrue(anotherExtraKey.isAttachedToHost(mHost1));
        assertTrue(anotherExtraKey.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, anotherExtraKey.retrieveDataFromHost(mHost1));

        Foo.KEY.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertTrue(extraKey.isAttachedToHost(mHost1));
        assertTrue(extraKey.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, extraKey.retrieveDataFromHost(mHost1));
        assertTrue(anotherExtraKey.isAttachedToHost(mHost1));
        assertTrue(anotherExtraKey.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, anotherExtraKey.retrieveDataFromHost(mHost1));

        extraKey.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertFalse(extraKey.isAttachedToHost(mHost1));
        assertFalse(extraKey.isAttachedToAnyHost(mFoo));
        assertNull(extraKey.retrieveDataFromHost(mHost1));
        assertTrue(anotherExtraKey.isAttachedToHost(mHost1));
        assertTrue(anotherExtraKey.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, anotherExtraKey.retrieveDataFromHost(mHost1));

        anotherExtraKey.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost1, mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertFalse(extraKey.isAttachedToHost(mHost1));
        assertFalse(extraKey.isAttachedToAnyHost(mFoo));
        assertNull(extraKey.retrieveDataFromHost(mHost1));
        assertFalse(anotherExtraKey.isAttachedToHost(mHost1));
        assertFalse(anotherExtraKey.isAttachedToAnyHost(mFoo));
        assertNull(anotherExtraKey.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_doubleAttachSingleDetach() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.attachToHost(mHost1, mFoo);
        runUntilIdle();

        // Attaching using the same key and object, so no detachment should have happened.
        mFoo.assertNoDetachedHosts();
        assertTrue(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, Foo.KEY.retrieveDataFromHost(mHost1));

        Foo.KEY.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_doubleAttachDetachFromAllHosts() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.attachToHost(mHost1, mFoo);
        runUntilIdle();

        // Attaching using the same key and object, so no detachment should have happened.
        mFoo.assertNoDetachedHosts();
        assertTrue(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, Foo.KEY.retrieveDataFromHost(mHost1));

        Foo.KEY.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_doubleDetachIsIgnored() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));

        Foo.KEY.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_doubleDetachFromAllHostsIsIgnored() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));

        Foo.KEY.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemMulitpleHosts_attachAndDetach() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.attachToHost(mHost2, mFoo);
        runUntilIdle();

        mFoo.assertNoDetachedHosts();
        assertTrue(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToHost(mHost2));
        assertTrue(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, Foo.KEY.retrieveDataFromHost(mHost1));
        assertEquals(mFoo, Foo.KEY.retrieveDataFromHost(mHost2));

        Foo.KEY.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToHost(mHost2));
        assertTrue(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertEquals(mFoo, Foo.KEY.retrieveDataFromHost(mHost2));

        Foo.KEY.detachFromHost(mHost2);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost2);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToHost(mHost2));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost2));
    }

    @Test
    public void testSingleItemMultipleHosts_attachAndMultipleDetachesAreIgnored() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.attachToHost(mHost2, mFoo);
        Foo.KEY.detachFromHost(mHost1);
        Foo.KEY.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToHost(mHost2));
        assertTrue(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertEquals(mFoo, Foo.KEY.retrieveDataFromHost(mHost2));

        Foo.KEY.detachFromHost(mHost2);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost2);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToHost(mHost2));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost2));

        Foo.KEY.detachFromHost(mHost2);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost2);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToHost(mHost2));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost2));
    }

    @Test
    public void testSingleItemMultipleHosts_attachAndDetachFromAllHosts() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.attachToHost(mHost2, mFoo);
        Foo.KEY.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatchAnyOrder(mHost1, mHost2);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToHost(mHost2));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost2));
    }

    @Test
    public void testSingleItemMultipleHosts_attachAndDoubleDetachFromAllHostsIsIgnored() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.attachToHost(mHost2, mFoo);
        Foo.KEY.detachFromAllHosts(mFoo);
        Foo.KEY.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatchAnyOrder(mHost1, mHost2);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToHost(mHost2));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost2));
    }

    @Test
    public void testSingleItemMultipleHosts_attachAndDetachInSequence() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.detachFromHost(mHost1);
        Foo.KEY.attachToHost(mHost2, mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToHost(mHost2));
        assertTrue(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertEquals(mFoo, Foo.KEY.retrieveDataFromHost(mHost2));

        Foo.KEY.detachFromHost(mHost2);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost2);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToHost(mHost2));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost2));
    }

    @Test
    public void testSingleItemMultipleHosts_attachAndDetachFromAllHostsInSequence() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.detachFromAllHosts(mFoo);
        Foo.KEY.attachToHost(mHost2, mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToHost(mHost2));
        assertTrue(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertEquals(mFoo, Foo.KEY.retrieveDataFromHost(mHost2));

        Foo.KEY.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost2);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToHost(mHost2));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost2));
    }

    @Test
    public void testTwoSimilarItemsSingleHost_attachAndDetach() {
        Foo foo1 = new Foo();
        Foo foo2 = new Foo();

        Foo.KEY.attachToHost(mHost1, foo1);
        runUntilIdle();

        foo1.assertNoDetachedHosts();
        foo2.assertNoDetachedHosts();
        assertTrue(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToAnyHost(foo1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(foo2));
        assertEquals(foo1, Foo.KEY.retrieveDataFromHost(mHost1));

        Foo.KEY.attachToHost(mHost1, foo2);
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertNoDetachedHosts();
        assertTrue(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(foo1));
        assertTrue(Foo.KEY.isAttachedToAnyHost(foo2));
        assertEquals(foo2, Foo.KEY.retrieveDataFromHost(mHost1));

        Foo.KEY.detachFromHost(mHost1);
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(foo1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(foo2));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testTwoSimilarItemsSingleHost_attachAndDetachInSequence() {
        Foo foo1 = new Foo();
        Foo foo2 = new Foo();

        Foo.KEY.attachToHost(mHost1, foo1);
        runUntilIdle();

        foo1.assertNoDetachedHosts();
        foo2.assertNoDetachedHosts();
        assertTrue(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToAnyHost(foo1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(foo2));
        assertEquals(foo1, Foo.KEY.retrieveDataFromHost(mHost1));

        Foo.KEY.detachFromHost(mHost1);
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertNoDetachedHosts();
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(foo1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(foo2));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));

        Foo.KEY.attachToHost(mHost1, foo2);
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertNoDetachedHosts();
        assertTrue(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(foo1));
        assertTrue(Foo.KEY.isAttachedToAnyHost(foo2));
        assertEquals(foo2, Foo.KEY.retrieveDataFromHost(mHost1));

        Foo.KEY.detachFromHost(mHost1);
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertDetachedHostsMatch(mHost1);
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(foo1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(foo2));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testTwoSimilarItemsSingleHost_attachAndGarbageColletionReturnsNull() {
        Foo foo1 = new Foo();
        Foo foo2 = new Foo();

        Foo.KEY.attachToHost(mHost1, foo1);
        Foo.KEY.attachToHost(mHost1, foo2);

        // Intentionally null out `foo1` to make it eligible for garbage collection.
        foo1 = null;
        forceGC();
        runUntilIdle();

        assertTrue(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToAnyHost(foo2));
        assertEquals(foo2, Foo.KEY.retrieveDataFromHost(mHost1));

        // NOTE: We can not verify anything using the `foo1` variable here, since it has been
        // garbage collected.

        // Intentionally null out `foo2` to make it eligible for garbage collection.
        foo2 = null;
        forceGC();
        runUntilIdle();

        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));

        // NOTE: We can not verify anything using the `foo2` variable here, since it has been
        // garbage collected.
    }

    @Test
    public void testTwoSimilarItemsMultipleHosts_destroyOnlyDetachesFromOneHost() {
        Foo foo1 = new Foo();
        Foo foo2 = new Foo();

        Foo.KEY.attachToHost(mHost1, foo1);
        Foo.KEY.attachToHost(mHost1, foo2);
        Foo.KEY.attachToHost(mHost2, foo2);
        Foo.KEY.attachToHost(mHost2, foo1);
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertDetachedHostsMatch(mHost2);
        assertEquals(foo2, Foo.KEY.retrieveDataFromHost(mHost1));
        assertEquals(foo1, Foo.KEY.retrieveDataFromHost(mHost2));

        mHost1.destroy();
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertDetachedHostsMatch(mHost2, mHost1);
        assertTrue(mHost1.isDestroyed());
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToHost(mHost2));
        assertEquals(foo1, Foo.KEY.retrieveDataFromHost(mHost2));

        mHost2.destroy();
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1, mHost2);
        foo2.assertDetachedHostsMatch(mHost2, mHost1);
        assertTrue(mHost2.isDestroyed());
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToHost(mHost2));
    }

    @Test
    public void
            testTwoSimilarItemsMultipleHosts_destroyShouldOnlyRemoveFromCurrentHostWithMultipleKeys() {
        Foo foo1 = new Foo();
        Foo foo2 = new Foo();

        UnownedUserDataKey<Foo> foo1key = new UnownedUserDataKey<>(Foo.class);
        UnownedUserDataKey<Foo> foo2key = new UnownedUserDataKey<>(Foo.class);

        foo1key.attachToHost(mHost1, foo1);
        foo2key.attachToHost(mHost1, foo2);
        runUntilIdle();

        foo1.assertNoDetachedHosts();
        foo2.assertNoDetachedHosts();
        assertTrue(foo1key.isAttachedToHost(mHost1));
        assertTrue(foo2key.isAttachedToHost(mHost1));
        assertTrue(foo1key.isAttachedToAnyHost(foo1));
        assertTrue(foo2key.isAttachedToAnyHost(foo2));
        assertEquals(foo1, foo1key.retrieveDataFromHost(mHost1));
        assertEquals(foo2, foo2key.retrieveDataFromHost(mHost1));

        // Since `foo1` is attached through `foo1key` and `foo2` is attached through `foo2key`, it
        // should not be possible to look up whether an object not attached through is own key is
        // attached to any host.
        assertFalse(foo1key.isAttachedToAnyHost(foo2));
        assertFalse(foo2key.isAttachedToAnyHost(foo1));

        foo1key.attachToHost(mHost2, foo1);
        foo2key.attachToHost(mHost2, foo2);
        runUntilIdle();

        foo1.assertNoDetachedHosts();
        foo2.assertNoDetachedHosts();
        assertEquals(foo1, foo1key.retrieveDataFromHost(mHost2));
        assertEquals(foo2, foo2key.retrieveDataFromHost(mHost2));

        mHost1.destroy();
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertDetachedHostsMatch(mHost1);
        assertTrue(mHost1.isDestroyed());
        assertFalse(foo1key.isAttachedToHost(mHost1));
        assertFalse(foo2key.isAttachedToHost(mHost1));
        assertTrue(foo1key.isAttachedToHost(mHost2));
        assertTrue(foo2key.isAttachedToHost(mHost2));

        mHost2.destroy();
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1, mHost2);
        foo2.assertDetachedHostsMatch(mHost1, mHost2);
        assertTrue(mHost2.isDestroyed());
        assertFalse(foo1key.isAttachedToHost(mHost1));
        assertFalse(foo2key.isAttachedToHost(mHost1));
        assertFalse(foo1key.isAttachedToHost(mHost2));
        assertFalse(foo2key.isAttachedToHost(mHost2));
    }

    @Test
    public void testTwoDifferentItemsSingleHost_attachAndDetach() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Bar.KEY.attachToHost(mHost1, mBar);
        runUntilIdle();

        mFoo.assertNoDetachedHosts();
        mBar.assertNoDetachedHosts();
        assertTrue(Bar.KEY.isAttachedToHost(mHost1));
        assertTrue(Bar.KEY.isAttachedToAnyHost(mBar));
        assertEquals(mBar, Bar.KEY.retrieveDataFromHost(mHost1));

        Foo.KEY.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        mBar.assertNoDetachedHosts();
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));

        Bar.KEY.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        mBar.assertDetachedHostsMatch(mHost1);
        assertFalse(Bar.KEY.isAttachedToHost(mHost1));
        assertFalse(Bar.KEY.isAttachedToAnyHost(mBar));
        assertNull(Bar.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testTwoDifferentItemsSingleHost_attachAndGarbageCollectionReturnsNull() {
        Foo foo = new Foo();
        Bar bar = new Bar();

        Foo.KEY.attachToHost(mHost1, foo);
        Bar.KEY.attachToHost(mHost1, bar);
        runUntilIdle();

        foo.assertNoDetachedHosts();
        bar.assertNoDetachedHosts();
        assertTrue(Foo.KEY.isAttachedToHost(mHost1));
        assertTrue(Foo.KEY.isAttachedToAnyHost(foo));
        assertEquals(foo, Foo.KEY.retrieveDataFromHost(mHost1));
        assertTrue(Bar.KEY.isAttachedToHost(mHost1));
        assertTrue(Bar.KEY.isAttachedToAnyHost(bar));
        assertEquals(bar, Bar.KEY.retrieveDataFromHost(mHost1));

        // Intentionally null out `foo` to make it eligible for garbage collection.
        foo = null;
        forceGC();
        runUntilIdle();

        bar.assertNoDetachedHosts();
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertTrue(Bar.KEY.isAttachedToHost(mHost1));
        assertTrue(Bar.KEY.isAttachedToAnyHost(bar));
        assertEquals(bar, Bar.KEY.retrieveDataFromHost(mHost1));

        // NOTE: We can not verify anything using the `foo` variable here, since it has been
        // garbage collected.

        // Intentionally null out `bar` to make it eligible for garbage collection.
        bar = null;
        forceGC();
        runUntilIdle();

        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertFalse(Bar.KEY.isAttachedToHost(mHost1));
        assertNull(Bar.KEY.retrieveDataFromHost(mHost1));

        // NOTE: We can not verify anything using the `bar` variable here, since it has been
        // garbage collected.
    }

    @Test
    public void testTwoDifferentItemsSingleHost_destroyWithMultipleEntriesLeft() {
        Foo.KEY.attachToHost(mHost1, mFoo);
        Bar.KEY.attachToHost(mHost1, mBar);

        // Since destruction happens by iterating over all entries and letting themselves detach
        // which results in removing themselves from the map, ensure that there are no issues with
        // concurrent modifications during the iteration over the map.
        mHost1.destroy();
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        mBar.assertDetachedHostsMatch(mHost1);
        assertTrue(mHost1.isDestroyed());
        assertFalse(Foo.KEY.isAttachedToHost(mHost1));
        assertFalse(Foo.KEY.isAttachedToAnyHost(mFoo));
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));
        assertFalse(Bar.KEY.isAttachedToHost(mHost1));
        assertFalse(Bar.KEY.isAttachedToAnyHost(mBar));
        assertNull(Bar.KEY.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleThreadPolicy() throws Exception {
        Foo.KEY.attachToHost(mHost1, mFoo);

        FutureTask<Void> getTask =
                new FutureTask<>(
                        () -> assertAsserts(() -> Foo.KEY.retrieveDataFromHost(mHost1)), null);
        PostTask.postTask(TaskTraits.USER_VISIBLE, getTask);
        getTask.get();

        // Manual cleanup to ensure we can verify host map size during tear down.
        Foo.KEY.detachFromAllHosts(mFoo);
    }

    @Test
    public void testNullKeyOrDataShouldBeDisallowed() {
        assertThrows(NullPointerException.class, () -> Foo.KEY.attachToHost(null, null));
        assertThrows(NullPointerException.class, () -> Foo.KEY.attachToHost(mHost1, null));
        assertThrows(NullPointerException.class, () -> Foo.KEY.attachToHost(null, mFoo));

        // Need a non-empty registry to avoid no-op.
        Foo.KEY.attachToHost(mHost1, mFoo);
        assertThrows(NullPointerException.class, () -> Foo.KEY.retrieveDataFromHost(null));

        assertThrows(NullPointerException.class, () -> Foo.KEY.detachFromHost(null));
        assertThrows(NullPointerException.class, () -> Foo.KEY.detachFromAllHosts(null));
        Foo.KEY.detachFromAllHosts(mFoo);
    }

    @Test
    public void testHost_operationsDisallowedAfterDestroy() {
        Foo.KEY.attachToHost(mHost1, mFoo);

        mHost1.destroy();
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertTrue(mHost1.isDestroyed());

        assertThrows(AssertionError.class, () -> Foo.KEY.attachToHost(mHost1, mFoo));

        // The following operation gracefully returns null.
        assertNull(Foo.KEY.retrieveDataFromHost(mHost1));

        // The following operations gracefully ignores the invocation.
        Foo.KEY.detachFromHost(mHost1);
        Foo.KEY.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
    }

    @Test
    public void testHost_garbageCollection() {
        UnownedUserDataHost extraHost =
                new UnownedUserDataHost(new Handler(Looper.getMainLooper()));

        Foo.KEY.attachToHost(mHost1, mFoo);
        Foo.KEY.attachToHost(extraHost, mFoo);

        // Intentionally null out `host` to make it eligible for garbage collection.
        extraHost = null;
        forceGC();

        // Should not fail to retrieve the object.
        assertTrue(Foo.KEY.isAttachedToHost(mHost1));
        assertEquals(mFoo, Foo.KEY.retrieveDataFromHost(mHost1));
        // There should now only be 1 host attachment left after the retrieval.
        assertEquals(1, Foo.KEY.getHostAttachmentCount(mFoo));

        // NOTE: We can not verify anything using the `extraHost` variable here, since it has been
        // garbage collected.

        // Manual cleanup to ensure we can verify host map size during tear down.
        Foo.KEY.detachFromAllHosts(mFoo);
    }

    private <E extends Throwable> void assertThrows(Class<E> exceptionType, Runnable runnable) {
        Throwable actualException = null;
        try {
            runnable.run();
        } catch (Throwable e) {
            actualException = e;
        }
        assertNotNull("Exception not thrown", actualException);
        assertEquals(exceptionType, actualException.getClass());
    }

    private void assertAsserts(Runnable runnable) {
        // When DCHECK is off, asserts are stripped.
        if (!BuildConfig.ENABLE_ASSERTS) return;

        try {
            runnable.run();
            throw new RuntimeException("Assertion should fail.");
        } catch (AssertionError e) {
            // Ignore. We expect this to happen.
        }
    }

    private static void runUntilIdle() {
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }
}
