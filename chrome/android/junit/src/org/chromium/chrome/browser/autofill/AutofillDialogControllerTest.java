// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.ConfirmationDialogHandler;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.ConfirmationDialogParams;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.DismissHandler;
import org.chromium.components.browser_ui.widget.StrictButtonPressController.ButtonClickResult;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link AutofillDialogController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class AutofillDialogControllerTest {
    private static final long NATIVE_AUTOFILL_DIALOG_VIEW = 1234L;
    private static final String TEST_TITLE = "Test Title";
    private static final String TEST_DESCRIPTION = "Test Description";
    private static final String TEST_BUTTON_TEXT = "Test Button";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutofillDialogController.Natives mNativeMock;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ActionConfirmationDialog mDialog;
    @Mock private DismissHandler mDismissHandler;

    @Captor private ArgumentCaptor<ConfirmationDialogParams> mDialogParamsCaptor;
    @Captor private ArgumentCaptor<ConfirmationDialogHandler> mDialogHandlerCaptor;

    private Activity mActivity;
    private AutofillDialogController mController;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
        AutofillDialogControllerJni.setInstanceForTesting(mNativeMock);
        mController =
                new AutofillDialogController(
                        NATIVE_AUTOFILL_DIALOG_VIEW, mActivity, mModalDialogManager, mDialog);
    }

    @Test
    public void testShowDialog_RendersCorrectly() {
        mController.show(TEST_TITLE, TEST_DESCRIPTION, TEST_BUTTON_TEXT);

        verify(mDialog).show(mDialogParamsCaptor.capture(), any());
        ConfirmationDialogParams dialogParams = mDialogParamsCaptor.getValue();

        ConfirmationDialogParams expectedParams =
                new ConfirmationDialogParams.Builder(mActivity)
                        .withTitle(TEST_TITLE)
                        .withDescription(TEST_DESCRIPTION)
                        .withPositiveButton(TEST_BUTTON_TEXT)
                        .build();
        assertEquals(expectedParams, dialogParams);
    }

    @Test
    public void testPositiveButtonClick_CallsNative() {
        mController.show(TEST_TITLE, TEST_DESCRIPTION, TEST_BUTTON_TEXT);

        verify(mDialog).show(any(), mDialogHandlerCaptor.capture());
        ConfirmationDialogHandler dialogHelper = mDialogHandlerCaptor.getValue();
        dialogHelper.onDialogInteracted(
                mDismissHandler, ButtonClickResult.POSITIVE, /* stopShowing= */ false);

        // Simulate dismissal after positive button click.
        verify(mNativeMock).onPositiveButtonClicked(NATIVE_AUTOFILL_DIALOG_VIEW);
        // The onDismissed should only be called if the dialog was dismissed by the user.
        verify(mNativeMock).onDismissed(NATIVE_AUTOFILL_DIALOG_VIEW);
    }

    @Test
    public void testNegativeButtonClick_CallsNativeOnDismissed() {
        mController.show(TEST_TITLE, TEST_DESCRIPTION, TEST_BUTTON_TEXT);

        verify(mDialog).show(any(), mDialogHandlerCaptor.capture());
        ConfirmationDialogHandler dialogHelper = mDialogHandlerCaptor.getValue();
        dialogHelper.onDialogInteracted(
                mDismissHandler, ButtonClickResult.NEGATIVE, /* stopShowing= */ false);

        // Simulate dismissal after negative button click.
        verify(mNativeMock, times(0)).onPositiveButtonClicked(NATIVE_AUTOFILL_DIALOG_VIEW);
        // The onDismissed should only be called if the dialog was dismissed by the user.
        verify(mNativeMock).onDismissed(NATIVE_AUTOFILL_DIALOG_VIEW);
    }

    @Test
    public void testDismiss_DismissesDialog() {
        mController.show(TEST_TITLE, TEST_DESCRIPTION, TEST_BUTTON_TEXT);
        mController.dismiss();

        verify(mModalDialogManager).dismissAllDialogs(DialogDismissalCause.DISMISSED_BY_NATIVE);
        // The onDismissed should only be called if the dialog was dismissed by the user.
        verify(mNativeMock, times(0)).onDismissed(NATIVE_AUTOFILL_DIALOG_VIEW);
    }
}
