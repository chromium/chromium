// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link BackPressManager}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class BackPressManagerTest {
    @Rule
    public HistogramTestRule mHistogramTester = new HistogramTestRule();

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
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        ObservableSupplierImpl.setIgnoreThreadChecksForTesting(true);
    }

    @AfterClass
    public static void afterClass() {
        ObservableSupplierImpl.setIgnoreThreadChecksForTesting(false);
    }

    @Test
    @SmallTest
    public void testBasic() {
        HistogramDelta d1 = new HistogramDelta(BackPressManager.HISTOGRAM, 0);
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 =
                TestThreadUtils.runOnUiThreadBlockingNoException(EmptyBackPressHandler::new);
        TestThreadUtils.runOnUiThreadBlocking(() -> { manager.addHandler(h1, 0); });

        manager.getCallback().handleOnBackPressed();

        Assert.assertEquals("Handler's histogram should be not recorded if it is not executed", 0,
                d1.getDelta());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { h1.getHandleBackPressChangedSupplier().set(true); });
        manager.getCallback().handleOnBackPressed();
        Assert.assertEquals(
                "Handler's histogram should be recorded if it is executed", 1, d1.getDelta());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { h1.getHandleBackPressChangedSupplier().set(false); });
        manager.getCallback().handleOnBackPressed();
        Assert.assertEquals("Handler's histogram should be not recorded if it is not executed", 1,
                d1.getDelta());
    }

    @Test
    @SmallTest
    public void testMultipleHandlers() {
        HistogramDelta d1 = new HistogramDelta(BackPressManager.HISTOGRAM, 0);
        HistogramDelta d2 = new HistogramDelta(BackPressManager.HISTOGRAM, 1);
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 =
                TestThreadUtils.runOnUiThreadBlockingNoException(EmptyBackPressHandler::new);
        EmptyBackPressHandler h2 =
                TestThreadUtils.runOnUiThreadBlockingNoException(EmptyBackPressHandler::new);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            manager.addHandler(h1, 0);
            manager.addHandler(h2, 1);
            h1.getHandleBackPressChangedSupplier().set(false);
            h2.getHandleBackPressChangedSupplier().set(true);
        });

        manager.getCallback().handleOnBackPressed();
        Assert.assertEquals("Handler's histogram should be not recorded if it is not executed", 0,
                d1.getDelta());
        Assert.assertEquals(
                "Handler's histogram should be recorded if it is executed", 1, d2.getDelta());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { h1.getHandleBackPressChangedSupplier().set(true); });
        manager.getCallback().handleOnBackPressed();
        Assert.assertEquals(
                "Handler's histogram should be recorded if it is executed", 1, d1.getDelta());
        Assert.assertEquals("Handler's histogram should be not recorded if it is not executed", 1,
                d2.getDelta());
    }
}
