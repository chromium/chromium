// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import androidx.annotation.Nullable;

import org.junit.Assert;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * A generic wrapper around {@link CallbackHelper} that takes an object when notified. Very often
 * tests pass a {@link org.chromium.base.Callback} which will be given a payload by the production
 * code, and the tests want to assert something about this payload. This class aims to reduce the
 * number of identical subclasses used to temporarily hold onto that payload.
 *
 * Sample usage:
 *
 * private interface ComplexSignature {
 *     void onResult(Object obj, Integer Int, Boolean bool);
 * }
 *
 * private interface ClassUnderTest {
 *     void getSimpleAsync(Callback<Object> callback);
 *     void getComplexAsync(ComplexSignature callback);
 * }
 *
 * // Typically the callback can be wired with simply a method reference.
 * @Test
 * public void testGetSimpleAsync() {
 *     ClassUnderTest testMe = initClassUnderTest();
 *     PayloadCallbackHelper<Object> callbackHelper = new PayloadCallbackHelper<>();
 *     testMe.getSimpleAsync(callbackHelper::notifyCalled);
 *     Assert.assertNotNull(callbackHelper.getOnlyPayloadBlocking());
 * }
 *
 * // Sometimes the method signature will be messier and you'll want a lambda.
 * @Test
 * public void testGetComplexAsync() {
 *     ClassUnderTest testMe = initClassUnderTest();
 *     PayloadCallbackHelper<Object> callbackHelper = new PayloadCallbackHelper<>();
 *     testMe.getComplexAsync((Object obj, Integer ignored1, Boolean ignored2) ->
 *         callbackHelper.notifyCalled(obj));
 *     Assert.assertNotNull(callbackHelper.getOnlyPayloadBlocking());
 * }
 *
 * @param <T> The type of object to be notified with.
 */
public class PayloadCallbackHelper<T> {
    private final List<T> mPayloadList = Collections.synchronizedList(new ArrayList<>());
    private final CallbackHelper mDelegate = new CallbackHelper();

    /**
     * Embed this method inside external callbacks to monitor when they are called.
     * @param payload The payload object to store for verification.
     */
    public void notifyCalled(T payload) {
        mPayloadList.add(payload);
        mDelegate.notifyCalled();
    }

    /**
     * Blocks until the requested payload is provided, and then returns it.
     * @param index Index into a conceptual array of payloads provided by sequential callbacks.
     * @return The nth payload provided to notify. Null is a valid return value if the callback was
     *         invoked with null.
     * @throws IndexOutOfBoundsException If notify is not called at least the specified number of
     *         times.
     */
    @Nullable
    public T getPayloadByIndexBlocking(int index) {
        return getPayloadByIndexBlocking(
                index, CallbackHelper.WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /**
     * Blocks until the requested payload is provided, and then returns it.
     * @param index Index into a conceptual array of payloads provided by sequential callbacks.
     * @param timeout timeout value for all callbacks to occur.
     * @param unit timeout unit.
     * @return The nth payload provided to notify. Null is a valid return value if the callback was
     *         invoked with null.
     * @throws IndexOutOfBoundsException If notify is not called at least the specified number of
     *         times.
     */
    @Nullable
    public T getPayloadByIndexBlocking(int index, long timeout, TimeUnit unit) {
        waitForCallback(1 + index, timeout, unit);
        return mPayloadList.get(index);
    }

    /**
     * Returns the payload, blocking if notify has not been called yet. Verifies that {@link
     * #notifyCalled} has only been invoked once.
     * @return The payload provided to notify. Null is a valid return value if the callback was
     *         invoked with null.
     * @throws IndexOutOfBoundsException If notify is never called.
     */
    @Nullable
    public T getOnlyPayloadBlocking() {
        waitForCallback(1, CallbackHelper.WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        // While this lock likely isn't necessary for tests to call this method correctly, it allows
        // this method to truly fulfil the contact promised by the method's name that there's only
        // one payload. Other threads may be waiting to notify while this lock is held. Note this
        // lock is the same Collections#synchronizedList is using.
        synchronized (mPayloadList) {
            Assert.assertEquals(1, mPayloadList.size());
            return mPayloadList.get(0);
        }
    }

    /**
     * @return The number of times notify has been called.
     */
    public int getCallCount() {
        return mDelegate.getCallCount();
    }

    /**
     * Blocks until notify has been called the specified number of times.
     * @param expectedCallCount The number of times notify should be called.
     * @param timeout timeout value for all callbacks to occur.
     * @param unit timeout unit.
     * @throws IndexOutOfBoundsException If notify is not called at least the specified number of
     *         times.
     */
    private void waitForCallback(int expectedCallCount, long timeout, TimeUnit unit) {
        int currentCallCount = mDelegate.getCallCount();
        int numberOfCallsToWaitFor = expectedCallCount - currentCallCount;
        if (numberOfCallsToWaitFor <= 0) {
            return;
        }
        try {
            mDelegate.waitForCallback(currentCallCount, numberOfCallsToWaitFor, timeout, unit);
        } catch (TimeoutException te) {
            throw new IllegalStateException(te);
        }
    }
}
