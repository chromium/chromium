// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link UnwrapObservableSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UnwrapObservableSupplierTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock Callback<Integer> mOnChangeCallback;
    private @Mock Object mObject1;
    private @Mock Object mObject2;

    private static ObservableSupplier<Integer> make(ObservableSupplier<Object> parentSupplier) {
        return new UnwrapObservableSupplier(parentSupplier, UnwrapObservableSupplierTest::unwrap);
    }

    private static Integer unwrap(Object obj) {
        return obj == null ? 0 : obj.hashCode();
    }

    @Test
    public void testGetWithoutObservers() {
        ObservableSupplierImpl<Object> parentSupplier = new ObservableSupplierImpl<>();
        ObservableSupplier<Integer> unwrapSupplier = make(parentSupplier);
        assertEquals(0, unwrapSupplier.get().intValue());
        assertFalse(parentSupplier.hasObservers());

        parentSupplier.set(mObject1);
        assertEquals(mObject1.hashCode(), unwrapSupplier.get().intValue());
        assertFalse(parentSupplier.hasObservers());

        parentSupplier.set(mObject2);
        assertEquals(mObject2.hashCode(), unwrapSupplier.get().intValue());
        assertFalse(parentSupplier.hasObservers());

        parentSupplier.set(null);
        assertEquals(0, unwrapSupplier.get().intValue());
        assertFalse(parentSupplier.hasObservers());
    }

    @Test
    public void testGetWithObserver() {
        ObservableSupplierImpl<Object> parentSupplier = new ObservableSupplierImpl<>();
        ObservableSupplier<Integer> unwrapSupplier = make(parentSupplier);
        unwrapSupplier.addObserver(mOnChangeCallback);

        ShadowLooper.idleMainLooper();
        assertTrue(parentSupplier.hasObservers());
        verify(mOnChangeCallback).onResult(eq(0));

        parentSupplier.set(mObject1);
        verify(mOnChangeCallback).onResult(eq(mObject1.hashCode()));

        parentSupplier.set(mObject2);
        verify(mOnChangeCallback).onResult(eq(mObject2.hashCode()));

        parentSupplier.set(null);
        // times(2) because it was already called with this value once at the start.
        verify(mOnChangeCallback, times(2)).onResult(eq(0));

        unwrapSupplier.removeObserver(mOnChangeCallback);
        assertFalse(parentSupplier.hasObservers());
    }

    @Test
    public void testAlreadyHasValueWhenObserverAdded() {
        ObservableSupplierImpl<Object> parentSupplier = new ObservableSupplierImpl<>(mObject1);
        ObservableSupplier<Integer> unwrapSupplier = make(parentSupplier);

        unwrapSupplier.addObserver(mOnChangeCallback);
        assertTrue(parentSupplier.hasObservers());

        ShadowLooper.idleMainLooper();
        verify(mOnChangeCallback).onResult(eq(mObject1.hashCode()));
    }
}
