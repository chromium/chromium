// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import androidx.activity.BackEventCompat;
import androidx.annotation.Nullable;
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
        private final ObservableSupplierImpl<Boolean> mSupplier = new ObservableSupplierImpl<>();
        protected final CallbackHelper mCallbackHelper = new CallbackHelper();

        public CallbackHelper getCallbackHelper() {
            return mCallbackHelper;
        }

        @Override
        public @BackPressResult int handleBackPress() {
            mCallbackHelper.notifyCalled();
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
            mCallbackHelper.notifyCalled();
            return BackPressResult.FAILURE;
        }
    }

    private static class EscModifyingBackPressHandler extends EmptyBackPressHandler {

        private final Boolean mReturnValue;

        private EscModifyingBackPressHandler(Boolean mReturnValue) {
            this.mReturnValue = mReturnValue;
        }

        @Override
        public boolean invokeBackActionOnEscape() {
            return false;
        }

        @Nullable
        @Override
        public Boolean handleEscPress() {
            mCallbackHelper.notifyCalled();
            return mReturnValue;
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

    @Test
    @SmallTest
    public void testEscapeUsageTrue() {
        BackPressManager manager = new BackPressManager();
        EscModifyingBackPressHandler h1 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new EscModifyingBackPressHandler(Boolean.TRUE));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(h1, 0);
                    h1.getHandleBackPressChangedSupplier().set(true);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "Handler should have returned true and consumed esc event.",
                            manager.processEscapeKeyEvent());
                    Assert.assertEquals(
                            "Handler did not execute custom esc key code.",
                            1,
                            h1.getCallbackHelper().getCallCount());
                });
    }

    @Test
    @SmallTest
    public void testEscapeUsageFalse() {
        BackPressManager manager = new BackPressManager();
        EscModifyingBackPressHandler h1 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new EscModifyingBackPressHandler(Boolean.FALSE));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(h1, 1);
                    h1.getHandleBackPressChangedSupplier().set(true);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNull(
                            "Handler should have returned null and not consumed any event.",
                            manager.processEscapeKeyEvent());
                    Assert.assertEquals(
                            "Handler did not execute custom esc key code.",
                            1,
                            h1.getCallbackHelper().getCallCount());
                });
    }

    @Test
    @SmallTest
    public void testEscapeUsageFallthrough() {
        BackPressManager manager = new BackPressManager();

        EscModifyingBackPressHandler h1 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new EscModifyingBackPressHandler(Boolean.FALSE));
        FailedBackPressHandler h2 = ThreadUtils.runOnUiThreadBlocking(FailedBackPressHandler::new);
        EmptyBackPressHandler h3 = ThreadUtils.runOnUiThreadBlocking(EmptyBackPressHandler::new);
        EscModifyingBackPressHandler h4 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new EscModifyingBackPressHandler(Boolean.TRUE));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(h1, 1);
                    manager.addHandler(h2, 3);
                    manager.addHandler(h3, 5);
                    manager.addHandler(h4, 9);
                    h1.getHandleBackPressChangedSupplier().set(true);
                    h2.getHandleBackPressChangedSupplier().set(true);
                    h3.getHandleBackPressChangedSupplier().set(false);
                    h4.getHandleBackPressChangedSupplier().set(true);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Handler should have fallen through failures to success and consumed"
                                    + " event.",
                            Boolean.TRUE,
                            manager.processEscapeKeyEvent());
                    Assert.assertEquals(
                            "Handler did not execute custom esc key code even though it will fail.",
                            1,
                            h1.getCallbackHelper().getCallCount());
                    Assert.assertEquals(
                            "Handler did not execute back press code even though it will fall"
                                    + " through.",
                            1,
                            h2.getCallbackHelper().getCallCount());
                    Assert.assertEquals(
                            "Handler should not have executed back press code.",
                            0,
                            h3.getCallbackHelper().getCallCount());
                    Assert.assertEquals(
                            "Handler did not execute custom esc key code.",
                            1,
                            h4.getCallbackHelper().getCallCount());
                });
    }

    @Test
    @SmallTest
    public void testEscapePressesDoNotUseFallback() {
        BackPressManager manager = new BackPressManager();

        EscModifyingBackPressHandler h1 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new EscModifyingBackPressHandler(Boolean.FALSE));
        EscModifyingBackPressHandler h2 =
                ThreadUtils.runOnUiThreadBlocking(() -> new EscModifyingBackPressHandler(null));

        // Fail if the BackPressManager calls the fallback method, which it shouldn't.
        manager.setFallbackOnBackPressed(
                () -> {
                    assert false
                            : "BackPressManager should not call fallback on escape key presses.";
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    manager.addHandler(h1, 2);
                    manager.addHandler(h2, 4);
                    h1.getHandleBackPressChangedSupplier().set(true);
                    h2.getHandleBackPressChangedSupplier().set(true);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNull(
                            "Manager should not have found any handlers to consume Esc.",
                            manager.processEscapeKeyEvent());
                    Assert.assertEquals(
                            "Handler did not execute custom esc key code even though it will fail.",
                            1,
                            h1.getCallbackHelper().getCallCount());
                    Assert.assertEquals(
                            "Handler did not execute custom esc key code even though it will fail.",
                            1,
                            h2.getCallbackHelper().getCallCount());
                });
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
