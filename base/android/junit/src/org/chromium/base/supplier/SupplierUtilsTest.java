// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Unit tests for {@link SupplierUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SupplierUtilsTest {

    @Test
    public void testWaitForAll_NoSuppliers() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(callbackHelper::notifyCalled);
        callbackHelper.waitForOnly();
    }

    @Test
    public void testWaitForAll_AllSuppliersAlreadyHaveValues() throws TimeoutException {
        Supplier<Integer> baseSupplier = () -> 4;
        OneshotSupplierImpl<String> oneshotSupplier = new OneshotSupplierImpl<>();
        oneshotSupplier.set("foo");
        ObservableSupplierImpl<Object> observableSupplier = new ObservableSupplierImpl<>();
        observableSupplier.set(new Object());
        SyncOneshotSupplierImpl<List<?>> syncOneshotSupplier = new SyncOneshotSupplierImpl<>();
        syncOneshotSupplier.set(new ArrayList<>());

        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(
                callbackHelper::notifyCalled,
                baseSupplier,
                oneshotSupplier,
                observableSupplier,
                syncOneshotSupplier);
        callbackHelper.waitForOnly();
    }

    @Test
    public void testWaitForAll_SomeSuppliersAlreadyHaveValues() throws TimeoutException {
        Supplier<Integer> baseSupplier = () -> 4;
        OneshotSupplierImpl<String> oneshotSupplier = new OneshotSupplierImpl<>();

        ObservableSupplierImpl<Object> observableSupplier = new ObservableSupplierImpl<>();
        observableSupplier.set(new Object());

        SyncOneshotSupplierImpl<List<?>> syncOneshotSupplier = new SyncOneshotSupplierImpl<>();

        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(
                callbackHelper::notifyCalled,
                baseSupplier,
                oneshotSupplier,
                observableSupplier,
                syncOneshotSupplier);

        Assert.assertEquals(0, callbackHelper.getCallCount());
        oneshotSupplier.set("foo");
        Assert.assertEquals(0, callbackHelper.getCallCount());
        syncOneshotSupplier.set(new ArrayList<>());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        callbackHelper.waitForOnly();
    }

    @Test
    public void testWaitForAll_NoSuppliersAlreadyHaveValues() throws TimeoutException {
        OneshotSupplierImpl<String> oneshotSupplier = new OneshotSupplierImpl<>();
        ObservableSupplierImpl<Object> observableSupplier = new ObservableSupplierImpl<>();
        SyncOneshotSupplierImpl<List<?>> syncOneshotSupplier = new SyncOneshotSupplierImpl<>();

        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(
                callbackHelper::notifyCalled,
                oneshotSupplier,
                observableSupplier,
                syncOneshotSupplier);

        Assert.assertEquals(0, callbackHelper.getCallCount());
        observableSupplier.set(new Object());
        Assert.assertEquals(0, callbackHelper.getCallCount());
        oneshotSupplier.set("foo");
        Assert.assertEquals(0, callbackHelper.getCallCount());
        syncOneshotSupplier.set(new ArrayList<>());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        callbackHelper.waitForOnly();
    }

    @Test
    public void testWaitForAll_WaitForOneshotSupplier() throws TimeoutException {
        OneshotSupplierImpl<Object> supplier = new OneshotSupplierImpl<>();

        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(callbackHelper::notifyCalled, supplier);

        Assert.assertEquals(0, callbackHelper.getCallCount());
        supplier.set(new Object());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        callbackHelper.waitForOnly();
    }

    @Test
    public void testWaitForAll_WaitForObservableSupplier() throws TimeoutException {
        ObservableSupplierImpl<Object> supplier = new ObservableSupplierImpl<>();

        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(callbackHelper::notifyCalled, supplier);

        Assert.assertEquals(0, callbackHelper.getCallCount());
        supplier.set(new Object());
        callbackHelper.waitForOnly();
    }

    @Test
    public void testWaitForAll_WaitForSyncOneshotSupplier() throws TimeoutException {
        SyncOneshotSupplierImpl<Object> supplier = new SyncOneshotSupplierImpl<>();

        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(callbackHelper::notifyCalled, supplier);

        Assert.assertEquals(0, callbackHelper.getCallCount());
        supplier.set(new Object());
        callbackHelper.waitForOnly();
    }
}
