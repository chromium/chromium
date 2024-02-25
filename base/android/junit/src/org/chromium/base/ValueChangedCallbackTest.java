// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ValueChangedCallback}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ValueChangedCallbackTest {
    private static final String TEST_STRING_1 = "Test";
    private static final String TEST_STRING_2 = "Test2";

    private int mCallCount;
    private String mOldSuppliedString;
    private String mNewSuppliedString;
    private ObservableSupplierImpl<String> mSupplier = new ObservableSupplierImpl<>();

    @Test
    public void testObserverCaching() {
        ValueChangedCallback<String> observer =
                new ValueChangedCallback<>(
                        (String newValue, String oldValue) -> {
                            mCallCount++;
                            mNewSuppliedString = newValue;
                            mOldSuppliedString = oldValue;
                        });
        mSupplier.addObserver(observer);
        ShadowLooper.runUiThreadTasks();

        checkState(0, null, null, "before setting first string.");

        mSupplier.set(TEST_STRING_1);
        checkState(1, TEST_STRING_1, null, "after setting first string.");

        mSupplier.set(TEST_STRING_2);
        checkState(2, TEST_STRING_2, TEST_STRING_1, "after setting second string.");

        mSupplier.set(null);
        checkState(3, null, TEST_STRING_2, "after setting third string.");

        mSupplier.removeObserver(observer);
    }

    @Test
    public void testObserverCachingDuplicate() {
        ValueChangedCallback<String> observer =
                new ValueChangedCallback<>(
                        (String newValue, String oldValue) -> {
                            mCallCount++;
                            mNewSuppliedString = newValue;
                            mOldSuppliedString = oldValue;
                        });
        observer.onResult(TEST_STRING_1);
        checkState(1, TEST_STRING_1, null, "setting first string.");

        // Duplicates are ignored.
        observer.onResult(TEST_STRING_1);
        checkState(1, TEST_STRING_1, null, "setting first string duplicate.");
    }

    private void checkState(
            int expectedCallCount,
            String expectedNewSuppliedString,
            String expectedOldSuppliedString,
            String assertDescription) {
        assertEquals("Incorrect call count " + assertDescription, expectedCallCount, mCallCount);
        assertEquals(
                "Incorrect new supplied string " + assertDescription,
                expectedNewSuppliedString,
                mNewSuppliedString);
        assertEquals(
                "Incorrect old supplied string " + assertDescription,
                expectedOldSuppliedString,
                mOldSuppliedString);
    }
}
