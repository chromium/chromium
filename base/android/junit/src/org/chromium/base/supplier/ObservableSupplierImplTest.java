// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertTrue;

import android.os.Handler;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link ObservableSupplierImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ObservableSupplierImplTest {
    private static final String TEST_STRING_1 = "Test";
    private static final String TEST_STRING_2 = "Test2";

    private final ObservableSupplierImpl<@Nullable String> mSupplier =
            new ObservableSupplierImpl<>();

    private int mCallCount;
    private String mLastSuppliedString;

    @Test
    public void testObserverNotification_SetMultiple() {
        Callback<String> supplierObserver =
                result -> {
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
    public void testAddObserver_ShouldNotifyOnAdd() {
        mSupplier.set(TEST_STRING_1);

        AtomicBoolean called = new AtomicBoolean(false);
        Callback<String> supplierObserver = ignored -> called.set(true);

        mSupplier.addObserver(supplierObserver);
        ShadowLooper.runUiThreadTasks();
        assertTrue(called.get());
    }

    @Test
    public void testAddObserver_ShouldNotNotifyOnAdd() {
        mSupplier.set(TEST_STRING_1);

        AtomicBoolean called = new AtomicBoolean(false);
        Callback<String> supplierObserver = ignored -> called.set(true);

        mSupplier.addSyncObserver(supplierObserver);
        ShadowLooper.runUiThreadTasks();
        assertFalse(called.get());

        mSupplier.set(TEST_STRING_2);
        ShadowLooper.runUiThreadTasks();
        assertTrue(called.get());
    }

    @Test
    public void testAddObserver_ShouldNotOmitNullOnAdd() {
        AtomicBoolean called = new AtomicBoolean(false);
        Callback<String> supplierObserver = ignored -> called.set(true);

        mSupplier.addSyncObserverAndCall(supplierObserver); // !NotifyBehavior.OMIT_NULL_ON_ADD;
        ShadowLooper.runUiThreadTasks();
        assertTrue(called.get());
    }

    @Test
    public void testAddObserver_ShouldOmitNullOnAdd() {
        AtomicBoolean called = new AtomicBoolean(false);
        Callback<String> supplierObserver = ignored -> called.set(true);

        mSupplier.addSyncObserverAndCallIfSet(supplierObserver);
        ShadowLooper.runUiThreadTasks();
        assertFalse(called.get());

        mSupplier.set(TEST_STRING_2);
        ShadowLooper.runUiThreadTasks();
        assertTrue(called.get());
    }

    @Test
    public void testAddObserver_ShouldPostOnAdd() {
        mSupplier.set(TEST_STRING_1);
        AtomicBoolean called = new AtomicBoolean(false);
        Callback<String> supplierObserver = ignored -> called.set(true);

        mSupplier.addSyncObserverAndPostIfSet(supplierObserver);
        boolean idle = ShadowLooper.shadowMainLooper().isIdle();
        assertFalse(idle);
    }

    @Test
    public void testAddObserver_ShouldCallOnAdd() {
        mSupplier.set(TEST_STRING_1);
        AtomicBoolean called = new AtomicBoolean(false);
        Callback<String> supplierObserver = ignored -> called.set(true);

        mSupplier.addSyncObserverAndCallIfSet(supplierObserver); // !NotifyBehavior.POST_ON_ADD
        boolean idle = ShadowLooper.shadowMainLooper().isIdle();
        assertTrue(idle);
    }

    @Test
    public void testAddObserver_ShouldCallOnChange() {
        mSupplier.set(TEST_STRING_1);
        AtomicBoolean called = new AtomicBoolean(false);
        Callback<String> supplierObserver = ignored -> called.set(true);

        mSupplier.addSyncObserver(supplierObserver); // !NotifyBehavior.POST_ON_CHANGE
        boolean idle = ShadowLooper.shadowMainLooper().isIdle();
        assertTrue(idle);

        mSupplier.set(TEST_STRING_2);
        idle = ShadowLooper.shadowMainLooper().isIdle();
        assertTrue(idle);
    }

    @Test
    public void testObserverNotification_SetSame() {
        Callback<String> supplierObserver =
                result -> {
                    mCallCount++;
                    mLastSuppliedString = result;
                };

        mSupplier.addObserver(supplierObserver);
        checkState(0, null, null, "before setting first string.");

        mSupplier.set(TEST_STRING_1);
        checkState(1, TEST_STRING_1, TEST_STRING_1, "after setting first string.");

        mSupplier.set(TEST_STRING_1);
        checkState(1, TEST_STRING_1, TEST_STRING_1, "after resetting first string.");

        // Need to trick Java to not intern our new string.
        String anotherTestString1 = new String(new char[] {'T', 'e', 's', 't'});
        assertNotSame(TEST_STRING_1, anotherTestString1);
        mSupplier.set(anotherTestString1);
        // Don't use checkState, as the string arguments do not really make sense.
        assertEquals(
                "Incorrect call count after setting a different but equal string.", 1, mCallCount);
    }

    @Test
    public void testObserverNotification_RemoveObserver() {
        Callback<String> supplierObserver =
                result -> {
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
        handler.post(
                () -> {
                    mSupplier.set(TEST_STRING_1);
                    checkState(0, null, TEST_STRING_1, "after setting first string.");

                    Callback<String> supplierObserver =
                            (String result) -> {
                                mCallCount++;
                                mLastSuppliedString = result;
                            };

                    mSupplier.addObserver(supplierObserver);

                    checkState(0, null, TEST_STRING_1, "after setting observer.");
                });

        handler.post(() -> checkState(1, TEST_STRING_1, TEST_STRING_1, "in second message loop."));
    }

    @Test
    public void testObserverNotification_RegisterObserverAfterSetThenSetAgain() {
        Handler handler = new Handler();
        handler.post(
                () -> {
                    mSupplier.set(TEST_STRING_1);
                    checkState(0, null, TEST_STRING_1, "after setting first string.");

                    Callback<String> supplierObserver =
                            (String result) -> {
                                mCallCount++;
                                mLastSuppliedString = result;
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
        handler.post(
                () -> {
                    mSupplier.set(TEST_STRING_1);
                    checkState(0, null, TEST_STRING_1, "after setting first string.");

                    Callback<String> supplierObserver =
                            (String result) -> {
                                mCallCount++;
                                mLastSuppliedString = result;
                            };

                    mSupplier.addObserver(supplierObserver);

                    checkState(0, null, TEST_STRING_1, "after setting observer.");

                    mSupplier.removeObserver(supplierObserver);
                });

        handler.post(() -> checkState(0, null, TEST_STRING_1, "in second message loop."));
    }

    @Test
    public void testObserverNotification_RemoveObserverInsideCallback() {
        Callback<String> supplierObserver =
                new Callback<>() {
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

    @Test
    public void testHasObservers() {
        Callback<String> observer1 = (ignored) -> {};
        Callback<String> observer2 = (ignored) -> {};

        assertFalse("No observers yet", mSupplier.hasObservers());

        mSupplier.addObserver(observer1);
        assertTrue("Should have observer1", mSupplier.hasObservers());

        mSupplier.addObserver(observer1);
        assertTrue("Adding observer1 twice shouldn't break anything", mSupplier.hasObservers());

        mSupplier.removeObserver(observer1);
        assertFalse(
                "observer1 should be entirely removed with one remove", mSupplier.hasObservers());

        mSupplier.addObserver(observer1);
        mSupplier.addObserver(observer2);
        assertTrue("Should have multiple observers", mSupplier.hasObservers());

        mSupplier.removeObserver(observer1);
        assertTrue("Should still have observer2", mSupplier.hasObservers());

        mSupplier.removeObserver(observer1);
        assertTrue("Removing observer1 twice shouldn't break anything", mSupplier.hasObservers());

        mSupplier.removeObserver(observer2);
        assertFalse("Both observers should be gone", mSupplier.hasObservers());
    }

    private void checkState(
            int expectedCallCount,
            String expectedLastSuppliedString,
            String expectedStringFromGet,
            String assertDescription) {
        assertEquals("Incorrect call count " + assertDescription, expectedCallCount, mCallCount);
        assertEquals(
                "Incorrect last supplied string " + assertDescription,
                expectedLastSuppliedString,
                mLastSuppliedString);
        assertEquals(
                "Incorrect #get() " + assertDescription, expectedStringFromGet, mSupplier.get());
    }
}
