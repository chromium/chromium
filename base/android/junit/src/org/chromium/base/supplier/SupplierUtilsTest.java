// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link SupplierUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SupplierUtilsTest {

    @Test
    public void testWaitForAll_NoSuppliers() {
        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(callbackHelper::notifyCalled);
        callbackHelper.assertNotCalled();
        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertCalledOnce();
    }

    @Test
    public void testWaitForAll_AllSuppliersAlreadyHaveValues() {
        Supplier<Integer> baseSupplier = () -> 4;
        OneshotSupplierImpl<String> oneshotSupplier = new OneshotSupplierImpl<>();
        oneshotSupplier.set("foo");
        SettableNonNullObservableSupplier<Object> observableSupplier =
                ObservableSuppliers.createNonNull(new Object());
        SyncOneshotSupplierImpl<List<?>> syncOneshotSupplier = new SyncOneshotSupplierImpl<>();
        syncOneshotSupplier.set(new ArrayList<>());

        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(
                callbackHelper::notifyCalled,
                baseSupplier,
                oneshotSupplier,
                observableSupplier,
                syncOneshotSupplier);
        callbackHelper.assertNotCalled();
        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertCalledOnce();
    }

    @Test
    public void testWaitForAll_SomeSuppliersAlreadyHaveValues() {
        Supplier<Integer> baseSupplier = () -> 4;
        OneshotSupplierImpl<String> oneshotSupplier = new OneshotSupplierImpl<>();

        SettableNonNullObservableSupplier<Object> observableSupplier =
                ObservableSuppliers.createNonNull(new Object());

        SyncOneshotSupplierImpl<List<?>> syncOneshotSupplier = new SyncOneshotSupplierImpl<>();

        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(
                callbackHelper::notifyCalled,
                baseSupplier,
                oneshotSupplier,
                observableSupplier,
                syncOneshotSupplier);

        callbackHelper.assertNotCalled();
        oneshotSupplier.set("foo");
        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertNotCalled();
        syncOneshotSupplier.set(new ArrayList<>());
        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertCalledOnce();
    }

    @Test
    public void testWaitForAll_NoSuppliersAlreadyHaveValues() {
        OneshotSupplierImpl<String> oneshotSupplier = new OneshotSupplierImpl<>();
        SettableMonotonicObservableSupplier<Object> observableSupplier =
                ObservableSuppliers.createMonotonic();
        SyncOneshotSupplierImpl<List<?>> syncOneshotSupplier = new SyncOneshotSupplierImpl<>();

        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(
                callbackHelper::notifyCalled,
                oneshotSupplier,
                observableSupplier,
                syncOneshotSupplier);

        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertNotCalled();
        observableSupplier.set(new Object());
        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertNotCalled();
        oneshotSupplier.set("foo");
        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertNotCalled();
        syncOneshotSupplier.set(new ArrayList<>());
        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertCalledOnce();
    }

    @Test
    public void testWaitForAll_WaitForOneshotSupplier() {
        OneshotSupplierImpl<Object> supplier = new OneshotSupplierImpl<>();

        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(callbackHelper::notifyCalled, supplier);

        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertNotCalled();
        supplier.set(new Object());
        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertCalledOnce();
    }

    @Test
    public void testWaitForAll_WaitForObservableSupplier() {
        SettableMonotonicObservableSupplier<Object> supplier =
                ObservableSuppliers.createMonotonic();

        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(callbackHelper::notifyCalled, supplier);

        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertNotCalled();
        supplier.set(new Object());
        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertCalledOnce();
    }

    @Test
    public void testWaitForAll_WaitForSyncOneshotSupplier() {
        SyncOneshotSupplierImpl<Object> supplier = new SyncOneshotSupplierImpl<>();

        CallbackHelper callbackHelper = new CallbackHelper();
        SupplierUtils.waitForAll(callbackHelper::notifyCalled, supplier);

        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertNotCalled();
        supplier.set(new Object());
        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.assertCalledOnce();
    }
}
