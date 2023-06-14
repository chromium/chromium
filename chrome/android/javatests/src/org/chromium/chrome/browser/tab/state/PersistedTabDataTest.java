// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;

import java.nio.ByteBuffer;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Test relating to {@link PersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class PersistedTabDataTest {
    private static final int INITIAL_VALUE = 42;
    private static final int CHANGED_VALUE = 51;

    @Mock
    ShoppingPersistedTabData mShoppingPersistedTabDataMock;

    @Before
    public void setUp() throws Exception {
        // TODO(crbug.com/1337102): Remove runOnUiThreadBlocking call after code refactoring/cleanup
        // ShoppingPersistedTabData must be mocked on the ui thread, otherwise a thread assert will
        // fail. An ObserverList is created when creating the mock. The same ObserverList is used
        // later in the test.
        ThreadUtils.runOnUiThreadBlocking(() -> { MockitoAnnotations.initMocks(this); });
    }

    @SmallTest
    @Test
    public void testCacheCallbacks()
            throws InterruptedException, TimeoutException, ExecutionException {
        Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> {
            Tab t = MockTab.createAndInitialize(1, false);
            t.setIsTabSaveEnabled(true);
            return t;
        });
        MockPersistedTabData mockPersistedTabData = ThreadUtils.runOnUiThreadBlocking(() -> {
            MockPersistedTabData mptd = new MockPersistedTabData(tab, INITIAL_VALUE);
            registerObserverSupplier(mptd);
            mptd.save();
            return mptd;
        });

        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // 1
            MockPersistedTabData.from(tab, (res) -> {
                Assert.assertEquals(INITIAL_VALUE, res.getField());
                registerObserverSupplier(
                        tab.getUserDataHost().getUserData(MockPersistedTabData.class));
                tab.getUserDataHost()
                        .getUserData(MockPersistedTabData.class)
                        .setField(CHANGED_VALUE);
                // Caching callbacks means 2) shouldn't overwrite CHANGED_VALUE
                // back to INITIAL_VALUE in the callback.
                MockPersistedTabData.from(tab, (ares) -> {
                    Assert.assertEquals(CHANGED_VALUE, ares.getField());
                    helper.notifyCalled();
                });
            });
        });
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // 2
            MockPersistedTabData.from(tab, (res) -> {
                Assert.assertEquals(CHANGED_VALUE, res.getField());
                mockPersistedTabData.delete();
                helper.notifyCalled();
            });
        });
        helper.waitForCallback(0, 2);
    }

    @SmallTest
    @UiThreadTest
    @Test
    public void testSerializeAndLogOutOfMemoryError_Get() {
        Tab tab = MockTab.createAndInitialize(1, false);
        OutOfMemoryMockPersistedTabDataGet outOfMemoryMockPersistedTabData =
                new OutOfMemoryMockPersistedTabDataGet(tab);
        Assert.assertNull(outOfMemoryMockPersistedTabData.getOomAndMetricsWrapper().get());
    }

    @SmallTest
    @UiThreadTest
    @Test
    public void testSerializeAndLogOutOfMemoryError() {
        Tab tab = MockTab.createAndInitialize(1, false);
        OutOfMemoryMockPersistedTabData outOfMemoryMockPersistedTabData =
                new OutOfMemoryMockPersistedTabData(tab);
        Assert.assertNull(outOfMemoryMockPersistedTabData.getOomAndMetricsWrapper().get());
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

    @SmallTest
    @UiThreadTest
    @Test
    public void testOnTabClose() throws TimeoutException {
        TabImpl tab = (TabImpl) MockTab.createAndInitialize(1, false);
        tab.setIsTabSaveEnabled(true);
        tab.getUserDataHost().setUserData(
                ShoppingPersistedTabData.class, mShoppingPersistedTabDataMock);
        PersistedTabData.onTabClose(tab);
        Assert.assertFalse(tab.getIsTabSaveEnabledSupplierForTesting().get());
        verify(mShoppingPersistedTabDataMock, times(1)).disableSaving();
    }

    static class ThreadVerifierMockPersistedTabData extends MockPersistedTabData {
        ThreadVerifierMockPersistedTabData(Tab tab) {
            super(tab, 0 /** unused in ThreadVerifierMockPersistedTabData */);
        }

        @Override
        public Serializer<ByteBuffer> getSerializer() {
            // Verify anything before the supplier is called on the UI thread
            ThreadUtils.assertOnUiThread();
            return () -> {
                // supplier.get() should be called on the background thread - if
                // it doesn't other {@link PersistedTabData} such as
                // {@link CriticalPersistedTabData} may unnecessarily consume
                // the UI thread and cause jank.
                ThreadUtils.assertOnBackgroundThread();
                return super.getSerializer().get();
            };
        }
    }

    static class OutOfMemoryMockPersistedTabDataGet extends MockPersistedTabData {
        OutOfMemoryMockPersistedTabDataGet(Tab tab) {
            super(tab, 0 /** unused in OutOfMemoryMockPersistedTabData */);
        }
        @Override
        public Serializer<ByteBuffer> getSerializer() {
            return () -> {
                // OutOfMemoryError thrown on getSerializer.get();
                throw new OutOfMemoryError("Out of memory error");
            };
        }
    }

    static class OutOfMemoryMockPersistedTabData extends MockPersistedTabData {
        OutOfMemoryMockPersistedTabData(Tab tab) {
            super(tab, 0 /** unused in OutOfMemoryMockPersistedTabData */);
        }
        @Override
        public Serializer<ByteBuffer> getSerializer() {
            // OutOfMemoryError thrown on getSerializer
            throw new OutOfMemoryError("Out of memory error");
        }
    }

    private static void registerObserverSupplier(MockPersistedTabData mockPersistedTabData) {
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(true);
        mockPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
    }
}
