// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link BackPressManager}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class BackPressManagerTest {
    private class EmptyBackPressHandler implements BackPressHandler {
        private ObservableSupplierImpl<Boolean> mSupplier = new ObservableSupplierImpl<>();

        @Override
        public void handleBackPress() {}

        @Override
        public ObservableSupplierImpl<Boolean> getHandleBackPressChangedSupplier() {
            return mSupplier;
        }
    }

    @BeforeClass
    public static void setUpClass() {
        ObservableSupplierImpl.setIgnoreThreadChecksForTesting(true);
    }

    @AfterClass
    public static void afterClass() {
        ObservableSupplierImpl.setIgnoreThreadChecksForTesting(false);
    }

    @Test
    @SmallTest
    public void testBasic() {
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BackPressManager.HISTOGRAM).build();

        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 =
                TestThreadUtils.runOnUiThreadBlockingNoException(EmptyBackPressHandler::new);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { manager.addHandler(h1, BackPressHandler.Type.FIND_TOOLBAR); });

        manager.getCallback().handleOnBackPressed();

        histogramWatcher.assertExpected(
                "Handler's histogram should be not recorded if it is not executed");

        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                BackPressManager.HISTOGRAM, 16); // 16 is FIND_TOOLBAR
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { h1.getHandleBackPressChangedSupplier().set(true); });
        manager.getCallback().handleOnBackPressed();
        histogramWatcher.assertExpected("Handler's histogram should be recorded if it is executed");

        histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BackPressManager.HISTOGRAM).build();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { h1.getHandleBackPressChangedSupplier().set(false); });
        manager.getCallback().handleOnBackPressed();
        histogramWatcher.assertExpected(
                "Handler's histogram should be not recorded if it is not executed");
    }

    @Test
    @SmallTest
    public void testMultipleHandlers() {
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                BackPressManager.HISTOGRAM, 2); // 2 is AR_DELEGATE
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 =
                TestThreadUtils.runOnUiThreadBlockingNoException(EmptyBackPressHandler::new);
        EmptyBackPressHandler h2 =
                TestThreadUtils.runOnUiThreadBlockingNoException(EmptyBackPressHandler::new);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            manager.addHandler(h1, BackPressHandler.Type.VR_DELEGATE);
            manager.addHandler(h2, BackPressHandler.Type.AR_DELEGATE);
            h1.getHandleBackPressChangedSupplier().set(false);
            h2.getHandleBackPressChangedSupplier().set(true);
        });

        manager.getCallback().handleOnBackPressed();
        histogramWatcher.assertExpected(
                "Only record to handler's histogram should have value 2 (AR_DELEGATE).");

        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                BackPressManager.HISTOGRAM, 1); // 1 is VR_DELEGATE
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { h1.getHandleBackPressChangedSupplier().set(true); });
        manager.getCallback().handleOnBackPressed();
        histogramWatcher.assertExpected(
                "Only record to handler's histogram should have value 1 (VR_DELEGATE).");
    }
}
