// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import android.os.Handler;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.lifecycle.Destroyable;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Unit tests for {@link ObservableSupplierImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ObservableSupplierImplTest {
    private static final String TEST_STRING_1 = "Test";
    private static final String TEST_STRING_2 = "Test2";

    private int mCallCount;
    private String mLastSuppliedString;
    private ObservableSupplierImpl<String> mSupplier = new ObservableSupplierImpl<>();

    @Test
    public void testObserverNotification_SetMultiple() {
        Callback<String> supplierObserver = result -> {
            mCallCount++;
            mLastSuppliedString = result;
        };

        mSupplier.addObserver(supplierObserver);
        checkState(0, null, null, "before setting first string.");

        mSupplier.set(TEST_STRING_1);
        checkState(1, TEST_STRING_1, TEST_STRING_1, "after setting first string.");

        mSupplier.set(TEST_STRING_2);
        checkState(2, TEST_STRING_2, TEST_STRING_2, "after setting second string.");

        mSupplier.set(null);
        checkState(3, null, null, "after setting third string.");
    }

    @Test
    public void testObserverNotification_SetSame() {
        Callback<String> supplierObserver = result -> {
            mCallCount++;
            mLastSuppliedString = result;
        };

        mSupplier.addObserver(supplierObserver);
        checkState(0, null, null, "before setting first string.");

        mSupplier.set(TEST_STRING_1);
        checkState(1, TEST_STRING_1, TEST_STRING_1, "after setting first string.");

        mSupplier.set(TEST_STRING_1);
        checkState(1, TEST_STRING_1, TEST_STRING_1, "after resetting first string.");
    }

    @Test
    public void testObserverNotification_RemoveObserver() {
        Callback<String> supplierObserver = result -> {
            mCallCount++;
            mLastSuppliedString = result;
        };

        mSupplier.addObserver(supplierObserver);
        checkState(0, null, null, "before setting first string.");

        mSupplier.set(TEST_STRING_1);
        checkState(1, TEST_STRING_1, TEST_STRING_1, "after setting first string.");

        mSupplier.removeObserver(supplierObserver);

        mSupplier.set(TEST_STRING_2);
        checkState(1, TEST_STRING_1, TEST_STRING_2, "after setting second string.");
    }

    @Test
    public void testObserverNotification_RegisterObserverAfterSet() {
        Handler handler = new Handler();
        handler.post(() -> {
            mSupplier.set(TEST_STRING_1);
            checkState(0, null, TEST_STRING_1, "after setting first string.");

            Callback<String> supplierObserver = new Callback<String>() {
                @Override
                public void onResult(String result) {
                    mCallCount++;
                    mLastSuppliedString = result;
                }
            };

            mSupplier.addObserver(supplierObserver);

            checkState(0, null, TEST_STRING_1, "after setting observer.");
        });

        handler.post(() -> checkState(1, TEST_STRING_1, TEST_STRING_1, "in second message loop."));
    }

    @Test
    public void testObserverNotification_RegisterObserverAfterSetThenSetAgain() {
        Handler handler = new Handler();
        handler.post(() -> {
            mSupplier.set(TEST_STRING_1);
            checkState(0, null, TEST_STRING_1, "after setting first string.");

            Callback<String> supplierObserver = new Callback<String>() {
                @Override
                public void onResult(String result) {
                    mCallCount++;
                    mLastSuppliedString = result;
                }
            };

            mSupplier.addObserver(supplierObserver);

            checkState(0, null, TEST_STRING_1, "after setting observer.");

            mSupplier.set(TEST_STRING_2);
            checkState(1, TEST_STRING_2, TEST_STRING_2, "after setting second string.");
        });

        handler.post(() -> checkState(1, TEST_STRING_2, TEST_STRING_2, "in second message loop."));
    }

    @Test
    public void testObserverNotification_RegisterObserverAfterSetThenRemove() {
        Handler handler = new Handler();
        handler.post(() -> {
            mSupplier.set(TEST_STRING_1);
            checkState(0, null, TEST_STRING_1, "after setting first string.");

            Callback<String> supplierObserver = new Callback<String>() {
                @Override
                public void onResult(String result) {
                    mCallCount++;
                    mLastSuppliedString = result;
                }
            };

            mSupplier.addObserver(supplierObserver);

            checkState(0, null, TEST_STRING_1, "after setting observer.");

            mSupplier.removeObserver(supplierObserver);
        });

        handler.post(() -> checkState(0, null, TEST_STRING_1, "in second message loop."));
    }

    @Test
    public void testObserverNotification_RemoveObserverInsideCallback() {
        Callback<String> supplierObserver = new Callback<String>() {
            @Override
            public void onResult(String result) {
                mCallCount++;
                mLastSuppliedString = result;
                mSupplier.removeObserver(this);
            }
        };

        mSupplier.addObserver(supplierObserver);
        checkState(0, null, null, "before setting first string.");

        mSupplier.set(TEST_STRING_1);
        checkState(1, TEST_STRING_1, TEST_STRING_1, "after setting first string.");

        mSupplier.set(TEST_STRING_2);
        checkState(1, TEST_STRING_1, TEST_STRING_2, "after setting second string.");
    }

    @Test(expected = IllegalStateException.class)
    public void testDestruction_accessPostDestroy_hasValue() {
        mSupplier.destroy();
        mSupplier.hasValue();
    }

    @Test(expected = IllegalStateException.class)
    public void testDestruction_accessPostDestroy_set() {
        mSupplier.destroy();
        mSupplier.set(TEST_STRING_1);
    }

    @Test(expected = IllegalStateException.class)
    public void testDestruction_accessPostDestroy_get() {
        mSupplier.set(TEST_STRING_1);
        mSupplier.destroy();
        mSupplier.get();
    }

    @Test(expected = IllegalStateException.class)
    public void testDestruction_accessPostDestroy_addObserver() {
        mSupplier.set(TEST_STRING_1);
        mSupplier.destroy();
        mSupplier.addObserver((String s) -> {});
    }

    @Test(expected = IllegalStateException.class)
    public void testDestruction_accessPostDestroy_removeObserver() {
        mSupplier.set(TEST_STRING_1);
        Callback<String> observer = (String s) -> {};
        mSupplier.addObserver(observer);
        mSupplier.destroy();
        mSupplier.removeObserver(observer);
    }

    @Test(expected = IllegalStateException.class)
    public void testDestruction_accessPostDestroy_destroy() {
        mSupplier.set(TEST_STRING_1);
        mSupplier.destroy();
        mSupplier.destroy();
    }

    @Test
    public void testDestruction_withNonDestroyable() {
        final AtomicBoolean suppliedObjectIsDestroyed = new AtomicBoolean(false);
        Object destroyable = new Object() {
            public void destroy() {
                suppliedObjectIsDestroyed.set(true);
            }
        };

        ObservableSupplierImpl<Object> supplier = new ObservableSupplierImpl<>();
        supplier.set(destroyable);
        Assert.assertTrue(supplier.hasValue());
        supplier.destroy();
        Assert.assertFalse(suppliedObjectIsDestroyed.get());
    }

    @Test
    public void testDestruction_withDestroyable() {
        final AtomicBoolean suppliedObjectIsDestroyed = new AtomicBoolean(false);
        Destroyable destroyable = new Destroyable() {
            @Override
            public void destroy() {
                suppliedObjectIsDestroyed.set(true);
            }
        };

        ObservableSupplierImpl<Destroyable> supplier = new ObservableSupplierImpl<>();
        supplier.set(destroyable);
        Assert.assertTrue(supplier.hasValue());
        supplier.destroy();
        Assert.assertTrue(suppliedObjectIsDestroyed.get());
    }

    private void checkState(int expectedCallCount, String expectedLastSuppliedString,
            String expectedStringFromGet, String assertDescription) {
        Assert.assertEquals(
                "Incorrect call count " + assertDescription, expectedCallCount, mCallCount);
        Assert.assertEquals("Incorrect last supplied string " + assertDescription,
                expectedLastSuppliedString, mLastSuppliedString);
        Assert.assertEquals(
                "Incorrect #get() " + assertDescription, expectedStringFromGet, mSupplier.get());
    }
}
