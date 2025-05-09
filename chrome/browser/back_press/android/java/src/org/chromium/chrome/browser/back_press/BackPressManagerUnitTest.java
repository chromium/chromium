// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import android.os.Build;
import android.window.OnBackInvokedCallback;

import androidx.activity.BackEventCompat;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

import java.util.concurrent.TimeoutException;

/** Unit tests for {@link BackPressManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackPressManagerUnitTest {

    private static class EmptyBackPressHandler implements BackPressHandler {
        private final ObservableSupplierImpl<Boolean> mSupplier = new ObservableSupplierImpl<>();
        protected final CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public @BackPressResult int handleBackPress() {
            mCallbackHelper.notifyCalled();
            return BackPressResult.SUCCESS;
        }

        @Override
        public ObservableSupplierImpl<Boolean> getHandleBackPressChangedSupplier() {
            return mSupplier;
        }

        public CallbackHelper getCallbackHelper() {
            return mCallbackHelper;
        }

        @Override
        public void handleOnBackCancelled() {}

        @Override
        public void handleOnBackProgressed(@NonNull BackEventCompat backEvent) {}

        @Override
        public void handleOnBackStarted(@NonNull BackEventCompat backEvent) {}
    }

    private static class EmptyBackPressHandlerFailure extends EmptyBackPressHandler {
        @Override
        public @BackPressResult int handleBackPress() {
            mCallbackHelper.notifyCalled();
            return BackPressResult.FAILURE;
        }
    }

    private static class EscapeBackPressHandler extends EmptyBackPressHandler {
        @Nullable
        @Override
        public Boolean handleEscPress() {
            mCallbackHelper.notifyCalled();
            return Boolean.TRUE;
        }

        @Override
        public boolean invokeBackActionOnEscape() {
            return false;
        }
    }

    private static class EscapeBackPressHandlerFailure extends EscapeBackPressHandler {
        @Nullable
        @Override
        public Boolean handleEscPress() {
            mCallbackHelper.notifyCalled();
            return Boolean.FALSE;
        }
    }

    @Test
    @Before
    public void setup() {
        MinimizeAppAndCloseTabBackPressHandler.setVersionForTesting(Build.VERSION_CODES.TIRAMISU);
    }

    @Test
    public void testBasic() {
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 = new EmptyBackPressHandler();
        manager.addHandler(h1, 0);

        Assert.assertFalse(
                "Callback should be disabled if none of handlers are enabled",
                manager.getCallback().isEnabled());
        try {
            manager.getCallback().handleOnBackPressed();
        } catch (AssertionError ignored) {
        }
        Assert.assertEquals(
                "Handler's callback should not be executed if it is disabled",
                0,
                h1.getCallbackHelper().getCallCount());

        h1.getHandleBackPressChangedSupplier().set(true);
        Assert.assertEquals(
                "Should return the active handler", h1, manager.getEnabledBackPressHandler());
        Assert.assertTrue(
                "Callback should be enabled if any of handlers are enabled",
                manager.getCallback().isEnabled());
        manager.getCallback().handleOnBackPressed();
        Assert.assertEquals(
                "Handler's callback should be executed if it is enabled",
                1,
                h1.getCallbackHelper().getCallCount());

        h1.getHandleBackPressChangedSupplier().set(false);
        try {
            manager.getCallback().handleOnBackPressed();
        } catch (AssertionError ignored) {
        }
        Assert.assertEquals(
                "Handler's callback should not be executed if it is disabled",
                1,
                h1.getCallbackHelper().getCallCount());
    }

    @Test
    public void testMaintainingHandler() {
        BackPressManager manager = new BackPressManager();
        manager.setIsGestureNavEnabledSupplier(() -> true);
        EmptyBackPressHandler h1 = Mockito.spy(new EmptyBackPressHandler());
        EmptyBackPressHandler h2 = Mockito.spy(new EmptyBackPressHandler());
        manager.addHandler(h1, 0);
        manager.addHandler(h2, 1);
        h1.getHandleBackPressChangedSupplier().set(false);
        h2.getHandleBackPressChangedSupplier().set(true);
        Assert.assertEquals(
                "Should return the active handler", h2, manager.getEnabledBackPressHandler());
        var backEvent = new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT);
        manager.getCallback().handleOnBackStarted(backEvent);
        Mockito.verify(h2).handleOnBackStarted(backEvent);

        backEvent = new BackEventCompat(1, 0, .5f, BackEventCompat.EDGE_LEFT);
        manager.getCallback().handleOnBackProgressed(backEvent);
        Mockito.verify(h2).handleOnBackProgressed(backEvent);

        backEvent = new BackEventCompat(2, 0, 1, BackEventCompat.EDGE_LEFT);
        manager.getCallback().handleOnBackProgressed(backEvent);
        Mockito.verify(h2).handleOnBackProgressed(backEvent);

        h1.getHandleBackPressChangedSupplier().set(true);

        manager.getCallback().handleOnBackPressed();
        Mockito.verify(h2).handleBackPress();
    }

    @Test
    public void testMultipleHandlers() {
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 = new EmptyBackPressHandler();
        EmptyBackPressHandler h2 = new EmptyBackPressHandler();
        manager.addHandler(h1, 0);
        manager.addHandler(h2, 1);

        h1.getHandleBackPressChangedSupplier().set(true);
        Assert.assertTrue(
                "Callback should be enabled if any of handlers are enabled.",
                manager.getCallback().isEnabled());

        h1.getHandleBackPressChangedSupplier().set(false);
        Assert.assertFalse(
                "Callback should be disabled if none of handlers are enabled.",
                manager.getCallback().isEnabled());

        h2.getHandleBackPressChangedSupplier().set(true);
        Assert.assertEquals(
                "Should return the first active handler", h2, manager.getEnabledBackPressHandler());
        Assert.assertTrue(
                "Callback should be enabled if any of handlers are enabled.",
                manager.getCallback().isEnabled());

        manager.getCallback().handleOnBackPressed();
        Assert.assertEquals(
                "Handler's callback should not be triggered if it's disabled",
                0,
                h1.getCallbackHelper().getCallCount());
        Assert.assertEquals(
                "Handler's callback should be triggered if it's enabled",
                1,
                h2.getCallbackHelper().getCallCount());

        h1.getHandleBackPressChangedSupplier().set(true);
        manager.getCallback().handleOnBackPressed();
        Assert.assertEquals(
                "Handler's callback should be triggered if it's enabled",
                1,
                h1.getCallbackHelper().getCallCount());
        Assert.assertEquals(
                "Handler's callback should not be triggered if other earlier handler has already"
                        + " consumed the event",
                1,
                h2.getCallbackHelper().getCallCount());
    }

    @Test
    public void testHighPriority() {
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 = new EmptyBackPressHandler();
        EmptyBackPressHandler h2 = new EmptyBackPressHandler();
        manager.addHandler(h1, 0);
        manager.addHandler(h2, 1);

        h1.getHandleBackPressChangedSupplier().set(true);
        h2.getHandleBackPressChangedSupplier().set(true);
        manager.getCallback().handleOnBackPressed();
        Assert.assertEquals(
                "Should return the active handler of higher priority",
                h1,
                manager.getEnabledBackPressHandler());
        Assert.assertEquals(
                "Enabled handler of higher priority should intercept the back gesture",
                1,
                h1.getCallbackHelper().getCallCount());

        h1.getHandleBackPressChangedSupplier().set(false);
        h2.getHandleBackPressChangedSupplier().set(true);
        manager.getCallback().handleOnBackPressed();
        Assert.assertEquals(
                "Disabled handler should be unable to intercept the back gesture",
                1,
                h1.getCallbackHelper().getCallCount());
        Assert.assertEquals(
                "Enabled handler of lower priority should be able to intercept the back gesture if"
                        + " there is no other enabled handler of higher priority..",
                1,
                h2.getCallbackHelper().getCallCount());
    }

    @Test
    public void testRemoveHandler() {
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 = new EmptyBackPressHandler();
        EmptyBackPressHandler h2 = new EmptyBackPressHandler();
        manager.addHandler(h1, 0);
        manager.addHandler(h2, 1);
        Assert.assertEquals("Two handlers should be registered", 2, getHandlerCount(manager));

        manager.removeHandler(h2);
        Assert.assertEquals("One handler should be removed", 1, getHandlerCount(manager));
        Assert.assertFalse("One handler should be removed", manager.has(2));

        manager.removeHandler(h1);
        Assert.assertEquals("All handlers should have been removed", 0, getHandlerCount(manager));
        Assert.assertFalse("All handlers should have been removed", manager.has(1));
    }

    @Test
    public void testRemoveEnabledHandler() {
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 = new EmptyBackPressHandler();
        EmptyBackPressHandler h2 = new EmptyBackPressHandler();
        manager.addHandler(h1, 0);
        manager.addHandler(h2, 1);
        Assert.assertEquals("Two handlers should be registered", 2, getHandlerCount(manager));
        Assert.assertFalse(
                "Callback should be disabled if no handler is enabled.",
                manager.getCallback().isEnabled());

        h1.getHandleBackPressChangedSupplier().set(true);
        h2.getHandleBackPressChangedSupplier().set(true);
        Assert.assertTrue(
                "Callback should be enabled if any handler is enabled.",
                manager.getCallback().isEnabled());

        manager.removeHandler(h2);
        Assert.assertEquals("One handler should be removed", 1, getHandlerCount(manager));
        Assert.assertFalse("One handler should be removed", manager.has(2));
        Assert.assertTrue(
                "Callback should be enabled if any handler is enabled.",
                manager.getCallback().isEnabled());

        manager.removeHandler(h1);
        Assert.assertEquals("All handlers should have been removed", 0, getHandlerCount(manager));
        Assert.assertFalse("All handlers should have been removed", manager.has(1));
        Assert.assertFalse(
                "Callback should be disabled if no handler is registered.",
                manager.getCallback().isEnabled());
    }

    @Test
    public void testNoHandlerRegistered() {
        BackPressManager manager = new BackPressManager();
        Assert.assertFalse(
                "Callback should be disabled if no handler is registered",
                manager.getCallback().isEnabled());
    }

    @Test
    public void testDisabledHandlers() {
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 = new EmptyBackPressHandler();
        EmptyBackPressHandler h2 = new EmptyBackPressHandler();
        manager.addHandler(h1, 0);
        manager.addHandler(h2, 1);

        Assert.assertFalse(
                "Callback should be disabled if no value is provided by handler",
                manager.getCallback().isEnabled());
        try {
            manager.getCallback().handleOnBackPressed();
        } catch (AssertionError ignored) {
        }
        Assert.assertEquals(
                "Callback should be disabled if no value is provided by handler",
                0,
                h1.getCallbackHelper().getCallCount());
        Assert.assertEquals(
                "Callback should be disabled if no value is provided by handler",
                0,
                h2.getCallbackHelper().getCallCount());

        h1.getHandleBackPressChangedSupplier().set(false);
        Assert.assertFalse(
                "Callback should be disabled if handler is disabled",
                manager.getCallback().isEnabled());
        try {
            manager.getCallback().handleOnBackPressed();
        } catch (AssertionError ignored) {
        }
        Assert.assertEquals(
                "Callback should be disabled if handler is disabled",
                0,
                h1.getCallbackHelper().getCallCount());
        Assert.assertEquals(
                "Callback should be disabled if handler is disabled",
                0,
                h2.getCallbackHelper().getCallCount());

        h2.getHandleBackPressChangedSupplier().set(false);
        Assert.assertFalse(
                "Callback should be disabled if handler is disabled",
                manager.getCallback().isEnabled());
        try {
            manager.getCallback().handleOnBackPressed();
        } catch (AssertionError ignored) {
        }
        Assert.assertEquals(
                "Callback should be disabled if handler is disabled",
                0,
                h1.getCallbackHelper().getCallCount());
        Assert.assertEquals(
                "Callback should be disabled if handler is disabled",
                0,
                h2.getCallbackHelper().getCallCount());

        Assert.assertFalse(
                "Callback should be disabled if no value is provided by handler",
                manager.getCallback().isEnabled());
    }

    @Test
    public void testDestroy() {
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 = new EmptyBackPressHandler();
        EmptyBackPressHandler h2 = new EmptyBackPressHandler();
        manager.addHandler(h1, 0);
        manager.addHandler(h2, 1);
        h1.getHandleBackPressChangedSupplier().set(true);
        Assert.assertTrue(
                "Callback should be enabled if any of handlers are enabled",
                manager.getCallback().isEnabled());

        manager.destroy();
        Assert.assertFalse(
                "Callback should be disabled if manager class has been destroyed",
                manager.getCallback().isEnabled());
    }

    @Test
    public void testObservableSupplierNullValue() {
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 = new EmptyBackPressHandler();
        EmptyBackPressHandler h2 = new EmptyBackPressHandler();
        manager.addHandler(h1, 0);
        manager.addHandler(h2, 1);
        h1.getHandleBackPressChangedSupplier().set(true);
        Assert.assertTrue(
                "Callback should be enabled if any of handlers are enabled",
                manager.getCallback().isEnabled());

        h1.getHandleBackPressChangedSupplier().set(null);
        Assert.assertFalse(
                "Callback should be disabled if no handler is enabled",
                manager.getCallback().isEnabled());
    }

    @Test
    public void testOnBackPressProgressed() {
        BackPressManager manager = new BackPressManager();
        manager.setIsGestureNavEnabledSupplier(() -> true);
        EmptyBackPressHandler h1 = Mockito.spy(new EmptyBackPressHandler());
        EmptyBackPressHandler h2 = Mockito.spy(new EmptyBackPressHandler());
        manager.addHandler(h1, 0);
        manager.addHandler(h2, 1);
        h1.getHandleBackPressChangedSupplier().set(false);
        h2.getHandleBackPressChangedSupplier().set(true);
        Assert.assertEquals(
                "Should return the active handler", h2, manager.getEnabledBackPressHandler());
        var backEvent = new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT);
        manager.getCallback().handleOnBackStarted(backEvent);
        Mockito.verify(h2).handleOnBackStarted(backEvent);

        backEvent = new BackEventCompat(1, 0, .5f, BackEventCompat.EDGE_LEFT);
        manager.getCallback().handleOnBackProgressed(backEvent);
        Mockito.verify(h2).handleOnBackProgressed(backEvent);

        backEvent = new BackEventCompat(2, 0, 1, BackEventCompat.EDGE_LEFT);
        manager.getCallback().handleOnBackProgressed(backEvent);
        Mockito.verify(h2).handleOnBackProgressed(backEvent);

        manager.getCallback().handleOnBackPressed();
        Mockito.verify(h2).handleBackPress();

        Mockito.verify(
                        h2,
                        Mockito.never()
                                .description(
                                        "Cancelled should never be called if back is not"
                                                + " cancelled"))
                .handleOnBackCancelled();
    }

    @Test
    public void testOnBackPressCancelled() {
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 = Mockito.spy(new EmptyBackPressHandler());
        EmptyBackPressHandler h2 = Mockito.spy(new EmptyBackPressHandler());
        manager.addHandler(h1, 0);
        manager.addHandler(h2, 1);
        h1.getHandleBackPressChangedSupplier().set(false);
        h2.getHandleBackPressChangedSupplier().set(true);
        Assert.assertEquals(
                "Should return the active handler", h2, manager.getEnabledBackPressHandler());
        var backEvent = new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT);
        manager.getCallback().handleOnBackStarted(backEvent);
        Mockito.verify(h2).handleOnBackStarted(backEvent);

        backEvent = new BackEventCompat(1, 0, .5f, BackEventCompat.EDGE_LEFT);
        manager.getCallback().handleOnBackProgressed(backEvent);
        Mockito.verify(h2).handleOnBackProgressed(backEvent);

        backEvent = new BackEventCompat(2, 0, 1, BackEventCompat.EDGE_LEFT);
        manager.getCallback().handleOnBackProgressed(backEvent);
        Mockito.verify(h2).handleOnBackProgressed(backEvent);

        manager.getCallback().handleOnBackCancelled();
        Mockito.verify(h2).handleOnBackCancelled();

        Mockito.verify(
                        h2,
                        Mockito.never()
                                .description(
                                        "handleBackPress should never be called if back is"
                                                + " cancelled"))
                .handleBackPress();
    }

    @Test
    public void testOnBackPressedCallback() throws TimeoutException {
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 = new EmptyBackPressHandler();
        manager.addHandler(h1, 0);

        CallbackHelper callbackHelper = new CallbackHelper();
        manager.setOnBackPressedListener(callbackHelper::notifyCalled);

        h1.getHandleBackPressChangedSupplier().set(true);
        Assert.assertEquals(
                "Should return the active handler", h1, manager.getEnabledBackPressHandler());
        Assert.assertTrue(
                "Callback should be enabled if any of handlers are enabled",
                manager.getCallback().isEnabled());
        manager.getCallback().handleOnBackPressed();
        Assert.assertEquals(
                "Handler's callback should be executed if it is enabled",
                1,
                h1.getCallbackHelper().getCallCount());

        callbackHelper.waitForCallback(
                "Callback should be triggered when back button is pressed", 0);

        manager.getCallback().handleOnBackPressed();
        callbackHelper.waitForCallback("Callback should be called again", 1);
    }

    @RequiresApi(api = Build.VERSION_CODES.TIRAMISU)
    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.TIRAMISU)
    public void testOnSystemNavigationObserver() {
        BackPressManager manager = new BackPressManager();
        manager.createOnSystemNavigationCallback();
        OnBackInvokedCallback callback = manager.getOnSystemNavigationCallback();
        CallbackHelper callbackHelper = new CallbackHelper();

        manager.addOnSystemNavigationObserver(callbackHelper::notifyCalled);
        manager.addOnSystemNavigationObserver(callbackHelper::notifyCalled);

        callback.onBackInvoked();
        Assert.assertEquals("All observers should be called", 2, callbackHelper.getCallCount());
    }

    @Test
    public void testEscapeUsageTrue() {
        BackPressManager manager = new BackPressManager();
        EscapeBackPressHandler h1 = new EscapeBackPressHandler();
        manager.addHandler(h1, 1);
        h1.getHandleBackPressChangedSupplier().set(true);

        Assert.assertEquals(
                "Handler should have invoked escape and consumed event.",
                Boolean.TRUE,
                manager.processEscapeKeyEvent());
        Assert.assertEquals(
                "Handler did not execute custom esc key code.",
                1,
                h1.getCallbackHelper().getCallCount());
    }

    @Test
    public void testEscapeUsageFalse() {
        BackPressManager manager = new BackPressManager();
        EscapeBackPressHandlerFailure h1 = new EscapeBackPressHandlerFailure();
        manager.addHandler(h1, 2);
        h1.getHandleBackPressChangedSupplier().set(true);

        Assert.assertNull(
                "Handler should not have consumed any event.", manager.processEscapeKeyEvent());
        Assert.assertEquals(
                "Handler did not execute custom esc key code.",
                1,
                h1.getCallbackHelper().getCallCount());
    }

    @Test
    public void testEscapeUsageFallthrough() {
        BackPressManager manager = new BackPressManager();
        EscapeBackPressHandlerFailure h1 = new EscapeBackPressHandlerFailure();
        EmptyBackPressHandlerFailure h2 = new EmptyBackPressHandlerFailure();
        EmptyBackPressHandlerFailure h3 = new EmptyBackPressHandlerFailure();
        EscapeBackPressHandler h4 = new EscapeBackPressHandler();

        manager.addHandler(h1, 3);
        manager.addHandler(h2, 6);
        manager.addHandler(h3, 8);
        manager.addHandler(h4, 10);
        h1.getHandleBackPressChangedSupplier().set(true);
        h2.getHandleBackPressChangedSupplier().set(true);
        h3.getHandleBackPressChangedSupplier().set(false);
        h4.getHandleBackPressChangedSupplier().set(true);

        Assert.assertEquals(
                "Handler should have fallen through failures to success and consumed event.",
                Boolean.TRUE,
                manager.processEscapeKeyEvent());
        Assert.assertEquals(
                "Handler did not execute custom esc key code even though it will fail.",
                1,
                h1.getCallbackHelper().getCallCount());
        Assert.assertEquals(
                "Handler did not execute back press code even though it will fall through.",
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
    }

    @Test
    public void testEscapePressesDoNotUseFallback() {
        BackPressManager manager = new BackPressManager();
        EscapeBackPressHandlerFailure h1 = new EscapeBackPressHandlerFailure();
        EscapeBackPressHandlerFailure h2 = new EscapeBackPressHandlerFailure();

        // Fail if the BackPressManager calls the fallback method, which it shouldn't.
        manager.setFallbackOnBackPressed(
                () -> {
                    assert false
                            : "BackPressManager should not call fallback on escape key presses.";
                });

        manager.addHandler(h1, 3);
        manager.addHandler(h2, 6);
        h1.getHandleBackPressChangedSupplier().set(true);
        h2.getHandleBackPressChangedSupplier().set(true);

        Assert.assertNull(
                "Manager should not have found any handlers to consume Esc.",
                manager.processEscapeKeyEvent());
        Assert.assertEquals(
                "Handler did not execute custom esc key code even though it will fail.",
                1,
                h1.getCallbackHelper().getCallCount());
        Assert.assertEquals(
                "Handler did not execute back press code even though it will fall through.",
                1,
                h2.getCallbackHelper().getCallCount());
    }

    private int getHandlerCount(BackPressManager manager) {
        int count = 0;
        for (BackPressHandler handler : manager.getHandlersForTesting()) {
            if (handler != null) count++;
        }
        return count;
    }
}
