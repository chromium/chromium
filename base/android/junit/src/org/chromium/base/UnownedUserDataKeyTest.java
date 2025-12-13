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
import org.chromium.base.test.BaseRobolectricTestRule;
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

    private static class TestUnownedUserData {
        private final List<UnownedUserDataHost> mDetachedHosts = new ArrayList<>();

        private static void onDetachedFromHost(TestUnownedUserData self, UnownedUserDataHost host) {
            self.mDetachedHosts.add(host);
        }

        public void assertDetachedHostsMatch(UnownedUserDataHost... hosts) {
            assertEquals(mDetachedHosts.size(), hosts.length);
            assertArrayEquals(mDetachedHosts.toArray(), hosts);
        }

        /**
         * Use this helper assert only when order of detachments can not be known, such as on
         * invocations of {@link UnownedUserDataKey#detachFromAllHosts}.
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

    private static final UnownedUserDataKey<TestUnownedUserData> KEY1 =
            new UnownedUserDataKey<>(TestUnownedUserData::onDetachedFromHost);

    private static final UnownedUserDataKey<TestUnownedUserData> KEY2 =
            new UnownedUserDataKey<>(TestUnownedUserData::onDetachedFromHost);

    private final TestUnownedUserData mFoo = new TestUnownedUserData();
    private final TestUnownedUserData mBar = new TestUnownedUserData();

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

    @Test
    public void testKeyEquality() {
        assertNotEquals(KEY1, KEY2);
    }

    @Test
    public void testSingleItemSingleHost_retrievalReturnsNullBeforeAttachment() {
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_attachAndDetach() {
        KEY1.attachToHost(mHost1, mFoo);

        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, KEY1.retrieveDataFromHost(mHost1));

        KEY1.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_attachAndGarbageCollectionReturnsNull() {
        TestUnownedUserData foo = new TestUnownedUserData();
        KEY1.attachToHost(mHost1, foo);

        // Intentionally null out `foo` to make it eligible for garbage collection.
        foo = null;
        forceGC();
        runUntilIdle();

        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertNull(KEY1.retrieveDataFromHost(mHost1));

        // NOTE: We can not verify anything using the `foo` variable here, since it has been
        // garbage collected.
    }

    @Test
    public void testSingleItemSingleHost_attachAndDetachFromAllHosts() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY1.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_differentKeys() {
        UnownedUserDataKey<TestUnownedUserData> extraKey =
                new UnownedUserDataKey<>(TestUnownedUserData::onDetachedFromHost);
        UnownedUserDataKey<TestUnownedUserData> anotherExtraKey =
                new UnownedUserDataKey<>(TestUnownedUserData::onDetachedFromHost);

        KEY1.attachToHost(mHost1, mFoo);
        extraKey.attachToHost(mHost1, mFoo);
        anotherExtraKey.attachToHost(mHost1, mFoo);
        runUntilIdle();

        mFoo.assertNoDetachedHosts();
        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, KEY1.retrieveDataFromHost(mHost1));
        assertTrue(extraKey.isAttachedToHost(mHost1));
        assertTrue(extraKey.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, extraKey.retrieveDataFromHost(mHost1));
        assertTrue(anotherExtraKey.isAttachedToHost(mHost1));
        assertTrue(anotherExtraKey.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, anotherExtraKey.retrieveDataFromHost(mHost1));

        KEY1.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertTrue(extraKey.isAttachedToHost(mHost1));
        assertTrue(extraKey.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, extraKey.retrieveDataFromHost(mHost1));
        assertTrue(anotherExtraKey.isAttachedToHost(mHost1));
        assertTrue(anotherExtraKey.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, anotherExtraKey.retrieveDataFromHost(mHost1));

        extraKey.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertFalse(extraKey.isAttachedToHost(mHost1));
        assertFalse(extraKey.isAttachedToAnyHost(mFoo));
        assertNull(extraKey.retrieveDataFromHost(mHost1));
        assertTrue(anotherExtraKey.isAttachedToHost(mHost1));
        assertTrue(anotherExtraKey.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, anotherExtraKey.retrieveDataFromHost(mHost1));

        anotherExtraKey.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost1, mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertFalse(extraKey.isAttachedToHost(mHost1));
        assertFalse(extraKey.isAttachedToAnyHost(mFoo));
        assertNull(extraKey.retrieveDataFromHost(mHost1));
        assertFalse(anotherExtraKey.isAttachedToHost(mHost1));
        assertFalse(anotherExtraKey.isAttachedToAnyHost(mFoo));
        assertNull(anotherExtraKey.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_doubleAttachSingleDetach() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY1.attachToHost(mHost1, mFoo);
        runUntilIdle();

        // Attaching using the same key and object, so no detachment should have happened.
        mFoo.assertNoDetachedHosts();
        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, KEY1.retrieveDataFromHost(mHost1));

        KEY1.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_doubleAttachDetachFromAllHosts() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY1.attachToHost(mHost1, mFoo);
        runUntilIdle();

        // Attaching using the same key and object, so no detachment should have happened.
        mFoo.assertNoDetachedHosts();
        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, KEY1.retrieveDataFromHost(mHost1));

        KEY1.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_doubleDetachIsIgnored() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY1.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));

        KEY1.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemSingleHost_doubleDetachFromAllHostsIsIgnored() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY1.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));

        KEY1.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleItemMulitpleHosts_attachAndDetach() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY1.attachToHost(mHost2, mFoo);
        runUntilIdle();

        mFoo.assertNoDetachedHosts();
        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToHost(mHost2));
        assertTrue(KEY1.isAttachedToAnyHost(mFoo));
        assertEquals(mFoo, KEY1.retrieveDataFromHost(mHost1));
        assertEquals(mFoo, KEY1.retrieveDataFromHost(mHost2));

        KEY1.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToHost(mHost2));
        assertTrue(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertEquals(mFoo, KEY1.retrieveDataFromHost(mHost2));

        KEY1.detachFromHost(mHost2);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost2);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToHost(mHost2));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertNull(KEY1.retrieveDataFromHost(mHost2));
    }

    @Test
    public void testSingleItemMultipleHosts_attachAndMultipleDetachesAreIgnored() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY1.attachToHost(mHost2, mFoo);
        KEY1.detachFromHost(mHost1);
        KEY1.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToHost(mHost2));
        assertTrue(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertEquals(mFoo, KEY1.retrieveDataFromHost(mHost2));

        KEY1.detachFromHost(mHost2);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost2);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToHost(mHost2));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertNull(KEY1.retrieveDataFromHost(mHost2));

        KEY1.detachFromHost(mHost2);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost2);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToHost(mHost2));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertNull(KEY1.retrieveDataFromHost(mHost2));
    }

    @Test
    public void testSingleItemMultipleHosts_attachAndDetachFromAllHosts() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY1.attachToHost(mHost2, mFoo);
        KEY1.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatchAnyOrder(mHost1, mHost2);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToHost(mHost2));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertNull(KEY1.retrieveDataFromHost(mHost2));
    }

    @Test
    public void testSingleItemMultipleHosts_attachAndDoubleDetachFromAllHostsIsIgnored() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY1.attachToHost(mHost2, mFoo);
        KEY1.detachFromAllHosts(mFoo);
        KEY1.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatchAnyOrder(mHost1, mHost2);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToHost(mHost2));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertNull(KEY1.retrieveDataFromHost(mHost2));
    }

    @Test
    public void testSingleItemMultipleHosts_attachAndDetachInSequence() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY1.detachFromHost(mHost1);
        KEY1.attachToHost(mHost2, mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToHost(mHost2));
        assertTrue(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertEquals(mFoo, KEY1.retrieveDataFromHost(mHost2));

        KEY1.detachFromHost(mHost2);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost2);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToHost(mHost2));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertNull(KEY1.retrieveDataFromHost(mHost2));
    }

    @Test
    public void testSingleItemMultipleHosts_attachAndDetachFromAllHostsInSequence() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY1.detachFromAllHosts(mFoo);
        KEY1.attachToHost(mHost2, mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToHost(mHost2));
        assertTrue(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertEquals(mFoo, KEY1.retrieveDataFromHost(mHost2));

        KEY1.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1, mHost2);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToHost(mHost2));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertNull(KEY1.retrieveDataFromHost(mHost2));
    }

    @Test
    public void testTwoSimilarItemsSingleHost_attachAndDetach() {
        TestUnownedUserData foo1 = new TestUnownedUserData();
        TestUnownedUserData foo2 = new TestUnownedUserData();

        KEY1.attachToHost(mHost1, foo1);
        runUntilIdle();

        foo1.assertNoDetachedHosts();
        foo2.assertNoDetachedHosts();
        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToAnyHost(foo1));
        assertFalse(KEY1.isAttachedToAnyHost(foo2));
        assertEquals(foo1, KEY1.retrieveDataFromHost(mHost1));

        KEY1.attachToHost(mHost1, foo2);
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertNoDetachedHosts();
        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(foo1));
        assertTrue(KEY1.isAttachedToAnyHost(foo2));
        assertEquals(foo2, KEY1.retrieveDataFromHost(mHost1));

        KEY1.detachFromHost(mHost1);
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(foo1));
        assertFalse(KEY1.isAttachedToAnyHost(foo2));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testTwoSimilarItemsSingleHost_attachAndDetachInSequence() {
        TestUnownedUserData foo1 = new TestUnownedUserData();
        TestUnownedUserData foo2 = new TestUnownedUserData();

        KEY1.attachToHost(mHost1, foo1);
        runUntilIdle();

        foo1.assertNoDetachedHosts();
        foo2.assertNoDetachedHosts();
        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToAnyHost(foo1));
        assertFalse(KEY1.isAttachedToAnyHost(foo2));
        assertEquals(foo1, KEY1.retrieveDataFromHost(mHost1));

        KEY1.detachFromHost(mHost1);
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertNoDetachedHosts();
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(foo1));
        assertFalse(KEY1.isAttachedToAnyHost(foo2));
        assertNull(KEY1.retrieveDataFromHost(mHost1));

        KEY1.attachToHost(mHost1, foo2);
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertNoDetachedHosts();
        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(foo1));
        assertTrue(KEY1.isAttachedToAnyHost(foo2));
        assertEquals(foo2, KEY1.retrieveDataFromHost(mHost1));

        KEY1.detachFromHost(mHost1);
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(foo1));
        assertFalse(KEY1.isAttachedToAnyHost(foo2));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testTwoSimilarItemsSingleHost_attachAndGarbageColletionReturnsNull() {
        TestUnownedUserData foo1 = new TestUnownedUserData();
        TestUnownedUserData foo2 = new TestUnownedUserData();

        KEY1.attachToHost(mHost1, foo1);
        KEY1.attachToHost(mHost1, foo2);

        // Intentionally null out `foo1` to make it eligible for garbage collection.
        foo1 = null;
        forceGC();
        runUntilIdle();

        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToAnyHost(foo2));
        assertEquals(foo2, KEY1.retrieveDataFromHost(mHost1));

        // NOTE: We can not verify anything using the `foo1` variable here, since it has been
        // garbage collected.

        // Intentionally null out `foo2` to make it eligible for garbage collection.
        foo2 = null;
        forceGC();
        runUntilIdle();

        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertNull(KEY1.retrieveDataFromHost(mHost1));

        // NOTE: We can not verify anything using the `foo2` variable here, since it has been
        // garbage collected.
    }

    @Test
    public void testTwoSimilarItemsMultipleHosts_destroyOnlyDetachesFromOneHost() {
        TestUnownedUserData foo1 = new TestUnownedUserData();
        TestUnownedUserData foo2 = new TestUnownedUserData();

        KEY1.attachToHost(mHost1, foo1);
        KEY1.attachToHost(mHost1, foo2);
        KEY1.attachToHost(mHost2, foo2);
        KEY1.attachToHost(mHost2, foo1);
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertDetachedHostsMatch(mHost2);
        assertEquals(foo2, KEY1.retrieveDataFromHost(mHost1));
        assertEquals(foo1, KEY1.retrieveDataFromHost(mHost2));

        mHost1.destroy();
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertDetachedHostsMatch(mHost2, mHost1);
        assertTrue(mHost1.isDestroyed());
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToHost(mHost2));
        assertEquals(foo1, KEY1.retrieveDataFromHost(mHost2));

        mHost2.destroy();
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1, mHost2);
        foo2.assertDetachedHostsMatch(mHost2, mHost1);
        assertTrue(mHost2.isDestroyed());
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToHost(mHost2));
    }

    @Test
    public void
            testTwoSimilarItemsMultipleHosts_destroyShouldOnlyRemoveFromCurrentHostWithMultipleKeys() {
        TestUnownedUserData foo1 = new TestUnownedUserData();
        TestUnownedUserData foo2 = new TestUnownedUserData();

        KEY1.attachToHost(mHost1, foo1);
        KEY2.attachToHost(mHost1, foo2);
        runUntilIdle();

        foo1.assertNoDetachedHosts();
        foo2.assertNoDetachedHosts();
        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY2.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToAnyHost(foo1));
        assertTrue(KEY2.isAttachedToAnyHost(foo2));
        assertEquals(foo1, KEY1.retrieveDataFromHost(mHost1));
        assertEquals(foo2, KEY2.retrieveDataFromHost(mHost1));

        // Since `foo1` is attached through `KEY1` and `foo2` is attached through `KEY2`, it
        // should not be possible to look up whether an object not attached through is own key is
        // attached to any host.
        assertFalse(KEY1.isAttachedToAnyHost(foo2));
        assertFalse(KEY2.isAttachedToAnyHost(foo1));

        KEY1.attachToHost(mHost2, foo1);
        KEY2.attachToHost(mHost2, foo2);
        runUntilIdle();

        foo1.assertNoDetachedHosts();
        foo2.assertNoDetachedHosts();
        assertEquals(foo1, KEY1.retrieveDataFromHost(mHost2));
        assertEquals(foo2, KEY2.retrieveDataFromHost(mHost2));

        mHost1.destroy();
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1);
        foo2.assertDetachedHostsMatch(mHost1);
        assertTrue(mHost1.isDestroyed());
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY2.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToHost(mHost2));
        assertTrue(KEY2.isAttachedToHost(mHost2));

        mHost2.destroy();
        runUntilIdle();

        foo1.assertDetachedHostsMatch(mHost1, mHost2);
        foo2.assertDetachedHostsMatch(mHost1, mHost2);
        assertTrue(mHost2.isDestroyed());
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY2.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToHost(mHost2));
        assertFalse(KEY2.isAttachedToHost(mHost2));
    }

    @Test
    public void testTwoDifferentItemsSingleHost_attachAndDetach() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY2.attachToHost(mHost1, mBar);
        runUntilIdle();

        mFoo.assertNoDetachedHosts();
        mBar.assertNoDetachedHosts();
        assertTrue(KEY2.isAttachedToHost(mHost1));
        assertTrue(KEY2.isAttachedToAnyHost(mBar));
        assertEquals(mBar, KEY2.retrieveDataFromHost(mHost1));

        KEY1.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        mBar.assertNoDetachedHosts();
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));

        KEY2.detachFromHost(mHost1);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        mBar.assertDetachedHostsMatch(mHost1);
        assertFalse(KEY2.isAttachedToHost(mHost1));
        assertFalse(KEY2.isAttachedToAnyHost(mBar));
        assertNull(KEY2.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testTwoDifferentItemsSingleHost_attachAndGarbageCollectionReturnsNull() {
        TestUnownedUserData foo = new TestUnownedUserData();
        TestUnownedUserData bar = new TestUnownedUserData();

        KEY1.attachToHost(mHost1, foo);
        KEY2.attachToHost(mHost1, bar);
        runUntilIdle();

        foo.assertNoDetachedHosts();
        bar.assertNoDetachedHosts();
        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertTrue(KEY1.isAttachedToAnyHost(foo));
        assertEquals(foo, KEY1.retrieveDataFromHost(mHost1));
        assertTrue(KEY2.isAttachedToHost(mHost1));
        assertTrue(KEY2.isAttachedToAnyHost(bar));
        assertEquals(bar, KEY2.retrieveDataFromHost(mHost1));

        // Intentionally null out `foo` to make it eligible for garbage collection.
        foo = null;
        forceGC();
        runUntilIdle();

        bar.assertNoDetachedHosts();
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertTrue(KEY2.isAttachedToHost(mHost1));
        assertTrue(KEY2.isAttachedToAnyHost(bar));
        assertEquals(bar, KEY2.retrieveDataFromHost(mHost1));

        // NOTE: We can not verify anything using the `foo` variable here, since it has been
        // garbage collected.

        // Intentionally null out `bar` to make it eligible for garbage collection.
        bar = null;
        forceGC();
        runUntilIdle();

        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertFalse(KEY2.isAttachedToHost(mHost1));
        assertNull(KEY2.retrieveDataFromHost(mHost1));

        // NOTE: We can not verify anything using the `bar` variable here, since it has been
        // garbage collected.
    }

    @Test
    public void testTwoDifferentItemsSingleHost_destroyWithMultipleEntriesLeft() {
        KEY1.attachToHost(mHost1, mFoo);
        KEY2.attachToHost(mHost1, mBar);

        // Since destruction happens by iterating over all entries and letting themselves detach
        // which results in removing themselves from the map, ensure that there are no issues with
        // concurrent modifications during the iteration over the map.
        mHost1.destroy();
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        mBar.assertDetachedHostsMatch(mHost1);
        assertTrue(mHost1.isDestroyed());
        assertFalse(KEY1.isAttachedToHost(mHost1));
        assertFalse(KEY1.isAttachedToAnyHost(mFoo));
        assertNull(KEY1.retrieveDataFromHost(mHost1));
        assertFalse(KEY2.isAttachedToHost(mHost1));
        assertFalse(KEY2.isAttachedToAnyHost(mBar));
        assertNull(KEY2.retrieveDataFromHost(mHost1));
    }

    @Test
    public void testSingleThreadPolicy() throws Exception {
        KEY1.attachToHost(mHost1, mFoo);

        FutureTask<Void> getTask =
                new FutureTask<>(
                        () -> assertAsserts(() -> KEY1.retrieveDataFromHost(mHost1)), null);
        PostTask.postTask(TaskTraits.USER_VISIBLE, getTask);
        BaseRobolectricTestRule.runAllBackgroundAndUi();
        getTask.get();

        // Manual cleanup to ensure we can verify host map size during tear down.
        KEY1.detachFromAllHosts(mFoo);
    }

    @Test
    public void testHost_operationsDisallowedAfterDestroy() {
        KEY1.attachToHost(mHost1, mFoo);

        mHost1.destroy();
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
        assertTrue(mHost1.isDestroyed());

        assertThrows(AssertionError.class, () -> KEY1.attachToHost(mHost1, mFoo));

        // The following operation gracefully returns null.
        assertNull(KEY1.retrieveDataFromHost(mHost1));

        // The following operations gracefully ignores the invocation.
        KEY1.detachFromHost(mHost1);
        KEY1.detachFromAllHosts(mFoo);
        runUntilIdle();

        mFoo.assertDetachedHostsMatch(mHost1);
    }

    @Test
    public void testHost_garbageCollection() {
        UnownedUserDataHost extraHost =
                new UnownedUserDataHost(new Handler(Looper.getMainLooper()));

        KEY1.attachToHost(mHost1, mFoo);
        KEY1.attachToHost(extraHost, mFoo);

        // Intentionally null out `host` to make it eligible for garbage collection.
        extraHost = null;
        forceGC();

        // Should not fail to retrieve the object.
        assertTrue(KEY1.isAttachedToHost(mHost1));
        assertEquals(mFoo, KEY1.retrieveDataFromHost(mHost1));
        // There should now only be 1 host attachment left after the retrieval.
        assertEquals(1, KEY1.getHostAttachmentCount(mFoo));

        // NOTE: We can not verify anything using the `extraHost` variable here, since it has been
        // garbage collected.

        // Manual cleanup to ensure we can verify host map size during tear down.
        KEY1.detachFromAllHosts(mFoo);
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
