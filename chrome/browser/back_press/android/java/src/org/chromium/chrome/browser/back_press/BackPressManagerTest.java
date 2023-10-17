// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/** Tests for {@link BackPressManager}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class BackPressManagerTest {
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private class EmptyBackPressHandler implements BackPressHandler {
        private ObservableSupplierImpl<Boolean> mSupplier = new ObservableSupplierImpl<>();

        @Override
        public @BackPressResult int handleBackPress() {
            return BackPressResult.UNKNOWN;
        }

        @Override
        public ObservableSupplierImpl<Boolean> getHandleBackPressChangedSupplier() {
            return mSupplier;
        }
    }

    private class FailedBackPressHandler extends EmptyBackPressHandler {
        @Override
        public @BackPressResult int handleBackPress() {
            return BackPressResult.FAILURE;
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
                () -> {
                    manager.addHandler(h1, BackPressHandler.Type.FIND_TOOLBAR);
                });

        triggerBackPressWithoutAssertionError(manager);

        histogramWatcher.assertExpected(
                "Handler's histogram should be not recorded if it is not executed");

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BackPressManager.HISTOGRAM, 16); // 16 is FIND_TOOLBAR
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    h1.getHandleBackPressChangedSupplier().set(true);
                    manager.getCallback().handleOnBackPressed();
                });
        histogramWatcher.assertExpected("Handler's histogram should be recorded if it is executed");

        histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BackPressManager.HISTOGRAM).build();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    h1.getHandleBackPressChangedSupplier().set(false);
                });
        triggerBackPressWithoutAssertionError(manager);
        histogramWatcher.assertExpected(
                "Handler's histogram should be not recorded if it is not executed");
    }

    @Test
    @SmallTest
    public void testMultipleHandlers() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BackPressManager.HISTOGRAM, 18); // 18 is XR_DELEGATE
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 =
                TestThreadUtils.runOnUiThreadBlockingNoException(EmptyBackPressHandler::new);
        EmptyBackPressHandler h2 =
                TestThreadUtils.runOnUiThreadBlockingNoException(EmptyBackPressHandler::new);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(h1, BackPressHandler.Type.VR_DELEGATE);
                    manager.addHandler(h2, BackPressHandler.Type.XR_DELEGATE);
                    h1.getHandleBackPressChangedSupplier().set(false);
                    h2.getHandleBackPressChangedSupplier().set(true);
                    manager.getCallback().handleOnBackPressed();
                });

        histogramWatcher.assertExpected(
                "Only record to handler's histogram should have value 18 (XR_DELEGATE).");

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BackPressManager.HISTOGRAM, 1); // 1 is VR_DELEGATE
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    h1.getHandleBackPressChangedSupplier().set(true);
                    manager.getCallback().handleOnBackPressed();
                });
        histogramWatcher.assertExpected(
                "Only record to handler's histogram should have value 1 (VR_DELEGATE).");
    }

    @Test
    @SmallTest
    public void testFailedHandlers() {
        BackPressManager manager = new BackPressManager();
        var vrFailedHandler =
                TestThreadUtils.runOnUiThreadBlockingNoException(FailedBackPressHandler::new);
        var arSuccessHandler =
                TestThreadUtils.runOnUiThreadBlockingNoException(EmptyBackPressHandler::new);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(vrFailedHandler, BackPressHandler.Type.VR_DELEGATE);
                    manager.addHandler(arSuccessHandler, BackPressHandler.Type.XR_DELEGATE);
                    vrFailedHandler.getHandleBackPressChangedSupplier().set(true);
                    arSuccessHandler.getHandleBackPressChangedSupplier().set(true);
                });

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                BackPressManager.FAILURE_HISTOGRAM,
                                BackPressManager.getHistogramValueForTesting(
                                        BackPressHandler.Type.VR_DELEGATE))
                        .expectIntRecord(
                                BackPressManager.HISTOGRAM,
                                BackPressManager.getHistogramValueForTesting(
                                        BackPressHandler.Type.XR_DELEGATE))
                        .build();
        triggerBackPressWithoutAssertionError(manager);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testFallbackCallback() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        BackPressManager manager = new BackPressManager();
        manager.setFallbackOnBackPressed(callbackHelper::notifyCalled);
        var vrFailedHandler =
                TestThreadUtils.runOnUiThreadBlockingNoException(FailedBackPressHandler::new);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(vrFailedHandler, BackPressHandler.Type.VR_DELEGATE);
                    vrFailedHandler.getHandleBackPressChangedSupplier().set(true);
                });

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                BackPressManager.FAILURE_HISTOGRAM,
                                BackPressManager.getHistogramValueForTesting(
                                        BackPressHandler.Type.VR_DELEGATE))
                        .build();
        triggerBackPressWithoutAssertionError(manager);
        callbackHelper.waitForFirst("Fallback should be triggered if all handlers failed.");
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testInterval() {
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords("Android.BackPress.Interval").build();

        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 =
                TestThreadUtils.runOnUiThreadBlockingNoException(EmptyBackPressHandler::new);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(h1, BackPressHandler.Type.FIND_TOOLBAR);
                    h1.getHandleBackPressChangedSupplier().set(true);
                    manager.getCallback().handleOnBackPressed();
                });

        histogramWatcher.assertExpected(
                "Handler's histogram should be not recorded for the first time");

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Android.BackPress.Interval", 42);
        mFakeTimeTestRule.advanceMillis(42);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    h1.getHandleBackPressChangedSupplier().set(true);
                    manager.getCallback().handleOnBackPressed();
                });
        histogramWatcher.assertExpected(
                "The interval histogram should be recorded if two back press events have been"
                        + " intercepted");
    }

    // Trigger back press ignoring built-in assertion errors.
    private void triggerBackPressWithoutAssertionError(BackPressManager manager) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        manager.getCallback().handleOnBackPressed();
                    } catch (AssertionError ignored) {
                        String msg = ignored.getMessage();
                        if (msg == null) throw ignored;
                        if (msg.equals(
                                "Callback is enabled but no handler consumed back gesture.")) {
                            return;
                        } else if (msg.contains("didn't correctly handle back press; handled by")) {
                            return;
                        }
                        throw ignored;
                    }
                });
    }
}
