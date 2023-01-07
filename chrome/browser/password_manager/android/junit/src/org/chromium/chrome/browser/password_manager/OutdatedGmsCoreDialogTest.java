// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 *  Tests for the outdated GMS Core dialog.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OutdatedGmsCoreDialogTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Callback<Boolean> mResultHandler;

    private ModalDialogManager mModalDialogManager;

    private OutdatedGmsCoreDialog mOutdatedGmsCoreDialog;

    @Before
    public void setUp() throws Exception {
        mModalDialogManager = new ModalDialogManager(
                mock(ModalDialogManager.Presenter.class), ModalDialogManager.ModalDialogType.APP);
        mOutdatedGmsCoreDialog = new OutdatedGmsCoreDialog(
                mModalDialogManager, ApplicationProvider.getApplicationContext(), mResultHandler);
    }

    @Test
    public void testHandlesUpdateButtonClick() {
        mOutdatedGmsCoreDialog.show();
        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();
        assertNotNull(dialogModel);
        dialogModel.get(ModalDialogProperties.CONTROLLER)
                .onClick(dialogModel, ModalDialogProperties.ButtonType.POSITIVE);

        verify(mResultHandler).onResult(true);
        assertNull(mModalDialogManager.getCurrentDialogForTest());
    }

    @Test
    public void testHandlesNegativeButtonClick() {
        mOutdatedGmsCoreDialog.show();
        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();
        assertNotNull(dialogModel);
        dialogModel.get(ModalDialogProperties.CONTROLLER)
                .onClick(dialogModel, ModalDialogProperties.ButtonType.NEGATIVE);

        verify(mResultHandler).onResult(false);
        assertNull(mModalDialogManager.getCurrentDialogForTest());
    }

    @Test
    public void testHandlesDialogDismiss() {
        mOutdatedGmsCoreDialog.show();
        mModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);

        verify(mResultHandler).onResult(false);
        assertNull(mModalDialogManager.getCurrentDialogForTest());
    }
}
