// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link ObservableSupplierImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UnownedUserDataSupplierTest {
    /** Serves as an example concrete class for {@link UnownedUserDataSupplier}. */
    static class TestUnownedUserDataSupplier extends UnownedUserDataSupplier<String> {
        private static final UnownedUserDataKey<TestUnownedUserDataSupplier> KEY =
                new UnownedUserDataKey<TestUnownedUserDataSupplier>(
                        TestUnownedUserDataSupplier.class);

        /** Use this pattern to mock the {@link UnownedUserDataSupplier} for testing. */
        private static TestUnownedUserDataSupplier sInstanceForTesting;

        /**
         * Retrieves an {@link ObservableSupplier} from the given host. Real implementations should
         * use {@link WindowAndroid}.
         */
        public static ObservableSupplier<String> from(UnownedUserDataHost host) {
            if (sInstanceForTesting != null) return sInstanceForTesting;
            return KEY.retrieveDataFromHost(host);
        }

        /**
         * Constructs a {@link TestUnownedUserDataSupplier} with the given host. Real
         * implementations should use {@link WindowAndroid} instead.
         */
        public TestUnownedUserDataSupplier(UnownedUserDataHost host) {
            super(KEY, host);
        }

        static UnownedUserDataKey<TestUnownedUserDataSupplier> getUnownedUserDataKeyForTesting() {
            return KEY;
        }

        static void setInstanceForTesting(TestUnownedUserDataSupplier testUnownedUserDataSupplier) {
            sInstanceForTesting = testUnownedUserDataSupplier;
        }
    }

    static final String TEST_STRING_1 = "testString1";
    static final String TEST_STRING_2 = "testString2";

    private final UnownedUserDataHost mHost = new UnownedUserDataHost();
    private final TestUnownedUserDataSupplier mSupplier = new TestUnownedUserDataSupplier(mHost);

    private boolean mIsDestroyed;

    @Before
    public void setUp() {
        mIsDestroyed = false;
    }

    @After
    public void tearDown() {
        if (!mIsDestroyed) {
            mSupplier.destroy();
            mIsDestroyed = true;
        }

        Assert.assertNull(TestUnownedUserDataSupplier.from(mHost));
    }

    @Test
    public void testSet() {
        mSupplier.set(TEST_STRING_1);

        // Simulate client
        ObservableSupplierImpl<String> supplier =
                (ObservableSupplierImpl) TestUnownedUserDataSupplier.from(mHost);
        Assert.assertEquals(TEST_STRING_1, supplier.get());

        Callback<String> callback = Mockito.mock(Callback.class);
        supplier.addObserver(callback);
        supplier.set(TEST_STRING_2);
        Mockito.verify(callback).onResult(TEST_STRING_2);
    }

    @Test
    public void testAttachMultipleSuppliersToSameHost() {
        TestUnownedUserDataSupplier secondarySupplier = new TestUnownedUserDataSupplier(mHost);
        Assert.assertFalse(
                TestUnownedUserDataSupplier.getUnownedUserDataKeyForTesting().isAttachedToAnyHost(
                        mSupplier));
        Assert.assertTrue(
                TestUnownedUserDataSupplier.getUnownedUserDataKeyForTesting().isAttachedToAnyHost(
                        secondarySupplier));
    }

    @Test
    public void testDestroy() {
        mSupplier.destroy();
        Assert.assertNull(TestUnownedUserDataSupplier.from(mHost));
        Assert.assertFalse(
                TestUnownedUserDataSupplier.getUnownedUserDataKeyForTesting().isAttachedToAnyHost(
                        mSupplier));
        mIsDestroyed = true;
    }

    @Test
    public void testDestroy_DoubleDestroy() {
        mSupplier.destroy();
        try {
            mSupplier.destroy();
            throw new Error("Expected an assert to be triggered.");
        } catch (AssertionError e) {
        }
        mIsDestroyed = true;
    }
}