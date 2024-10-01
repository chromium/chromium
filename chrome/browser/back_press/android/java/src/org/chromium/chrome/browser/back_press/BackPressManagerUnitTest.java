// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import android.os.Build;

import androidx.activity.BackEventCompat;
import androidx.annotation.NonNull;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

import java.util.concurrent.TimeoutException;

/** Unit tests for {@link BackPressManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.BACK_TO_HOME_ANIMATION})
public class BackPressManagerUnitTest {

    private static class EmptyBackPressHandler implements BackPressHandler {
        private ObservableSupplierImpl<Boolean> mSupplier = new ObservableSupplierImpl<>();
        private CallbackHelper mCallbackHelper = new CallbackHelper();

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
    @DisableFeatures({ChromeFeatureList.BACK_TO_HOME_ANIMATION})
    public void testAlwaysEnabledCallback_TabbedActivity() {
        BackPressManager manager = new BackPressManager();
        manager.setHasSystemBackArm(true);
        EmptyBackPressHandler h1 = new EmptyBackPressHandler();
        EmptyBackPressHandler h2 = new EmptyBackPressHandler();
        manager.addHandler(h1, 0);
        manager.addHandler(h2, 1);
        h1.getHandleBackPressChangedSupplier().set(true);
        Assert.assertTrue(
                "Callback should be enabled if any of handlers are enabled",
                manager.getCallback().isEnabled());
        h1.getHandleBackPressChangedSupplier().set(false);
        Assert.assertFalse("No handler is enabled", manager.shouldInterceptBackPress());
        Assert.assertTrue("Callback is always enabled", manager.getCallback().isEnabled());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.BACK_TO_HOME_ANIMATION})
    public void testAlwaysEnabledCallback_NonTabbedActivity() {
        BackPressManager manager = new BackPressManager();
        EmptyBackPressHandler h1 = new EmptyBackPressHandler();
        EmptyBackPressHandler h2 = new EmptyBackPressHandler();
        manager.addHandler(h1, 0);
        manager.addHandler(h2, 1);
        h1.getHandleBackPressChangedSupplier().set(true);
        Assert.assertTrue(
                "Callback should be enabled if any of handlers are enabled",
                manager.getCallback().isEnabled());
        h1.getHandleBackPressChangedSupplier().set(false);
        Assert.assertFalse("No handler is enabled", manager.shouldInterceptBackPress());
        Assert.assertFalse(
                "Callback should not be always enabled on non tabbed activity",
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

    private int getHandlerCount(BackPressManager manager) {
        int count = 0;
        for (BackPressHandler handler : manager.getHandlersForTesting()) {
            if (handler != null) count++;
        }
        return count;
    }
}
