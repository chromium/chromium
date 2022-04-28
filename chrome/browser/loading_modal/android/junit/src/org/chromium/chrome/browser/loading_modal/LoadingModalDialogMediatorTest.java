// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.loading_modal;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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

/**
 * Tests for {@link LoadingModalDialogMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class LoadingModalDialogMediatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private ModalDialogManager mModalDialogManager;

    @Mock
    private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    private LoadingModalDialogMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        Mockito.when(mModalDialogManagerSupplier.get()).thenReturn(mModalDialogManager);
        mMediator = new LoadingModalDialogMediator(mModalDialogManagerSupplier);
        mModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                         .with(ModalDialogProperties.CONTROLLER, mMediator)
                         .build();
    }

    @Test
    public void testObservesDialogManager() {
        mMediator.showDialog(mModel);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mModalDialogManager, times(1)).addObserver(mMediator);

        mMediator.onDismiss(mModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        verify(mModalDialogManager, times(1)).removeObserver(mMediator);
    }

    @Test
    public void testLoadedBeforeDelay() {
        // Tests dialog is not displayed if dismissed in less then 500ms.
        mMediator.showDialog(mModel);
        ShadowLooper.idleMainLooper(400, TimeUnit.MILLISECONDS);
        verify(mModalDialogManager, never())
                .showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);

        mMediator.dismissDialog(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mModalDialogManager, never())
                .showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);
    }

    @Test
    public void testLoadedAfterDelayBeforeDialog() {
        // Tests that dialog is immedeately dismissed if it was scheduled to be shown by the dialog
        // manager but was postponed due to higher priority dialog.
        mMediator.showDialog(mModel);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mModalDialogManager, times(1))
                .showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);

        mMediator.dismissDialog(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        verify(mModalDialogManager, times(1))
                .dismissDialog(mModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
    }

    @Test
    public void testLoadedAfterDelayAndPostponed() {
        // Tests that dialog is dismissed after at least 500 ms since it was shown. Covers the case
        // when the dialog was postponed by the Dialog Manager due higher priority dialog.
        mMediator.showDialog(mModel);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mModalDialogManager, times(1))
                .showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);

        // Assume the dialog visibility was postponed for 1000 ms due to higher priority dialog.
        ShadowLooper.idleMainLooper(1000, TimeUnit.MILLISECONDS);
        mMediator.onDialogAdded(mModel);

        mMediator.dismissDialog(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        // Dialog was sent 1400 ms ago, but is visible for 400 ms only, so it should not be
        // dismissed yet.
        ShadowLooper.idleMainLooper(400, TimeUnit.MILLISECONDS);
        verify(mModalDialogManager, never())
                .dismissDialog(mModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);

        // Dialog is visible for 500 ms, so it should be dismissed.
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        verify(mModalDialogManager, times(1))
                .dismissDialog(mModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
    }
}
