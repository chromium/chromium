// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

/**
 * Test class for {@link UserDataHost}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class UserDataHostTest {
    private final UserDataHost mHost = new UserDataHost();

    private static class TestObjectA implements UserData {
        private boolean mDestroyed;

        @Override
        public void destroy() {
            mDestroyed = true;
        }

        private boolean isDestroyed() {
            return mDestroyed;
        }
    }

    private static class TestObjectB implements UserData {
        private boolean mDestroyed;

        @Override
        public void destroy() {
            mDestroyed = true;
        }

        private boolean isDestroyed() {
            return mDestroyed;
        }
    }

    private <T extends UserData, E extends RuntimeException> void getUserDataException(
            Class<T> key, Class<E> exceptionType) {
        try {
            mHost.getUserData(key);
        } catch (Exception e) {
            if (!exceptionType.isInstance(e)) throw e;
        }
    }

    private <T extends UserData, E extends RuntimeException> void setUserDataException(
            Class<T> key, T obj, Class<E> exceptionType) {
        try {
            mHost.setUserData(key, obj);
        } catch (Exception e) {
            if (!exceptionType.isInstance(e)) throw e;
        }
    }

    private <T extends UserData, E extends RuntimeException> void removeUserDataException(
            Class<T> key, Class<E> exceptionType) {
        try {
            mHost.removeUserData(key);
        } catch (Exception e) {
            if (!exceptionType.isInstance(e)) throw e;
        }
    }

    /**
     * Verifies basic operations.
     */
    @Test
    @SmallTest
    public void testBasicOperations() {
        TestObjectA obj = new TestObjectA();
        mHost.setUserData(TestObjectA.class, obj);
        Assert.assertEquals(obj, mHost.getUserData(TestObjectA.class));
        Assert.assertEquals(obj, mHost.removeUserData(TestObjectA.class));
        Assert.assertNull(mHost.getUserData(TestObjectA.class));
        removeUserDataException(TestObjectA.class, IllegalStateException.class);
    }

    /**
     * Verifies nulled key or data are not allowed.
     */
    @Test
    @SmallTest
    public void testNullKeyOrDataAreDisallowed() {
        TestObjectA obj = new TestObjectA();
        setUserDataException(null, null, IllegalArgumentException.class);
        setUserDataException(TestObjectA.class, null, IllegalArgumentException.class);
        setUserDataException(null, obj, IllegalArgumentException.class);
        getUserDataException(null, IllegalArgumentException.class);
        removeUserDataException(null, IllegalArgumentException.class);
    }

    /**
     * Verifies {@link #setUserData()} overwrites current data.
     */
    @Test
    @SmallTest
    public void testSetUserDataOverwrites() {
        TestObjectA obj1 = new TestObjectA();
        mHost.setUserData(TestObjectA.class, obj1);
        Assert.assertEquals(obj1, mHost.getUserData(TestObjectA.class));

        TestObjectA obj2 = new TestObjectA();
        mHost.setUserData(TestObjectA.class, obj2);
        Assert.assertEquals(obj2, mHost.getUserData(TestObjectA.class));
    }

    /**
     * Verifies operation on a different thread is not allowed.
     */
    @Test
    @SmallTest
    public void testSingleThreadPolicy() {
        TestObjectA obj = new TestObjectA();
        mHost.setUserData(TestObjectA.class, obj);
        ThreadUtils.runOnUiThreadBlocking(
                () -> getUserDataException(TestObjectA.class, IllegalStateException.class));
    }

    /**
     * Verifies {@link UserHostData#destroy()} detroyes each {@link UserData} object.
     */
    @Test
    @SmallTest
    public void testDestroy() {
        TestObjectA objA = new TestObjectA();
        TestObjectB objB = new TestObjectB();
        mHost.setUserData(TestObjectA.class, objA);
        mHost.setUserData(TestObjectB.class, objB);
        Assert.assertEquals(objA, mHost.getUserData(TestObjectA.class));
        Assert.assertEquals(objB, mHost.getUserData(TestObjectB.class));

        mHost.destroy();
        Assert.assertTrue(objA.isDestroyed());
        Assert.assertTrue(objB.isDestroyed());
    }

    /**
     * Verifies that no operation is allowed after {@link #destroy()} is called.
     */
    @Test
    @SmallTest
    public void testOperationsDisallowedAfterDestroy() {
        TestObjectA obj = new TestObjectA();
        mHost.setUserData(TestObjectA.class, obj);
        Assert.assertEquals(obj, mHost.getUserData(TestObjectA.class));

        mHost.destroy();
        getUserDataException(TestObjectA.class, IllegalStateException.class);
        setUserDataException(TestObjectA.class, obj, IllegalStateException.class);
        removeUserDataException(TestObjectA.class, IllegalStateException.class);
    }
}
