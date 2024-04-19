// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link LazyOneshotSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LazyOneshotSupplierTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // TODO(crbug.com/40287492): Switch to @Mock once the default #bind works on mocks.
    @Spy private Callback<Object> mOnAvailable;

    @Test
    public void testFromValueObject() {
        Object foo = new Object();
        LazyOneshotSupplier lazyOneshotSupplier = LazyOneshotSupplier.fromValue(foo);
        lazyOneshotSupplier.onAvailable(mOnAvailable);
        assertFalse(lazyOneshotSupplier.hasValue());

        assertEquals(foo, lazyOneshotSupplier.get());
        assertTrue(lazyOneshotSupplier.hasValue());

        ShadowLooper.idleMainLooper();
        verify(mOnAvailable).onResult(eq(foo));
    }

    @Test
    public void testFromValueNull() {
        LazyOneshotSupplier lazyOneshotSupplier = LazyOneshotSupplier.fromValue(null);
        lazyOneshotSupplier.onAvailable(mOnAvailable);
        assertFalse(lazyOneshotSupplier.hasValue());

        assertNull(lazyOneshotSupplier.get());
        assertTrue(lazyOneshotSupplier.hasValue());

        ShadowLooper.idleMainLooper();
        verify(mOnAvailable).onResult(eq(null));
    }

    @Test
    public void testFromSupplierObject() {
        Object foo = new Object();
        LazyOneshotSupplier lazyOneshotSupplier = LazyOneshotSupplier.fromSupplier(() -> foo);
        lazyOneshotSupplier.onAvailable(mOnAvailable);
        assertFalse(lazyOneshotSupplier.hasValue());

        assertEquals(foo, lazyOneshotSupplier.get());
        assertTrue(lazyOneshotSupplier.hasValue());

        ShadowLooper.idleMainLooper();
        verify(mOnAvailable).onResult(eq(foo));
    }

    @Test
    public void testFromSupplierNull() {
        LazyOneshotSupplier lazyOneshotSupplier = LazyOneshotSupplier.fromSupplier(() -> null);
        lazyOneshotSupplier.onAvailable(mOnAvailable);
        assertFalse(lazyOneshotSupplier.hasValue());

        assertNull(lazyOneshotSupplier.get());
        assertTrue(lazyOneshotSupplier.hasValue());

        ShadowLooper.idleMainLooper();
        verify(mOnAvailable).onResult(eq(null));
    }
}
