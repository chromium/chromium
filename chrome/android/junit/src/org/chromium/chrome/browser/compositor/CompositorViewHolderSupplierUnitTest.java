// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link CompositorViewHolderSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CompositorViewHolderSupplierUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private CompositorViewHolder mCompositorViewHolder;

    private UnownedUserDataHost mUnownedUserDataHost;

    @Before
    public void setUp() {
        mUnownedUserDataHost = new UnownedUserDataHost();
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUnownedUserDataHost);
    }

    @Test
    public void testAttachAndGet() {
        SettableMonotonicObservableSupplier<CompositorViewHolder> supplier =
                ObservableSuppliers.createMonotonic();
        CompositorViewHolderSupplier.attach(mUnownedUserDataHost, supplier);
        assertEquals(supplier, CompositorViewHolderSupplier.from(mWindowAndroid));
        assertNull(CompositorViewHolderSupplier.getValueOrNullFrom(mWindowAndroid));

        supplier.set(mCompositorViewHolder);
        assertEquals(
                mCompositorViewHolder,
                CompositorViewHolderSupplier.getValueOrNullFrom(mWindowAndroid));
    }

    @Test
    public void testDestroy() {
        SettableMonotonicObservableSupplier<CompositorViewHolder> supplier =
                ObservableSuppliers.createMonotonic();
        CompositorViewHolderSupplier.attach(mUnownedUserDataHost, supplier);
        assertEquals(supplier, CompositorViewHolderSupplier.from(mWindowAndroid));

        CompositorViewHolderSupplier.destroy(supplier);
        assertNull(CompositorViewHolderSupplier.from(mWindowAndroid));
        assertNull(CompositorViewHolderSupplier.getValueOrNullFrom(mWindowAndroid));
    }

    @Test
    public void testNullWindowAndroid() {
        assertNull(CompositorViewHolderSupplier.from(/* windowAndroid= */ null));
        assertNull(CompositorViewHolderSupplier.getValueOrNullFrom(/* windowAndroid= */ null));
    }

    @Test
    public void testInstanceForTesting() {
        assertNull(CompositorViewHolderSupplier.getValueOrNullFrom(mWindowAndroid));
        CompositorViewHolderSupplier.setInstanceForTesting(mCompositorViewHolder);
        assertEquals(
                mCompositorViewHolder,
                CompositorViewHolderSupplier.getValueOrNullFrom(mWindowAndroid));
    }
}
