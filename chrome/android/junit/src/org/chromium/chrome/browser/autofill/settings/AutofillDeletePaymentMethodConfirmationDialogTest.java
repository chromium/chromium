// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.util.Arrays;
import java.util.Collection;

/** Unit tests for {@link AutofillDeletePaymentMethodConfirmationDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillDeletePaymentMethodConfirmationDialogTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Callback<Integer> mResultHandlerMock;

    private FakeModalDialogManager mModalDialogManager;
    private AutofillDeletePaymentMethodConfirmationDialog mDialog;

    @Before
    public void setUp() {
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
    }

    @Test
    @SmallTest
    public void testDeleteButtonPressed_handlesPositiveDialogDismissalCause() {
        mDialog =
                new AutofillDeletePaymentMethodConfirmationDialog(
                        mModalDialogManager,
                        ApplicationProvider.getApplicationContext(),
                        mResultHandlerMock,
                        /* titleResId= */ R.string.autofill_credit_card_delete_confirmation_title);
        mDialog.show();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        assertThat(dialogModel).isNotNull();
        mModalDialogManager.clickPositiveButton();
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        verify(mResultHandlerMock).onResult(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void testCancelButtonPressed_handlesNegativeDialogDismissalCause() {
        mDialog =
                new AutofillDeletePaymentMethodConfirmationDialog(
                        mModalDialogManager,
                        ApplicationProvider.getApplicationContext(),
                        mResultHandlerMock,
                        /* titleResId= */ R.string.autofill_credit_card_delete_confirmation_title);
        mDialog.show();
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        assertThat(dialogModel).isNotNull();
        mModalDialogManager.clickNegativeButton();
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        verify(mResultHandlerMock).onResult(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @RunWith(Parameterized.class)
    public class ParameterizedTests {
        @Parameterized.Parameter public int mTitleResId;

        @Parameterized.Parameters
        public static Collection<Object[]> data() {
            return Arrays.asList(
                    new Object[][] {
                        {
                            R.string.autofill_credit_card_delete_confirmation_title,
                            R.string.autofill_iban_delete_confirmation_title
                        }
                    });
        }

        @Test
        @SmallTest
        public void testDialogShowsPaymentMethodTitle() {
            mDialog =
                    new AutofillDeletePaymentMethodConfirmationDialog(
                            mModalDialogManager,
                            ApplicationProvider.getApplicationContext(),
                            mResultHandlerMock,
                            mTitleResId);
            mDialog.show();

            Context context = RuntimeEnvironment.application.getApplicationContext();
            String dialogTitle =
                    mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE);
            assertThat(dialogTitle).isEqualTo(context.getString(mTitleResId));
        }
    }
}
