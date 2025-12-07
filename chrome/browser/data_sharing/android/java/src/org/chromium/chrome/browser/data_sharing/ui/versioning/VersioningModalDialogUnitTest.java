// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.versioning;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.modaldialog.ModalDialogProperties.CONTROLLER;

import android.content.Context;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.MockitoHelper;

/** Unit tests for {@link VersioningModalDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VersioningModalDialogUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Runnable mMockExitRunnable;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    private PropertyModel mModel;
    private Controller mController;

    private void showDialog() {
        VersioningModalDialog.show(mContext, mModalDialogManager);
        verify(mModalDialogManager)
                .showDialog(mPropertyModelCaptor.capture(), eq(ModalDialogType.APP));
        mModel = mPropertyModelCaptor.getValue();
        mController = mModel.get(CONTROLLER);
        MockitoHelper.doCallback(
                        1,
                        (Integer dismissalCause) -> mController.onDismiss(mModel, dismissalCause))
                .when(mModalDialogManager)
                .dismissDialog(any(), anyInt());
    }

    @Test
    public void testShow_positiveButton() {
        showDialog();
        mController.onClick(mModel, ButtonType.POSITIVE);
        verify(mContext).startActivity(any());
    }

    @Test
    public void testShow_negativeButton() {
        showDialog();
        mController.onClick(mModel, ButtonType.NEGATIVE);
        verify(mContext, never()).startActivity(any());
    }

    @Test
    public void testShow_clickOutside() {
        showDialog();
        mController.onDismiss(mModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        verify(mContext, never()).startActivity(any());
    }

    @Test
    public void testShowWithCustomMessage_dialogContentAndShownCorrectly() {
        String message = "Test Message";
        PropertyModel returnedModel =
                VersioningModalDialog.showWithCustomMessage(
                        mContext, mModalDialogManager, message, mMockExitRunnable);
        verify(mModalDialogManager)
                .showDialog(mPropertyModelCaptor.capture(), eq(ModalDialogType.APP));
        mModel = mPropertyModelCaptor.getValue();
        mController = mModel.get(CONTROLLER);
        assertEquals(returnedModel, mModel);

        // Verify that custom body message is set.
        assertEquals(message, mModel.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1));

        // Verify that dismissing the dialog calls the exit runnable.
        mController.onDismiss(mModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        verify(mMockExitRunnable).run();
    }
}
