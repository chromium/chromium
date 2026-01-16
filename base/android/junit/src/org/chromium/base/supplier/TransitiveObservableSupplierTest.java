// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
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
import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link TransitiveObservableSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TransitiveObservableSupplierTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock Callback<String> mOnChangeCallback;

    @Test
    public void testGetWithoutObservers() {
        ObservableSupplierImpl<MonotonicObservableSupplier<String>> parentSupplier =
                new ObservableSupplierImpl<>();
        ObservableSupplierImpl<String> targetSupplier1 = new ObservableSupplierImpl<>();

        NullableObservableSupplier<String> transitiveSupplier =
                parentSupplier.createTransitiveNullable(obs -> obs);
        assertNull(transitiveSupplier.get());

        parentSupplier.set(targetSupplier1);
        assertNull(transitiveSupplier.get());

        targetSupplier1.set("valueA");
        assertEquals("valueA", transitiveSupplier.get());

        targetSupplier1.set(null);
        assertNull(transitiveSupplier.get());

        targetSupplier1.set("valueB");
        assertEquals("valueB", transitiveSupplier.get());

        parentSupplier.set(null);
        assertNull(transitiveSupplier.get());

        targetSupplier1.set("valueC");
        assertNull(transitiveSupplier.get());

        parentSupplier.set(targetSupplier1);
        assertEquals("valueC", transitiveSupplier.get());
    }

    @Test
    public void testGetWithObserver() {
        ObservableSupplierImpl<MonotonicObservableSupplier<String>> parentSupplier =
                new ObservableSupplierImpl<>();
        ObservableSupplierImpl<String> targetSupplier1 = new ObservableSupplierImpl<>();
        ObservableSupplierImpl<String> targetSupplier2 = new ObservableSupplierImpl<>();

        NullableObservableSupplier<String> transitiveSupplier =
                parentSupplier.createTransitiveNullable(obs -> obs);
        assertNull(transitiveSupplier.get());

        assertNull(transitiveSupplier.addObserver(mOnChangeCallback));
        verifyNoInteractions(mOnChangeCallback);

        parentSupplier.set(targetSupplier1);
        assertNull(transitiveSupplier.get());
        verifyNoInteractions(mOnChangeCallback);

        targetSupplier1.set("valueA");
        assertEquals("valueA", transitiveSupplier.get());
        verify(mOnChangeCallback).onResult(eq("valueA"));

        targetSupplier1.set("valueB");
        assertEquals("valueB", transitiveSupplier.get());
        verify(mOnChangeCallback).onResult(eq("valueB"));

        targetSupplier1.set(null);
        assertNull(transitiveSupplier.get());
        verify(mOnChangeCallback).onResult(eq(null));

        targetSupplier2.set("valueC");
        parentSupplier.set(targetSupplier2);
        assertEquals("valueC", transitiveSupplier.get());
        verify(mOnChangeCallback).onResult(eq("valueC"));

        transitiveSupplier.removeObserver(mOnChangeCallback);
        targetSupplier2.set("valueD");
        assertEquals("valueD", transitiveSupplier.get());
        verify(mOnChangeCallback, never()).onResult(eq("valueD"));
    }

    @Test
    public void testSameObserver() {
        ObservableSupplierImpl<MonotonicObservableSupplier<String>> parentSupplier =
                new ObservableSupplierImpl<>();
        ObservableSupplierImpl<String> targetSupplier = new ObservableSupplierImpl<>();
        parentSupplier.set(targetSupplier);

        NullableObservableSupplier<String> transitiveSupplier =
                parentSupplier.createTransitiveNullable(obs -> obs);
        assertNull(transitiveSupplier.addObserver(mOnChangeCallback));
        assertTrue(parentSupplier.hasObservers());
        assertTrue(targetSupplier.hasObservers());

        targetSupplier.set("valueA");
        assertEquals("valueA", transitiveSupplier.get());
        verify(mOnChangeCallback).onResult(eq("valueA"));

        assertEquals("valueA", transitiveSupplier.addObserver(mOnChangeCallback));
        transitiveSupplier.removeObserver(mOnChangeCallback);
        assertFalse(parentSupplier.hasObservers());
        assertFalse(targetSupplier.hasObservers());
    }

    @Test
    public void testAlreadyHasValueWhenObserverAdded() {
        ObservableSupplierImpl<MonotonicObservableSupplier<String>> parentSupplier =
                new ObservableSupplierImpl<>();
        ObservableSupplierImpl<String> targetSupplier = new ObservableSupplierImpl<>();
        parentSupplier.set(targetSupplier);
        targetSupplier.set("valueA");

        NullableObservableSupplier<String> transitiveSupplier =
                parentSupplier.createTransitiveNullable(obs -> obs);
        assertEquals("valueA", transitiveSupplier.get());

        assertEquals("valueA", transitiveSupplier.addObserver(mOnChangeCallback));
        assertEquals("valueA", transitiveSupplier.get());
        ShadowLooper.idleMainLooper();
        verify(mOnChangeCallback).onResult(eq("valueA"));
    }

    @Test
    public void testAddObserver_ShouldNotifyOnAdd() {
        ObservableSupplierImpl<MonotonicObservableSupplier<String>> parentSupplier =
                new ObservableSupplierImpl<>();
        ObservableSupplierImpl<String> targetSupplier1 = new ObservableSupplierImpl<>();

        NullableObservableSupplier<String> transitiveSupplier =
                parentSupplier.createTransitiveNullable(obs -> obs);
        assertNull(transitiveSupplier.get());

        parentSupplier.set(targetSupplier1);
        assertNull(transitiveSupplier.get());
        verifyNoInteractions(mOnChangeCallback);

        targetSupplier1.set("valueA");
        assertEquals("valueA", transitiveSupplier.get());
        verifyNoInteractions(mOnChangeCallback);

        assertEquals(
                "valueA", transitiveSupplier.addSyncObserverAndCallIfNonNull(mOnChangeCallback));
        ShadowLooper.runUiThreadTasks();
        verify(mOnChangeCallback).onResult(eq("valueA"));
    }

    @Test
    public void testAddObserver_ShouldNotNotifyOnAdd() {
        ObservableSupplierImpl<MonotonicObservableSupplier<String>> parentSupplier =
                new ObservableSupplierImpl<>();
        ObservableSupplierImpl<String> targetSupplier1 = new ObservableSupplierImpl<>();

        NullableObservableSupplier<String> transitiveSupplier =
                parentSupplier.createTransitiveNullable(obs -> obs);
        assertNull(transitiveSupplier.get());

        parentSupplier.set(targetSupplier1);
        assertNull(transitiveSupplier.get());
        verifyNoInteractions(mOnChangeCallback);

        targetSupplier1.set("valueA");
        assertEquals("valueA", transitiveSupplier.get());
        verifyNoInteractions(mOnChangeCallback);

        assertEquals("valueA", transitiveSupplier.addSyncObserver(mOnChangeCallback));
        ShadowLooper.runUiThreadTasks();
        verifyNoInteractions(mOnChangeCallback);

        targetSupplier1.set("valueB");
        assertEquals("valueB", transitiveSupplier.get());
        verify(mOnChangeCallback).onResult(eq("valueB"));
    }

    @Test
    public void testNonNull_noObservers() {
        NonNullObservableSupplier<String> nonNullSupplier =
                ObservableSuppliers.createNonNull("nonNull");
        SettableMonotonicObservableSupplier<String> monotonicSupplier =
                ObservableSuppliers.createMonotonic();
        SettableNullableObservableSupplier<String> nullableSupplier =
                ObservableSuppliers.createNullable("nullable");

        // Should throw since monotonicSupplier.get() is null.
        assertThrows(
                AssertionError.class,
                () -> monotonicSupplier.createTransitiveNonNull(parent -> nonNullSupplier));
        // Should throw since nullableSupplier is not marked as monotonic.
        monotonicSupplier.set("foo");
        assertThrows(
                AssertionError.class,
                () ->
                        monotonicSupplier
                                .createTransitiveMonotonic(
                                        parent -> (MonotonicObservableSupplier<?>) nullableSupplier)
                                .get());

        SettableMonotonicObservableSupplier<String> monotonicSupplier2 =
                ObservableSuppliers.createMonotonic();
        AtomicReference<MonotonicObservableSupplier<String>> retValue =
                new AtomicReference<>(monotonicSupplier2);
        MonotonicObservableSupplier<String> transMonotonic =
                monotonicSupplier.createTransitiveMonotonic(unused -> retValue.get());
        assertNull(transMonotonic.get());
        monotonicSupplier2.set("foo");
        assertEquals("foo", transMonotonic.get());
        retValue.set(ObservableSuppliers.createMonotonic());
        // Will trigger since transitioning from non-null -> null.
        monotonicSupplier.set("triggers lambda");
        assertThrows(AssertionError.class, transMonotonic::get);
    }

    @Test
    public void testNonNull_withObservers() {
        NonNullObservableSupplier<String> nonNullSupplier =
                ObservableSuppliers.createNonNull("nonNull");
        SettableMonotonicObservableSupplier<String> monotonicSupplier =
                ObservableSuppliers.createMonotonic();
        SettableNullableObservableSupplier<String> nullableSupplier =
                ObservableSuppliers.createNullable("nullable");

        // Should throw since nullableSupplier is not marked as monotonic.
        monotonicSupplier.set("foo");
        assertThrows(
                AssertionError.class,
                () ->
                        monotonicSupplier
                                .createTransitiveMonotonic(
                                        parent -> (MonotonicObservableSupplier<?>) nullableSupplier)
                                .addObserver(CallbackUtils.emptyCallback()));

        SettableMonotonicObservableSupplier<String> monotonicSupplier2 =
                ObservableSuppliers.createMonotonic();
        AtomicReference<MonotonicObservableSupplier<String>> retValue =
                new AtomicReference<>(monotonicSupplier2);
        MonotonicObservableSupplier<String> transMonotonic =
                monotonicSupplier.createTransitiveMonotonic(unused -> retValue.get());
        assertNull(transMonotonic.addObserver(mOnChangeCallback));
        monotonicSupplier2.set("foo");
        verify(mOnChangeCallback).onResult("foo");
        clearInvocations(mOnChangeCallback);
        retValue.set(ObservableSuppliers.createMonotonic());
        // Will trigger since transitioning from non-null -> null.
        assertThrows(AssertionError.class, () -> monotonicSupplier.set("triggers lambda"));
    }

    @Test
    public void testMonotonicDefaultValue() {
        NonNullObservableSupplier<String> nonNullSupplier =
                ObservableSuppliers.createNonNull("nonNull");
        SettableMonotonicObservableSupplier<String> monotonicSupplier =
                ObservableSuppliers.createMonotonic();
        SettableNullableObservableSupplier<String> nullableSupplier =
                ObservableSuppliers.createNullable("nullable");

        // Test NonNull due to default value.
        NonNullObservableSupplier<String> transitiveNonNull =
                monotonicSupplier.createTransitiveNonNull("foo", unused -> nonNullSupplier);
        transitiveNonNull.addSyncObserverAndCallIfNonNull(mOnChangeCallback);
        verify(mOnChangeCallback).onResult(eq("foo"));
        assertEquals("foo", transitiveNonNull.get());

        // Test transition away from default value.
        monotonicSupplier.set("bar");
        verify(mOnChangeCallback).onResult(eq("nonNull"));
        assertEquals("nonNull", transitiveNonNull.get());
    }

    @Test
    public void testNullableDefaultValue() {
        SettableNullableObservableSupplier<String> nullableSupplier1 =
                ObservableSuppliers.createNullable();
        SettableNullableObservableSupplier<String> nullableSupplier2 =
                ObservableSuppliers.createNullable();
        AtomicReference<NullableObservableSupplier> secondSupplier =
                new AtomicReference<>(nullableSupplier2);

        NullableObservableSupplier<String> transitive =
                nullableSupplier1.createTransitiveNullable(unused -> secondSupplier.get());
        transitive.addSyncObserverAndCallIfNonNull(mOnChangeCallback);
        verifyNoInteractions(mOnChangeCallback);
        assertNull(transitive.get());

        // Set a value.
        nullableSupplier2.set("A");
        nullableSupplier1.set("B");
        verify(mOnChangeCallback).onResult("A");
        clearInvocations(mOnChangeCallback);
        assertEquals("A", transitive.get());

        // Test back to null because of supplier2.
        nullableSupplier2.set(null);
        verify(mOnChangeCallback).onResult(null);
        clearInvocations(mOnChangeCallback);
        assertNull(transitive.get());

        // Set a value.
        nullableSupplier2.set("B");
        verify(mOnChangeCallback).onResult("B");
        clearInvocations(mOnChangeCallback);
        assertEquals("B", transitive.get());

        // Test back to null because of supplier1.
        nullableSupplier1.set(null);
        verify(mOnChangeCallback).onResult(null);
        assertNull(transitive.get());
    }

    @Test
    public void testNonNullDefaultValue_withObservers() {
        NonNullObservableSupplier<String> nonNullSupplier =
                ObservableSuppliers.createNonNull("nonNull");
        SettableNullableObservableSupplier<String> nullableSupplier =
                ObservableSuppliers.createNullable();

        // Test NonNull due to default value.
        NonNullObservableSupplier<String> transitiveNonNull =
                nullableSupplier.createTransitiveNonNull("foo", unused -> nonNullSupplier);
        transitiveNonNull.addSyncObserverAndCallIfNonNull(mOnChangeCallback);
        verify(mOnChangeCallback).onResult(eq("foo"));
        clearInvocations(mOnChangeCallback);
        assertEquals("foo", transitiveNonNull.get());

        // Test transition away from default value.
        nullableSupplier.set("bar");
        verify(mOnChangeCallback).onResult(eq("nonNull"));
        clearInvocations(mOnChangeCallback);
        assertEquals("nonNull", transitiveNonNull.get());

        // Back to default value.
        nullableSupplier.set(null);
        verify(mOnChangeCallback).onResult(eq("foo"));
        assertEquals("foo", transitiveNonNull.get());
    }

    @Test
    public void testNonNullDefaultValue_withoutObservers() {
        NonNullObservableSupplier<String> nonNullSupplier =
                ObservableSuppliers.createNonNull("nonNull");
        SettableNullableObservableSupplier<String> nullableSupplier =
                ObservableSuppliers.createNullable();

        // Test NonNull due to default value.
        NonNullObservableSupplier<String> transitiveNonNull =
                nullableSupplier.createTransitiveNonNull("foo", unused -> nonNullSupplier);
        assertEquals("foo", transitiveNonNull.get());

        // Test transition away from default value.
        nullableSupplier.set("bar");
        assertEquals("nonNull", transitiveNonNull.get());

        // Back to default value.
        nullableSupplier.set(null);
        assertEquals("foo", transitiveNonNull.get());
    }

    @Test
    public void testGetAfterDestroy() {
        SettableNullableObservableSupplier<String> nullableSupplier1 =
                ObservableSuppliers.createNullable();
        SettableNullableObservableSupplier<String> nullableSupplier2 =
                ObservableSuppliers.createNullable();

        SettableNullableObservableSupplier<String> transitive =
                nullableSupplier1.createTransitiveNullable(unused -> nullableSupplier2);
        transitive.addSyncObserverAndCallIfNonNull(mOnChangeCallback);
        assertNull(transitive.get());

        // Set a value.
        nullableSupplier2.set("A");
        nullableSupplier1.set("B");
        verify(mOnChangeCallback).onResult("A");
        clearInvocations(mOnChangeCallback);
        assertEquals("A", transitive.get());

        // Ensure destroy() causes get() to return null.
        transitive.destroy();
        assertNull(transitive.get());
    }

    @Test
    public void testDestroyUnregisters() {
        SettableNullableObservableSupplier<String> nullableSupplier1 =
                ObservableSuppliers.createNullable();
        SettableNullableObservableSupplier<String> nullableSupplier2 =
                ObservableSuppliers.createNullable();

        SettableNullableObservableSupplier<String> transitive =
                nullableSupplier1.createTransitiveNullable(unused -> nullableSupplier2);
        transitive.addSyncObserverAndCallIfNonNull(mOnChangeCallback);
        transitive.destroy();

        // Set a value.
        nullableSupplier2.set("A");
        nullableSupplier1.set("B");
        verify(mOnChangeCallback, never()).onResult(any());
        assertNull(transitive.get());
    }
}
