// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link SyncOneshotSupplierImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SyncOneshotSupplierImplTest {
    private SyncOneshotSupplierImpl<Integer> mSupplier = new SyncOneshotSupplierImpl<>();

    private AtomicInteger mValue1 = new AtomicInteger();
    private AtomicInteger mValue2 = new AtomicInteger();

    @Test
    @SmallTest
    public void testGet() {
        final int expectedValue = 5;
        assertNull(mSupplier.get());
        mSupplier.set(expectedValue);
        assertEquals(expectedValue, (int) mSupplier.get());
    }

    @Test
    @SmallTest
    public void testSet() {
        final int expectedValue = 5;
        assertNull(mSupplier.onAvailable(mValue1::set));
        assertNull(mSupplier.onAvailable(mValue2::set));

        assertEquals(0, mValue1.get());
        assertEquals(0, mValue2.get());

        mSupplier.set(expectedValue);

        assertEquals(expectedValue, mValue1.get());
        assertEquals(expectedValue, mValue2.get());
    }

    @Test
    @SmallTest
    public void testSetBeforeOnAvailable() {
        final int expectedValue = 10;
        mSupplier.set(expectedValue);

        assertEquals(expectedValue, (int) mSupplier.onAvailable(mValue1::set));
        assertEquals(expectedValue, (int) mSupplier.onAvailable(mValue2::set));

        assertEquals(expectedValue, mValue1.get());
        assertEquals(expectedValue, mValue2.get());
    }

    @Test
    @SmallTest
    public void testSetInterleaved() {
        final int expectedValue = 20;
        assertNull(mSupplier.onAvailable(mValue1::set));

        mSupplier.set(expectedValue);
        assertEquals(expectedValue, mValue1.get());

        assertEquals(expectedValue, (int) mSupplier.onAvailable(mValue2::set));

        assertEquals(expectedValue, mValue1.get());
        assertEquals(expectedValue, mValue2.get());
    }
}
