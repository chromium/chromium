// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.function.Function;

/** Unit tests for {@link TransitiveObservableSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TransitiveObservableSupplierTest {
    // Shared singleton lambda to satisfy all callers. Type erasure makes it all equivalent at
    // runtime and we still get compile time type safety.
    private static final Function<?, ?> SHARED_TRAMPOLINE = arg -> arg;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock Callback<Object> mOnChangeCallback;
    // While nothing is verified on these, mocks have good toString methods by default.
    private @Mock Object mObject1;
    private @Mock Object mObject2;
    private @Mock Object mObject3;
    private @Mock Object mObject4;

    /**
     * Convenience helper when the parent value needs no unwrapping. These methods should be moved
     * to the implementation file if any client needs it.
     */
    private static <Z> TransitiveObservableSupplier<ObservableSupplier<Z>, Z> make(
            ObservableSupplier<ObservableSupplier<Z>> parentSupplier) {
        return new TransitiveObservableSupplier(parentSupplier, trampoline());
    }

    private static <T> Function<T, T> trampoline() {
        return (Function<T, T>) SHARED_TRAMPOLINE;
    }

    @Test
    public void testGetWithoutObservers() {
        ObservableSupplierImpl<ObservableSupplier<Object>> parentSupplier =
                new ObservableSupplierImpl<>();
        ObservableSupplierImpl<Object> targetSupplier1 = new ObservableSupplierImpl<>();

        ObservableSupplier<Object> transitiveSupplier = make(parentSupplier);
        assertNull(transitiveSupplier.get());

        parentSupplier.set(targetSupplier1);
        assertNull(transitiveSupplier.get());

        targetSupplier1.set(mObject1);
        assertEquals(mObject1, transitiveSupplier.get());

        targetSupplier1.set(null);
        assertNull(transitiveSupplier.get());

        targetSupplier1.set(mObject2);
        assertEquals(mObject2, transitiveSupplier.get());

        parentSupplier.set(null);
        assertNull(transitiveSupplier.get());

        targetSupplier1.set(mObject3);
        assertNull(transitiveSupplier.get());

        parentSupplier.set(targetSupplier1);
        assertEquals(mObject3, transitiveSupplier.get());
    }

    @Test
    public void testGetWithObserver() {
        ObservableSupplierImpl<ObservableSupplier<Object>> parentSupplier =
                new ObservableSupplierImpl<>();
        ObservableSupplierImpl<Object> targetSupplier1 = new ObservableSupplierImpl<>();
        ObservableSupplierImpl<Object> targetSupplier2 = new ObservableSupplierImpl<>();

        ObservableSupplier<Object> transitiveSupplier = make(parentSupplier);
        assertNull(transitiveSupplier.get());

        assertNull(transitiveSupplier.addObserver(mOnChangeCallback));
        verifyNoInteractions(mOnChangeCallback);

        parentSupplier.set(targetSupplier1);
        assertNull(transitiveSupplier.get());
        verifyNoInteractions(mOnChangeCallback);

        targetSupplier1.set(mObject1);
        assertEquals(mObject1, transitiveSupplier.get());
        verify(mOnChangeCallback).onResult(eq(mObject1));

        targetSupplier1.set(mObject2);
        assertEquals(mObject2, transitiveSupplier.get());
        verify(mOnChangeCallback).onResult(eq(mObject2));

        targetSupplier1.set(null);
        assertEquals(null, transitiveSupplier.get());
        verify(mOnChangeCallback).onResult(eq(null));

        targetSupplier2.set(mObject3);
        parentSupplier.set(targetSupplier2);
        assertEquals(mObject3, transitiveSupplier.get());
        verify(mOnChangeCallback).onResult(eq(mObject3));

        transitiveSupplier.removeObserver(mOnChangeCallback);
        targetSupplier2.set(mObject4);
        assertEquals(mObject4, transitiveSupplier.get());
        verify(mOnChangeCallback, never()).onResult(eq(mObject4));
    }

    @Test
    public void testSameObserver() {
        ObservableSupplierImpl<ObservableSupplier<Object>> parentSupplier =
                new ObservableSupplierImpl<>();
        ObservableSupplierImpl<Object> targetSupplier = new ObservableSupplierImpl<>();
        parentSupplier.set(targetSupplier);

        ObservableSupplier<Object> transitiveSupplier = make(parentSupplier);
        assertEquals(null, transitiveSupplier.addObserver(mOnChangeCallback));
        assertTrue(parentSupplier.hasObservers());
        assertTrue(targetSupplier.hasObservers());

        targetSupplier.set(mObject1);
        assertEquals(mObject1, transitiveSupplier.get());
        verify(mOnChangeCallback).onResult(eq(mObject1));

        assertEquals(mObject1, transitiveSupplier.addObserver(mOnChangeCallback));
        transitiveSupplier.removeObserver(mOnChangeCallback);
        assertFalse(parentSupplier.hasObservers());
        assertFalse(targetSupplier.hasObservers());
    }

    @Test
    public void testAlreadyHasValueWhenObserverAdded() {
        ObservableSupplierImpl<ObservableSupplier<Object>> parentSupplier =
                new ObservableSupplierImpl<>();
        ObservableSupplierImpl<Object> targetSupplier = new ObservableSupplierImpl<>();
        parentSupplier.set(targetSupplier);
        targetSupplier.set(mObject1);

        ObservableSupplier<Object> transitiveSupplier = make(parentSupplier);
        assertEquals(mObject1, transitiveSupplier.get());

        assertEquals(mObject1, transitiveSupplier.addObserver(mOnChangeCallback));
        assertEquals(mObject1, transitiveSupplier.get());
        ShadowLooper.idleMainLooper();
        verify(mOnChangeCallback).onResult(eq(mObject1));
    }
}
