// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.lifetime;

import org.jni_zero.JniRawPtr;
import org.jni_zero.JniUniquePtr;
import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.BuildConfig;

import java.util.concurrent.atomic.AtomicReference;

/** junit tests for {@link LifetimeAssert}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LifetimeAssertUnitTest {
    private static class TestClass {
        // Put assert inside of a test class to mirror typical api usage.
        final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    }

    private final Object mLock = new Object();
    private TestClass mTestClass;
    private LifetimeAssert.WrappedReference mTargetRef;
    private boolean mFound;
    private String mHookMessage;

    @Before
    public void setUp() {
        Assume.assumeTrue("Requires asserts", BuildConfig.ENABLE_ASSERTS);
        mTestClass = new TestClass();
        mTargetRef = mTestClass.mLifetimeAssert.mWrapper;
        mFound = false;
        mHookMessage = null;
        LifetimeAssert.sTestHook =
                (ref, msg) -> {
                    if (ref == mTargetRef) {
                        synchronized (mLock) {
                            mFound = true;
                            mHookMessage = msg;
                            mLock.notify();
                        }
                    }
                };
    }

    @After
    public void tearDown() {
        LifetimeAssert.sTestHook = null;
    }

    private void runTest(boolean setSafe) {
        synchronized (mLock) {
            if (setSafe) {
                LifetimeAssert.setSafeToGc(mTestClass.mLifetimeAssert, true);
            }
            // Null out field to make reference GC'able.
            mTestClass = null;
            // Call System.gc() until the background thread notices the reference.
            for (int i = 0; i < 10 && !mFound; ++i) {
                System.gc();
                try {
                    mLock.wait(200);
                } catch (InterruptedException e) {
                }
            }
            Assert.assertTrue(mFound);
            if (setSafe) {
                Assert.assertNull(mHookMessage);
            } else {
                Assert.assertNotNull(mHookMessage);
            }
        }
    }

    @Test
    public void testSafeGc() {
        runTest(true);
    }

    @Test
    public void testUnsafeGc() {
        runTest(false);
    }

    @Test(expected = LifetimeAssert.LifetimeAssertException.class)
    public void testAssertAllInstancesDestroyedForTesting_notSafeToGc() {
        LifetimeAssert.assertAllInstancesDestroyedForTesting();
    }

    @Test
    public void testAssertAllInstancesDestroyedForTesting_safeToGc() {
        LifetimeAssert.setSafeToGc(mTestClass.mLifetimeAssert, true);
        // Should not throw.
        LifetimeAssert.assertAllInstancesDestroyedForTesting();
    }

    @Test
    public void testAssertAllInstancesDestroyedForTesting_resetAfterAssert() {
        try {
            LifetimeAssert.assertAllInstancesDestroyedForTesting();
            Assert.fail();
        } catch (LifetimeAssert.LifetimeAssertException e) {
            // Expected.
        }
        // Should no longer throw.
        LifetimeAssert.assertAllInstancesDestroyedForTesting();
    }

    @Test
    public void testResetForTesting() {
        LifetimeAssert.resetForTesting();
        // Should not throw.
        LifetimeAssert.assertAllInstancesDestroyedForTesting();
    }

    @Test
    public void testSafePointersTrackerFactoryRegistration() {
        LifetimeAssert.setSafeToGc(mTestClass.mLifetimeAssert, true);
        LifetimeAssert.registerSafePointersTrackerFactory();
        JniUniquePtr<?> ptr = JniUniquePtr.createForTesting(0x1234L);
        ptr.destroy();
        JniRawPtr<?> rawPtr = JniRawPtr.createForTesting(0x1234L);
        rawPtr.release();
        LifetimeAssert.assertAllInstancesDestroyedForTesting();
    }

    @Test
    public void testAssertAllInstancesDestroyedForTesting_creationContextInMessage() {
        LifetimeAssert.setSafeToGc(mTestClass.mLifetimeAssert, true);
        Object target = new Object();
        LifetimeAssert asserter = LifetimeAssert.create(target, "passed to Foo#bar parameter baz");
        try {
            LifetimeAssert.assertAllInstancesDestroyedForTesting();
            Assert.fail();
        } catch (LifetimeAssert.LifetimeAssertException e) {
            Assert.assertTrue(
                    e.getMessage(),
                    e.getMessage()
                            .startsWith(
                                    "Object of type java.lang.Object (passed to Foo#bar parameter"
                                            + " baz) was not destroyed"));
        }
        LifetimeAssert.destroy(asserter);
    }

    @Test(expected = LifetimeAssert.LifetimeAssertException.class)
    public void testSafePointersTrackerFactoryRegistration_unclosedLeak() {
        LifetimeAssert.setSafeToGc(mTestClass.mLifetimeAssert, true);
        LifetimeAssert.registerSafePointersTrackerFactory();
        JniUniquePtr.createForTesting(0x1234L);
        LifetimeAssert.assertAllInstancesDestroyedForTesting();
    }

    @Test
    public void testScopedDisable_returnsNullNestedAndRestoresAfterScope() {
        LifetimeAssert.setSafeToGc(mTestClass.mLifetimeAssert, true);

        try (var outer = LifetimeAssert.scopedDisable("outer")) {
            Assert.assertNull(LifetimeAssert.create(new Object()));
            Assert.assertNull(LifetimeAssert.create(new Object(), "context"));
            Assert.assertNull(LifetimeAssert.create(new Object(), /* safeToGc= */ false));
            try (var inner = LifetimeAssert.scopedDisable("inner")) {
                Assert.assertNull(LifetimeAssert.create(new Object()));
            }
            // Still disabled until outer scope closes.
            Assert.assertNull(LifetimeAssert.create(new Object()));
        }

        // Operations on null LifetimeAsserts returned from scopedDisable are safe no-ops.
        LifetimeAssert.assertNotDestroyed(null);
        LifetimeAssert.setSafeToGc(null, true);
        LifetimeAssert.destroy(null);

        // Assertions are re-enabled after exiting the scope.
        LifetimeAssert createdAfterScope = LifetimeAssert.create(new Object());
        Assert.assertNotNull(createdAfterScope);
        LifetimeAssert.destroy(createdAfterScope);
        LifetimeAssert.assertAllInstancesDestroyedForTesting();
    }

    @Test
    public void testScopedDisable_safePointersNotTracked() {
        LifetimeAssert.setSafeToGc(mTestClass.mLifetimeAssert, true);
        LifetimeAssert.registerSafePointersTrackerFactory();

        JniUniquePtr<?> uniquePtr;
        JniRawPtr<?> rawPtr;
        try (var ignored = LifetimeAssert.scopedDisable("long-lived owner")) {
            uniquePtr = JniUniquePtr.createForTesting(0x1234L);
            rawPtr = JniRawPtr.createForTesting(0x5678L);
        }

        // Neither pointer is tracked, so assertAllInstancesDestroyedForTesting() succeeds.
        LifetimeAssert.assertAllInstancesDestroyedForTesting();
        uniquePtr.destroy();
        rawPtr.release();
    }

    @Test
    public void testScopedDisable_appliesAcrossThreads() throws InterruptedException {
        LifetimeAssert.setSafeToGc(mTestClass.mLifetimeAssert, true);
        AtomicReference<LifetimeAssert> otherThreadAssert =
                new AtomicReference<>(mTestClass.mLifetimeAssert);

        try (var ignored = LifetimeAssert.scopedDisable("applies across threads")) {
            Thread other =
                    new Thread(() -> otherThreadAssert.set(LifetimeAssert.create(new Object())));
            other.start();
            other.join();
        }

        Assert.assertNull(otherThreadAssert.get());
        LifetimeAssert.assertAllInstancesDestroyedForTesting();
    }
}
