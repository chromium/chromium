// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;

import java.util.concurrent.TimeoutException;

/**
 * Test relating to {@link PersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class PersistedTabDataTest {
    private static final int INITIAL_VALUE = 42;
    private static final int CHANGED_VALUE = 51;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
    }

    @SmallTest
    @UiThreadTest
    @Test
    public void testCacheCallbacks() throws InterruptedException {
        Tab tab = MockTab.createAndInitialize(1, false);
        tab.setIsTabSaveEnabled(true);
        MockPersistedTabData mockPersistedTabData = new MockPersistedTabData(tab, INITIAL_VALUE);
        registerObserverSupplier(mockPersistedTabData);
        mockPersistedTabData.save();
        // 1
        MockPersistedTabData.from(tab, (res) -> {
            Assert.assertEquals(INITIAL_VALUE, res.getField());
            registerObserverSupplier(tab.getUserDataHost().getUserData(MockPersistedTabData.class));
            tab.getUserDataHost().getUserData(MockPersistedTabData.class).setField(CHANGED_VALUE);
            // Caching callbacks means 2) shouldn't overwrite CHANGED_VALUE
            // back to INITIAL_VALUE in the callback.
            MockPersistedTabData.from(
                    tab, (ares) -> { Assert.assertEquals(CHANGED_VALUE, ares.getField()); });
        });
        // 2
        MockPersistedTabData.from(tab, (res) -> {
            Assert.assertEquals(CHANGED_VALUE, res.getField());
            mockPersistedTabData.delete();
        });
    }

    @SmallTest
    @UiThreadTest
    @Test
    public void testSerializeAndLogOutOfMemoryError() {
        Tab tab = MockTab.createAndInitialize(1, false);
        OutOfMemoryMockPersistedTabData outOfMemoryMockPersistedTabData =
                new OutOfMemoryMockPersistedTabData(tab);
        Assert.assertNull(outOfMemoryMockPersistedTabData
                                  .getOomAndMetricsWrapper(
                                          outOfMemoryMockPersistedTabData.getSerializeSupplier())
                                  .get());
    }

    @SmallTest
    @UiThreadTest
    @Test(expected = OutOfMemoryError.class)
    public void testSerializeOutOfMemoryError() {
        Tab tab = MockTab.createAndInitialize(1, false);
        OutOfMemoryMockPersistedTabData outOfMemoryMockPersistedTabData =
                new OutOfMemoryMockPersistedTabData(tab);
        outOfMemoryMockPersistedTabData.getSerializeSupplier().get();
    }

    @SmallTest
    @UiThreadTest
    @Test
    public void testSerializeSupplierUiBackgroundThread() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = MockTab.createAndInitialize(1, false);
            ThreadVerifierMockPersistedTabData threadVerifierMockPersistedTabData =
                    new ThreadVerifierMockPersistedTabData(tab);
            threadVerifierMockPersistedTabData.save();
            helper.notifyCalled();
        });
        helper.waitForCallback(count);
    }

    static class ThreadVerifierMockPersistedTabData extends MockPersistedTabData {
        ThreadVerifierMockPersistedTabData(Tab tab) {
            super(tab, 0 /** unused in ThreadVerifierMockPersistedTabData */);
        }

        @Override
        public Supplier<byte[]> getSerializeSupplier() {
            // Verify anything before the supplier is called on the UI thread
            ThreadUtils.assertOnUiThread();
            return () -> {
                // supplier.get() should be called on the background thread - if
                // it doesn't other {@link PersistedTabData} such as
                // {@link CriticalPersistedTabData} may unnecessarily consume
                // the UI thread and cause jank.
                ThreadUtils.assertOnBackgroundThread();
                return super.getSerializeSupplier().get();
            };
        }
    }

    static class OutOfMemoryMockPersistedTabData extends MockPersistedTabData {
        OutOfMemoryMockPersistedTabData(Tab tab) {
            super(tab, 0 /** unused in OutOfMemoryMockPersistedTabData */);
        }
        @Override
        public Supplier<byte[]> getSerializeSupplier() {
            return () -> {
                throw new OutOfMemoryError("Out of memory error");
            };
        }
    }

    private static void registerObserverSupplier(MockPersistedTabData mockPersistedTabData) {
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(true);
        mockPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
    }
}
