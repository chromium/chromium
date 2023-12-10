// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Unit tests for {@link AutofillDeleteSavedCvcsConfirmationDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillDeleteSavedCvcsConfirmationDialogTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Boolean> mCallbackMock;
    private FakeModalDialogManager mModalDialogManager;
    private AutofillDeleteSavedCvcsConfirmationDialog mDialog;

    @Before
    public void setUp() {
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
        mDialog =
                new AutofillDeleteSavedCvcsConfirmationDialog(
                        ApplicationProvider.getApplicationContext(),
                        mModalDialogManager,
                        mCallbackMock);
    }

    @Test
    @SmallTest
    public void testDialogIsShown() {
        mDialog.show();

        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        verify(mCallbackMock, never()).onResult(any());
    }

    @Test
    @SmallTest
    public void testDialogShowsTitleMessageAndButtonLabels() {
        mDialog.show();

        Context context = RuntimeEnvironment.application.getApplicationContext();
        String dialogTitle =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE);
        assertThat(dialogTitle)
                .isEqualTo(
                        context.getString(
                                R.string.autofill_delete_saved_cvcs_confirmation_dialog_title));
        String dialogMessage =
                (String)
                        mModalDialogManager
                                .getShownDialogModel()
                                .get(ModalDialogProperties.MESSAGE_PARAGRAPH_1);
        assertThat(dialogMessage)
                .isEqualTo(
                        context.getString(
                                R.string.autofill_delete_saved_cvcs_confirmation_dialog_message));
        String positiveButtonLabel =
                mModalDialogManager
                        .getShownDialogModel()
                        .get(ModalDialogProperties.POSITIVE_BUTTON_TEXT);
        assertThat(positiveButtonLabel)
                .isEqualTo(
                        context.getString(
                                R.string
                                        .autofill_delete_saved_cvcs_confirmation_dialog_delete_button_label));
        String negativeButtonLabel =
                mModalDialogManager
                        .getShownDialogModel()
                        .get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT);
        assertThat(negativeButtonLabel).isEqualTo(context.getString(android.R.string.cancel));
    }

    @Test
    @SmallTest
    public void testDeleteButton_whenPressed_closesDialogAndCallbackReceivesTrue() {
        mDialog.show();

        mModalDialogManager.clickPositiveButton();

        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        verify(mCallbackMock).onResult(true);
    }

    @Test
    @SmallTest
    public void testCancelButton_whenPressed_closesDialogAndCallbackReceivesFalse() {
        mDialog.show();

        mModalDialogManager.clickNegativeButton();

        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        verify(mCallbackMock).onResult(false);
    }
}
