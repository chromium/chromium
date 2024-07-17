// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.loading_modal;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.equalTo;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.modaldialog.DialogDismissalCause.ACTIVITY_DESTROYED;

import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator.State;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Integration tests for LoadingModalDialog. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class LoadingModalDialogIntegrationTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static BlankUiTestActivity sActivity;

    private static class TestDialogManagerObserver implements ModalDialogManagerObserver {
        private final CallbackHelper mDialogAddedCallbackHelper = new CallbackHelper();
        private final CallbackHelper mDialogDismissedCallbackHelper = new CallbackHelper();

        @Override
        public void onDialogAdded(PropertyModel model) {
            mDialogAddedCallbackHelper.notifyCalled();
        }

        @Override
        public void onDialogDismissed(PropertyModel model) {
            mDialogDismissedCallbackHelper.notifyCalled();
        }

        CallbackHelper getDialogAddedCallbackHelper() {
            return mDialogAddedCallbackHelper;
        }

        CallbackHelper getDialogDismissedCallbackHelper() {
            return mDialogDismissedCallbackHelper;
        }
    }

    private TestDialogManagerObserver mObserver = new TestDialogManagerObserver();

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        sActivity = runOnUiThreadBlocking(() -> sActivityTestRule.getActivity());
    }

    @Before
    public void setupTest() throws Exception {
        mObserver = new TestDialogManagerObserver();
        runOnUiThreadBlocking(() -> sActivity.getModalDialogManager().addObserver(mObserver));
    }

    @After
    public void teardownTest() throws Exception {
        runOnUiThreadBlocking(() -> sActivity.getModalDialogManager().removeObserver(mObserver));
    }

    @Test
    @MediumTest
    public void testShownAndDismissed() throws TimeoutException {
        LoadingModalDialogCoordinator coordinator =
                runOnUiThreadBlocking(
                        () ->
                                LoadingModalDialogCoordinator.create(
                                        getDialogManager(),
                                        sActivity,
                                        new Handler(Looper.getMainLooper())));
        coordinator.skipDelayForTesting();
        coordinator.disableTimeoutForTesting();

        runOnUiThreadBlocking(coordinator::show);
        mObserver.getDialogAddedCallbackHelper().waitForOnly();

        runOnUiThreadBlocking(coordinator::dismiss);
        mObserver.getDialogDismissedCallbackHelper().waitForOnly();

        assertThat(coordinator.getState(), equalTo(State.FINISHED));
    }

    @Test
    @MediumTest
    public void testShownAndCancelled() throws TimeoutException, ExecutionException {
        LoadingModalDialogCoordinator coordinator =
                runOnUiThreadBlocking(
                        () ->
                                LoadingModalDialogCoordinator.create(
                                        getDialogManager(),
                                        sActivity,
                                        new Handler(Looper.getMainLooper())));
        coordinator.skipDelayForTesting();
        coordinator.disableTimeoutForTesting();

        runOnUiThreadBlocking(coordinator::show);
        mObserver.getDialogAddedCallbackHelper().waitForOnly();

        View cancelButton = coordinator.getButtonsView().findViewById(R.id.cancel_loading_modal);
        runOnUiThreadBlocking(cancelButton::performClick);
        mObserver.getDialogDismissedCallbackHelper().waitForOnly();
        assertThat(coordinator.getState(), equalTo(State.CANCELLED));
    }

    @Test
    @MediumTest
    public void testShownAndDestroyed() throws TimeoutException {
        LoadingModalDialogCoordinator coordinator =
                runOnUiThreadBlocking(
                        () ->
                                LoadingModalDialogCoordinator.create(
                                        getDialogManager(),
                                        sActivity,
                                        new Handler(Looper.getMainLooper())));
        coordinator.skipDelayForTesting();
        coordinator.disableTimeoutForTesting();

        runOnUiThreadBlocking(coordinator::show);
        mObserver.getDialogAddedCallbackHelper().waitForOnly();

        runOnUiThreadBlocking(
                () -> sActivity.getModalDialogManager().dismissAllDialogs(ACTIVITY_DESTROYED));
        mObserver.getDialogDismissedCallbackHelper().waitForOnly();
        assertThat(coordinator.getState(), equalTo(State.CANCELLED));
    }

    private static ObservableSupplier<ModalDialogManager> getDialogManager() {
        ObservableSupplierImpl<ModalDialogManager> supplier = new ObservableSupplierImpl<>();
        supplier.set(sActivity.getModalDialogManager());
        return supplier;
    }
}
