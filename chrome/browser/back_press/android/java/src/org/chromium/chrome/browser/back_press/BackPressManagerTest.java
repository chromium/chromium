// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import androidx.activity.BackEventCompat;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

import java.util.concurrent.TimeoutException;

/** Tests for {@link BackPressManager}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class BackPressManagerTest {

    private static class EmptyBackPressHandler implements BackPressHandler {
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

    private static class FailedBackPressHandler extends EmptyBackPressHandler {
        @Override
        public @BackPressResult int handleBackPress() {
            return BackPressResult.FAILURE;
        }
    }

    @Test
    @SmallTest
    public void testBasic() {
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BackPressManager.HISTOGRAM).build();

        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 = ThreadUtils.runOnUiThreadBlocking(EmptyBackPressHandler::new);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(h1, BackPressHandler.Type.FIND_TOOLBAR);
                });

        triggerBackPressWithoutAssertionError(manager);

        histogramWatcher.assertExpected(
                "Handler's histogram should be not recorded if it is not executed");

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BackPressManager.HISTOGRAM, 16); // 16 is FIND_TOOLBAR
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    h1.getHandleBackPressChangedSupplier().set(true);
                    manager.getCallback().handleOnBackPressed();
                });
        histogramWatcher.assertExpected("Handler's histogram should be recorded if it is executed");

        histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BackPressManager.HISTOGRAM).build();
        ThreadUtils.runOnUiThreadBlocking(
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
        EmptyBackPressHandler h1 = ThreadUtils.runOnUiThreadBlocking(EmptyBackPressHandler::new);
        EmptyBackPressHandler h2 = ThreadUtils.runOnUiThreadBlocking(EmptyBackPressHandler::new);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(h1, BackPressHandler.Type.TEXT_BUBBLE);
                    manager.addHandler(h2, BackPressHandler.Type.XR_DELEGATE);
                    h1.getHandleBackPressChangedSupplier().set(false);
                    h2.getHandleBackPressChangedSupplier().set(true);
                    manager.getCallback().handleOnBackPressed();
                });

        histogramWatcher.assertExpected(
                "Only record to handler's histogram should have value 18 (XR_DELEGATE).");

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BackPressManager.HISTOGRAM,
                        BackPressManager.getHistogramValue(BackPressHandler.Type.TEXT_BUBBLE));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    h1.getHandleBackPressChangedSupplier().set(true);
                    manager.getCallback().handleOnBackPressed();
                });
        histogramWatcher.assertExpected(
                "Only record to handler's histogram should have value 0 (TEXT_BUBBLE).");
    }

    @Test
    @SmallTest
    public void testFailedHandlers() {
        BackPressManager manager = new BackPressManager();
        var textBubbleFailedHandler =
                ThreadUtils.runOnUiThreadBlocking(FailedBackPressHandler::new);
        var arSuccessHandler = ThreadUtils.runOnUiThreadBlocking(EmptyBackPressHandler::new);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(textBubbleFailedHandler, BackPressHandler.Type.TEXT_BUBBLE);
                    manager.addHandler(arSuccessHandler, BackPressHandler.Type.XR_DELEGATE);
                    textBubbleFailedHandler.getHandleBackPressChangedSupplier().set(true);
                    arSuccessHandler.getHandleBackPressChangedSupplier().set(true);
                });

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                BackPressManager.FAILURE_HISTOGRAM,
                                BackPressManager.getHistogramValue(
                                        BackPressHandler.Type.TEXT_BUBBLE))
                        .expectIntRecord(
                                BackPressManager.HISTOGRAM,
                                BackPressManager.getHistogramValue(
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
        var textBubbleFailedHandler =
                ThreadUtils.runOnUiThreadBlocking(FailedBackPressHandler::new);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(textBubbleFailedHandler, BackPressHandler.Type.TEXT_BUBBLE);
                    textBubbleFailedHandler.getHandleBackPressChangedSupplier().set(true);
                });

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                BackPressManager.FAILURE_HISTOGRAM,
                                BackPressManager.getHistogramValue(
                                        BackPressHandler.Type.TEXT_BUBBLE))
                        .build();
        triggerBackPressWithoutAssertionError(manager);
        callbackHelper.waitForOnly("Fallback should be triggered if all handlers failed.");
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testNoRecordWhenBackIsCancelled() {
        BackPressManager manager = new BackPressManager();

        EmptyBackPressHandler h1 = ThreadUtils.runOnUiThreadBlocking(EmptyBackPressHandler::new);

        var record =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.BackPress.SwipeEdge")
                        .expectNoRecords("Android.BackPress.Intercept")
                        .expectNoRecords("Android.BackPress.Intercept.LeftEdge")
                        .expectNoRecords("Android.BackPress.Intercept.RightEdge")
                        .expectNoRecords("Android.BackPress.SwipeEdge.TabHistoryNavigation")
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(h1, BackPressHandler.Type.TAB_HISTORY);
                    h1.getHandleBackPressChangedSupplier().set(true);

                    var backEvent = new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT);
                    manager.getCallback().handleOnBackStarted(backEvent);
                    backEvent = new BackEventCompat(1, 0, .5f, BackEventCompat.EDGE_LEFT);
                    manager.getCallback().handleOnBackProgressed(backEvent);

                    manager.getCallback().handleOnBackCancelled();
                });

        record.assertExpected("No record when back gesture is cancelled.");
    }

    @Test
    @SmallTest
    public void testRecordSwipeEdge() {
        BackPressManager manager = new BackPressManager();
        manager.setIsGestureNavEnabledSupplier(() -> true);

        EmptyBackPressHandler h1 = ThreadUtils.runOnUiThreadBlocking(EmptyBackPressHandler::new);
        EmptyBackPressHandler h2 = ThreadUtils.runOnUiThreadBlocking(EmptyBackPressHandler::new);

        var edgeRecords =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.BackPress.SwipeEdge", 0) // from left
                        .expectIntRecord(
                                "Android.BackPress.Intercept.LeftEdge",
                                BackPressManager.getHistogramValue(
                                        BackPressHandler.Type.XR_DELEGATE))
                        .expectNoRecords("Android.BackPress.Intercept.RightEdge")
                        .expectNoRecords("Android.BackPress.SwipeEdge.TabHistoryNavigation")
                        .build();
        // Trigger XR delegate back press handler from left side.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(h1, BackPressHandler.Type.TEXT_BUBBLE);
                    manager.addHandler(h2, BackPressHandler.Type.XR_DELEGATE);
                    h1.getHandleBackPressChangedSupplier().set(false);
                    h2.getHandleBackPressChangedSupplier().set(true);
                    Assert.assertTrue(manager.getCallback().isEnabled());

                    var backEvent = new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT);
                    manager.getCallback().handleOnBackStarted(backEvent);
                    backEvent = new BackEventCompat(1, 0, .5f, BackEventCompat.EDGE_LEFT);
                    manager.getCallback().handleOnBackProgressed(backEvent);

                    manager.getCallback().handleOnBackPressed();
                });

        edgeRecords.assertExpected("Wrong histogram records for XR delegate.");

        var edgeRecords2 =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.BackPress.SwipeEdge", 1) // from right
                        .expectIntRecord(
                                "Android.BackPress.Intercept.RightEdge",
                                BackPressManager.getHistogramValue(
                                        BackPressHandler.Type.TEXT_BUBBLE))
                        .expectNoRecords("Android.BackPress.Intercept.LeftEdge")
                        .expectNoRecords("Android.BackPress.SwipeEdge.TabHistoryNavigation")
                        .build();
        // Trigger Text bubble delegate back press handler from right side.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    h1.getHandleBackPressChangedSupplier().set(true);
                    h2.getHandleBackPressChangedSupplier().set(true);

                    var backEvent = new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_RIGHT);
                    manager.getCallback().handleOnBackStarted(backEvent);
                    backEvent = new BackEventCompat(1, 0, .5f, BackEventCompat.EDGE_RIGHT);
                    manager.getCallback().handleOnBackProgressed(backEvent);

                    manager.getCallback().handleOnBackPressed();
                });

        edgeRecords2.assertExpected("Wrong histogram records for Text Bubble delegate.");
    }

    @Test
    @SmallTest
    public void testRecordSwipeEdgeOfTabHistoryNavigation() {
        BackPressManager manager = new BackPressManager();
        manager.setIsGestureNavEnabledSupplier(() -> true);

        EmptyBackPressHandler h1 = ThreadUtils.runOnUiThreadBlocking(EmptyBackPressHandler::new);

        var edgeRecords =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.BackPress.SwipeEdge", 0) // from left
                        .expectIntRecord("Android.BackPress.SwipeEdge.TabHistoryNavigation", 0)
                        .expectIntRecord(
                                "Android.BackPress.Intercept.LeftEdge",
                                BackPressManager.getHistogramValue(
                                        BackPressHandler.Type.TAB_HISTORY))
                        .expectNoRecords("Android.BackPress.Intercept.RightEdge")
                        .build();
        // Trigger tab history navigation from left side.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(h1, BackPressHandler.Type.TAB_HISTORY);
                    h1.getHandleBackPressChangedSupplier().set(true);

                    var backEvent = new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT);
                    manager.getCallback().handleOnBackStarted(backEvent);
                    backEvent = new BackEventCompat(1, 0, .5f, BackEventCompat.EDGE_LEFT);
                    manager.getCallback().handleOnBackProgressed(backEvent);

                    manager.getCallback().handleOnBackPressed();
                });

        edgeRecords.assertExpected("Wrong histogram records for tab history navigation.");

        var edgeRecords2 =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.BackPress.SwipeEdge", 1) // from right
                        .expectIntRecord("Android.BackPress.SwipeEdge.TabHistoryNavigation", 1)
                        .expectIntRecord(
                                "Android.BackPress.Intercept.RightEdge",
                                BackPressManager.getHistogramValue(
                                        BackPressHandler.Type.TAB_HISTORY))
                        .expectNoRecords("Android.BackPress.Intercept.LeftEdge")
                        .build();
        // Trigger tab history navigation from right side.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var backEvent = new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_RIGHT);
                    manager.getCallback().handleOnBackStarted(backEvent);
                    backEvent = new BackEventCompat(1, 0, .5f, BackEventCompat.EDGE_RIGHT);
                    manager.getCallback().handleOnBackProgressed(backEvent);

                    manager.getCallback().handleOnBackPressed();
                });

        edgeRecords2.assertExpected("Wrong histogram records for VR delegate.");
    }

    // Trigger back press ignoring built-in assertion errors.
    private void triggerBackPressWithoutAssertionError(BackPressManager manager) {
        ThreadUtils.runOnUiThreadBlocking(
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
