// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.loading_modal;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;

import static org.chromium.ui.modaldialog.DialogDismissalCause.ACTIVITY_DESTROYED;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator.State;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeoutException;

/** Integration tests for LoadingModalDialog. */
@RunWith(BaseRobolectricTestRunner.class)
public class LoadingModalDialogIntegrationTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager.Presenter mPresenter;

    private Activity mActivity;
    private ModalDialogManager mModalDialogManager;

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

    @Before
    public void setupTest() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mModalDialogManager =
                new ModalDialogManager(mPresenter, ModalDialogManager.ModalDialogType.TAB);
        mObserver = new TestDialogManagerObserver();
        getDialogManager().addObserver(mObserver);
    }

    @After
    public void teardownTest() {
        getDialogManager().removeObserver(mObserver);
    }

    @Test
    public void testShownAndDismissed() throws TimeoutException {
        LoadingModalDialogCoordinator coordinator =
                LoadingModalDialogCoordinator.create(
                        getDialogManager(), mActivity, new Handler(Looper.getMainLooper()));
        coordinator.skipDelayForTesting();
        coordinator.disableTimeoutForTesting();

        coordinator.show();
        mObserver.getDialogAddedCallbackHelper().waitForOnly();

        coordinator.dismiss();
        mObserver.getDialogDismissedCallbackHelper().waitForOnly();

        assertThat(coordinator.getState(), equalTo(State.FINISHED));
    }

    @Test
    public void testShownAndCancelled() throws TimeoutException {
        LoadingModalDialogCoordinator coordinator =
                LoadingModalDialogCoordinator.create(
                        getDialogManager(), mActivity, new Handler(Looper.getMainLooper()));
        coordinator.skipDelayForTesting();
        coordinator.disableTimeoutForTesting();

        coordinator.show();
        mObserver.getDialogAddedCallbackHelper().waitForOnly();

        View cancelButton = coordinator.getButtonsView().findViewById(R.id.cancel_loading_modal);
        cancelButton.performClick();
        mObserver.getDialogDismissedCallbackHelper().waitForOnly();
        assertThat(coordinator.getState(), equalTo(State.CANCELLED));
    }

    @Test
    public void testShownAndDestroyed() throws TimeoutException {
        LoadingModalDialogCoordinator coordinator =
                LoadingModalDialogCoordinator.create(
                        getDialogManager(), mActivity, new Handler(Looper.getMainLooper()));
        coordinator.skipDelayForTesting();
        coordinator.disableTimeoutForTesting();

        coordinator.show();
        mObserver.getDialogAddedCallbackHelper().waitForOnly();

        getDialogManager().dismissAllDialogs(ACTIVITY_DESTROYED);
        mObserver.getDialogDismissedCallbackHelper().waitForOnly();
        assertThat(coordinator.getState(), equalTo(State.CANCELLED));
    }

    private ModalDialogManager getDialogManager() {
        return mModalDialogManager;
    }
}
