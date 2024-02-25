// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.loading_modal;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.os.Handler;
import android.os.SystemClock;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeUnit;

/** Tests for {@link LoadingModalDialogMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LoadingModalDialogMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;

    @Mock private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    @Mock private LoadingModalDialogCoordinator.Observer mDialogCoordinatorObserver;

    private LoadingModalDialogMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        Mockito.when(mModalDialogManagerSupplier.get()).thenReturn(mModalDialogManager);
        mMediator = new LoadingModalDialogMediator(mModalDialogManagerSupplier, new Handler());
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mMediator)
                        .build();
        mMediator.addObserver(mDialogCoordinatorObserver);
    }

    @Test
    public void testObservesDialogManager() {
        mMediator.show(mModel);
        ShadowLooper.runMainLooperOneTask();
        verify(mModalDialogManager).addObserver(mMediator);

        mMediator.onDismiss(mModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        verify(mModalDialogManager).removeObserver(mMediator);
    }

    @Test
    public void testLoadedBeforeDelay() {
        // Tests dialog is not displayed if dismissed in less then 500ms.
        mMediator.show(mModel);
        ShadowLooper.idleMainLooper(400, TimeUnit.MILLISECONDS);
        verify(mModalDialogManager, never())
                .showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);

        mMediator.dismiss();
        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.FINISHED);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mModalDialogManager, never())
                .showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);
        verify(mDialogCoordinatorObserver, never()).onDismissable();
        verify(mDialogCoordinatorObserver, never())
                .onDismissedWithState(LoadingModalDialogCoordinator.State.FINISHED);
    }

    @Test
    public void testLoadedAfterDelayBeforeDialog() {
        // Tests that dialog is immedeately dismissed if it was scheduled to be shown by the dialog
        // manager but was postponed due to higher priority dialog.
        mMediator.show(mModel);
        ShadowLooper.runMainLooperOneTask();
        verify(mModalDialogManager).showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);

        mMediator.dismiss();
        verify(mModalDialogManager)
                .dismissDialog(mModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        mMediator.onDismiss(mModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.FINISHED);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mDialogCoordinatorObserver, never()).onDismissable();
        verify(mDialogCoordinatorObserver, never())
                .onDismissedWithState(LoadingModalDialogCoordinator.State.FINISHED);
    }

    @Test
    public void testLoadedAfterDelayAndPostponed() {
        // Tests that dialog is dismissed after at least 500 ms since it was shown. Covers the
        // case when the dialog was postponed by the Dialog Manager due higher priority dialog.
        mMediator.show(mModel);
        ShadowLooper.runMainLooperOneTask();
        verify(mModalDialogManager).showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);

        // Assume the dialog visibility was postponed for 1000 ms due to higher priority dialog.
        ShadowLooper.idleMainLooper(1000, TimeUnit.MILLISECONDS);
        mMediator.onDialogAdded(mModel);

        mMediator.dismiss();
        // Dialog was sent 1400 ms ago, but is visible for 400 ms only, so it should not be
        // dismissed yet.
        ShadowLooper.idleMainLooper(400, TimeUnit.MILLISECONDS);
        verify(mDialogCoordinatorObserver, never()).onDismissable();
        verify(mModalDialogManager, never())
                .dismissDialog(mModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);

        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.FINISHED);

        // Dialog is visible for 500 ms, so it should be dismissed.
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        verify(mDialogCoordinatorObserver).onDismissable();
        verify(mModalDialogManager)
                .dismissDialog(mModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        mMediator.onDismiss(mModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.FINISHED);
    }

    @Test
    public void testTimedOutAfterBeingShown() {
        // Tests that dialog is dismissed after the timeout is reached.
        long startTimeMs = SystemClock.elapsedRealtime();
        mMediator.show(mModel);

        // Wait for the dialog to be shown.
        ShadowLooper.runMainLooperOneTask();
        verify(mModalDialogManager).showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);
        assertEquals(SystemClock.elapsedRealtime() - startTimeMs, 500);

        mMediator.onDialogAdded(mModel);
        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.SHOWN);

        // Wait for the dialog to be shown.
        ShadowLooper.runMainLooperOneTask();
        verify(mDialogCoordinatorObserver).onDismissable();
        assertEquals(SystemClock.elapsedRealtime() - startTimeMs, 1000);

        // Wait until timeout occurs (4500 ms visibility timeout + 500 ms delay).
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(SystemClock.elapsedRealtime() - startTimeMs, 5000);
        verify(mModalDialogManager).dismissDialog(mModel, DialogDismissalCause.CLIENT_TIMEOUT);

        mMediator.onDismiss(mModel, DialogDismissalCause.CLIENT_TIMEOUT);
        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.TIMED_OUT);
        verify(mDialogCoordinatorObserver)
                .onDismissedWithState(LoadingModalDialogCoordinator.State.TIMED_OUT);
    }

    @Test
    public void testIsCancelledWhenSystemBackUsed() {
        // Tests that dialog status updates correctly when cancelled.
        mMediator.show(mModel);
        ShadowLooper.runMainLooperOneTask();
        mMediator.onDialogAdded(mModel);
        mMediator.onDismiss(mModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.CANCELLED);
        verify(mDialogCoordinatorObserver)
                .onDismissedWithState(LoadingModalDialogCoordinator.State.CANCELLED);
        verify(mDialogCoordinatorObserver, never()).onDismissable();
    }

    @Test
    public void testStatusIsUpdatedToFinished() {
        // Tests that dialog status updates correctly up to finished.
        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.READY);

        mMediator.show(mModel);
        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.PENDING);

        ShadowLooper.runMainLooperOneTask();
        verify(mModalDialogManager).showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);
        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.PENDING);

        mMediator.onDialogAdded(mModel);
        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.SHOWN);

        mMediator.dismiss();
        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.FINISHED);
        verify(mDialogCoordinatorObserver, never()).onDismissable();

        ShadowLooper.runMainLooperOneTask();
        verify(mModalDialogManager)
                .dismissDialog(mModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        verify(mDialogCoordinatorObserver).onDismissable();

        mMediator.onDismiss(mModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        assertEquals(mMediator.getState(), LoadingModalDialogCoordinator.State.FINISHED);
    }
}
