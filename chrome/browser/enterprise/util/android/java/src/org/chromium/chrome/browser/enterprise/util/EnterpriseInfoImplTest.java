// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.util;

import android.os.Handler;
import android.os.Looper;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.RejectedExecutionException;

/** Unit tests for {@link EnterpriseInfoImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowPostTask.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class EnterpriseInfoImplTest {
    @Mock public EnterpriseInfo.Natives mNatives;

    @Before
    public void setUp() {
        EnterpriseInfo.reset();
        // Skip the AsyncTask, we don't actually want to query the device, just enqueue callbacks.
        getEnterpriseInfoImpl().setSkipAsyncCheckForTesting(true);

        MockitoAnnotations.initMocks(this);
        EnterpriseInfoJni.TEST_HOOKS.setInstanceForTesting(mNatives);
    }

    @After
    public void tearDown() {
        EnterpriseInfoJni.TEST_HOOKS.setInstanceForTesting(null);
    }

    private EnterpriseInfoImpl getEnterpriseInfoImpl() {
        return (EnterpriseInfoImpl) EnterpriseInfo.getInstance();
    }

    /**
     * Tests that the callback is called with the correct result. Tests both the first computation
     * and the cached value.
     */
    @Test
    @SmallTest
    public void testCallbacksGetResultValue() {
        EnterpriseInfoImpl instance = getEnterpriseInfoImpl();

        EnterpriseInfo.OwnedState stateIn = new EnterpriseInfo.OwnedState(false, true);

        class CallbackWithResult implements Callback<EnterpriseInfo.OwnedState> {
            public EnterpriseInfo.OwnedState result;

            @Override
            public void onResult(EnterpriseInfo.OwnedState result) {
                this.result = result;
            }
        }
        CallbackWithResult callback = new CallbackWithResult();
        CallbackWithResult callback2 = new CallbackWithResult();

        // Make the request and service the callbacks.
        instance.getDeviceEnterpriseInfo(callback);
        instance.getDeviceEnterpriseInfo(callback2);
        instance.setCacheResult(stateIn);
        instance.onEnterpriseInfoResultAvailable();

        // Results should be the same for all callbacks.
        Assert.assertEquals(
                "Callback doesn't match the expected result on servicing.",
                callback.result,
                stateIn);
        Assert.assertEquals(
                "Callback doesn't match the expected result on servicing.",
                callback2.result,
                stateIn);

        // Reset the callbacks.
        callback.result = null;
        callback2.result = null;
        Assert.assertNotEquals("Callback wasn't reset properly.", callback.result, stateIn);
        Assert.assertNotEquals("Callback wasn't reset properly.", callback2.result, stateIn);

        // Check the cached value is returned correctly.
        // Cached results are immediately added to the message queue. With the Roboelectric
        // framework these async tasks are run synchronously. Meaning as soon as we make the call
        // we'll have the result when it returns.
        instance.getDeviceEnterpriseInfo(callback);
        Assert.assertEquals(
                "Callback doesn't match the expected cached result.", callback.result, stateIn);

        instance.getDeviceEnterpriseInfo(callback2);
        Assert.assertEquals(
                "Callback doesn't match the expected cached result.", callback2.result, stateIn);
    }

    /** Test that if multiple callbacks get queued up that they're all serviced. */
    @Test
    @SmallTest
    public void testMultipleCallbacksServiced() {
        EnterpriseInfoImpl instance = getEnterpriseInfoImpl();
        CallbackHelper helper = new CallbackHelper();

        Callback<EnterpriseInfo.OwnedState> callback =
                (result) -> {
                    // We don't care about the result in this test.
                    helper.notifyCalled();
                };

        // Load up requests
        final int count = 5;
        for (int i = 0; i < count; i++) {
            instance.getDeviceEnterpriseInfo(callback);
        }

        // Nothing should be called yet.
        Assert.assertEquals(
                "Callbacks were serviced before they were meant to be.", 0, helper.getCallCount());

        // Do it. The result value here is irrelevant, put anything.
        instance.setCacheResult(new EnterpriseInfo.OwnedState(true, true));
        instance.onEnterpriseInfoResultAvailable();

        Assert.assertEquals(
                "The wrong number of callbacks were serviced.", count, helper.getCallCount());
    }

    /** Tests that a reentrant callback doesn't cause a synchronous reentry. */
    @Test
    @SmallTest
    public void testReentrantCallback() {
        EnterpriseInfoImpl instance = getEnterpriseInfoImpl();
        CallbackHelper helper = new CallbackHelper();

        // Make sure there is a cached value so that the getDeviceEnterpriseInfo() calls below will
        // post() immediately with a result. The value itself doesn't matter.
        instance.setCacheResult(new EnterpriseInfo.OwnedState(false, true));

        Callback<EnterpriseInfo.OwnedState> callback =
                (result) -> {
                    // We don't care about the result in this test.
                    helper.notifyCalled();
                };

        Callback<EnterpriseInfo.OwnedState> reentrantCallback =
                (result) -> {
                    // We don't care about the result in this test.
                    helper.notifyCalled();
                    instance.getDeviceEnterpriseInfo(callback);
                };

        Handler handler = new Handler(Looper.myLooper());

        // Roboelectric synchronously calls post() functions in its Looper, but we can still use it
        // to test for async behavior by inserting a post() with our assert at the correct point.
        handler.post(
                () -> {
                    // getDeviceEnterpriseInfo should insert a post() to run its callback. This
                    // post() will run after the outer post() is finished.
                    instance.getDeviceEnterpriseInfo(reentrantCallback);

                    // This inner post() will be inserted after the one from
                    // getDeviceEnterpriseInfo(reentrantCallback). Therefore it will run after
                    // |reentrantCallback| is invoked. When |reentrantCallback| is invoked it will
                    // run its own getDeviceEnterpriseInfo() which will in turn insert its own
                    // post(). If all goes as expected this assert should check after |helper| is
                    // notified once by |reentrantCallback| but before it's notified a second time
                    // by |callback|.
                    handler.post(
                            () -> {
                                Assert.assertEquals(
                                        "Reentrant callback wasn't executed as expect.",
                                        1,
                                        helper.getCallCount());
                            });

                    /* At this point the message queue should look like:
                       -------------------------------
                       Outer post() // Being run now.
                       post(reentrantCallback) // Inserts the `post(callback)`.
                       post(Assert)
                       post(callback) // Not yet inserted.
                       -------------------------------
                    */
                });
        // By this point all post()s should have been run, including |callback|'s.
        Assert.assertEquals("Second callback wasn't executed.", 2, helper.getCallCount());
    }

    /** Tests that OwnedStates's overridden equals() works as expected. */
    @Test
    @SmallTest
    public void testOwnedStateEquals() {
        // Two references to the same object are equal. Values don't matter here.
        EnterpriseInfo.OwnedState ref = new EnterpriseInfo.OwnedState(true, true);
        EnterpriseInfo.OwnedState sameRef = ref;
        Assert.assertEquals("Same reference check failed.", ref, sameRef);

        // This is also true for null.
        EnterpriseInfo.OwnedState nullRef = null;
        EnterpriseInfo.OwnedState nullRef2 = null;
        Assert.assertEquals("Null reference check failed.", nullRef, nullRef2);

        // A valid object and null should not be equal.

        Assert.assertNotEquals("Valid obj == null check failed.", ref, nullRef);

        // A different type of, valid, object shouldn't be equal.
        Object obj = new Object();
        Assert.assertNotEquals("Wrong object type check failed.", ref, obj);

        // Two valid owned states should only be equal when their member variables are all the same.
        EnterpriseInfo.OwnedState trueTrue = ref;
        EnterpriseInfo.OwnedState falseFalse = new EnterpriseInfo.OwnedState(false, false);
        EnterpriseInfo.OwnedState trueFalse = new EnterpriseInfo.OwnedState(true, false);
        EnterpriseInfo.OwnedState falseTrue = new EnterpriseInfo.OwnedState(false, true);
        EnterpriseInfo.OwnedState trueTrue2 = new EnterpriseInfo.OwnedState(true, true);

        Assert.assertNotEquals("Wrong value check failed.", trueTrue, falseFalse);
        Assert.assertNotEquals("Wrong value check failed.", trueTrue, trueFalse);
        Assert.assertNotEquals("Wrong value check failed.", trueTrue, falseTrue);
        Assert.assertEquals("Correct value check failed.", trueTrue, trueTrue2);
    }

    @Test
    @SmallTest
    public void testGetManagedStateForNative() {
        EnterpriseInfo.getManagedStateForNative();
        Mockito.verifyNoMoreInteractions(mNatives);

        getEnterpriseInfoImpl().setCacheResult(new EnterpriseInfo.OwnedState(true, false));
        getEnterpriseInfoImpl().onEnterpriseInfoResultAvailable();
        Mockito.verify(mNatives, Mockito.times(1)).updateNativeOwnedState(true, false);
    }

    @Test
    @SmallTest
    public void testGetManagedStateForNativeNullOwnedState() {
        getEnterpriseInfoImpl().setSkipAsyncCheckForTesting(false);
        ShadowPostTask.setTestImpl(
                (@TaskTraits int taskTraits, Runnable task, long delay) -> {
                    throw new RejectedExecutionException();
                });

        EnterpriseInfo.getManagedStateForNative();
        Mockito.verify(mNatives, Mockito.times(1)).updateNativeOwnedState(false, false);
    }
}
