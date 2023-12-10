// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.settings.AutofillDeleteCreditCardConfirmationDialog;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Unit tests for {@link AutofillDeleteCreditCardConfirmationDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillDeleteCreditCardConfirmationDialogTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Callback<Integer> mResultHandlerMock;

    private FakeModalDialogManager mModalDialogManager;
    private AutofillDeleteCreditCardConfirmationDialog mDialog;

    @Before
    public void setUp() {
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
        mDialog =
                new AutofillDeleteCreditCardConfirmationDialog(
                        mModalDialogManager,
                        ApplicationProvider.getApplicationContext(),
                        mResultHandlerMock);
        mDialog.show();
    }

    @Test
    @SmallTest
    public void testDeleteButtonPressed_handlesPositiveDialogDismissalCause() {
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        assertThat(dialogModel).isNotNull();
        mModalDialogManager.clickPositiveButton();
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        verify(mResultHandlerMock).onResult(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void testCancelButtonPressed_handlesNegativeDialogDismissalCause() {
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        assertThat(dialogModel).isNotNull();
        mModalDialogManager.clickNegativeButton();
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        verify(mResultHandlerMock).onResult(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }
}
