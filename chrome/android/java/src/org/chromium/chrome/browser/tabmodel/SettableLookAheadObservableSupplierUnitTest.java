// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier.NotifyBehavior;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.tab.LookAheadObservableSupplier;

/** Unit tests for {@link LookAheadObservableSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettableLookAheadObservableSupplierUnitTest {
    private static final String SUPPLIER_VALUE = "value";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<String> mLookAheadObserver;
    @Mock private Callback<String> mObserver;

    private SettableLookAheadObservableSupplier<String> mSupplier;

    @Before
    public void setUp() {
        mSupplier = new SettableLookAheadObservableSupplier<>();
    }

    @Test
    public void testSetWithoutWillSet() {
        mSupplier.addLookAheadObserver(mLookAheadObserver);
        mSupplier.addSyncObserverAndPostIfNonNull(mObserver);

        mSupplier.set(SUPPLIER_VALUE);

        verify(mLookAheadObserver).onResult(SUPPLIER_VALUE);
        verify(mObserver).onResult(SUPPLIER_VALUE);
        assertEquals(SUPPLIER_VALUE, mSupplier.get());
    }

    @Test
    public void testSetWithWillSet() {
        mSupplier.addLookAheadObserver(mLookAheadObserver);
        mSupplier.addSyncObserverAndPostIfNonNull(mObserver);

        mSupplier.willSet(SUPPLIER_VALUE);

        verify(mLookAheadObserver).onResult(SUPPLIER_VALUE);
        verify(mObserver, never()).onResult(SUPPLIER_VALUE);
        assertNull(mSupplier.get());

        mSupplier.set(SUPPLIER_VALUE);

        verify(mLookAheadObserver).onResult(SUPPLIER_VALUE);
        verify(mObserver).onResult(SUPPLIER_VALUE);
        assertEquals(SUPPLIER_VALUE, mSupplier.get());
    }

    @Test
    public void testRemoveObservers() {
        mSupplier.addLookAheadObserver(mLookAheadObserver);
        mSupplier.addSyncObserverAndPostIfNonNull(mObserver);

        mSupplier.removeLookAheadObserver(mLookAheadObserver);
        mSupplier.removeObserver(mObserver);

        mSupplier.set(SUPPLIER_VALUE);

        verify(mLookAheadObserver, never()).onResult(SUPPLIER_VALUE);
        verify(mObserver, never()).onResult(SUPPLIER_VALUE);
    }

    @Test
    public void testObserverCount() {
        assertEquals(0, mSupplier.getObserverCount());

        mSupplier.addLookAheadObserver(mLookAheadObserver);
        assertEquals(1, mSupplier.getObserverCount());

        mSupplier.addSyncObserverAndPostIfNonNull(mObserver);
        assertEquals(2, mSupplier.getObserverCount());

        mSupplier.removeLookAheadObserver(mLookAheadObserver);
        assertEquals(1, mSupplier.getObserverCount());

        mSupplier.removeObserver(mObserver);
        assertEquals(0, mSupplier.getObserverCount());
    }

    @Test
    public void testAddLookAheadObserverWithBehavior() {
        mSupplier.set(SUPPLIER_VALUE);

        mSupplier.addLookAheadObserver(mLookAheadObserver, NotifyBehavior.NOTIFY_ON_ADD);

        verify(mLookAheadObserver).onResult(SUPPLIER_VALUE);
    }

    @Test(expected = AssertionError.class)
    public void testSetMismatchThrows() {
        Assume.assumeTrue(BuildConfig.ENABLE_ASSERTS);
        mSupplier.willSet("value1");
        mSupplier.set("value2");
    }

    @Test
    public void testDestroy() {
        mSupplier.addLookAheadObserver(mLookAheadObserver);
        mSupplier.addSyncObserverAndPostIfNonNull(mObserver);

        mSupplier.destroy();
        assertEquals(0, mSupplier.getObserverCount());
    }
}
