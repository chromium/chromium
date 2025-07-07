// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.versioning;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.modaldialog.ModalDialogProperties.CONTROLLER;

import android.content.Context;

import org.junit.Before;
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

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    private PropertyModel mModel;
    private Controller mController;

    @Before
    public void setUp() {
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
        mController.onClick(mModel, ButtonType.POSITIVE);
        verify(mContext).startActivity(any());
    }

    @Test
    public void testShow_negativeButton() {
        mController.onClick(mModel, ButtonType.NEGATIVE);
        verify(mContext, never()).startActivity(any());
    }

    @Test
    public void testShow_clickOutside() {
        mController.onDismiss(mModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        verify(mContext, never()).startActivity(any());
    }
}
