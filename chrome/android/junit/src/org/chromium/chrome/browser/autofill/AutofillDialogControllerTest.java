// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.widget.TextView;

import org.jni_zero.JniRawPtr;
import org.junit.After;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillDialogController.NativeAutofillDialogViewAndroid;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.ConfirmationDialogHandler;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.ConfirmationDialogParams;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.DismissHandler;
import org.chromium.components.browser_ui.widget.StrictButtonPressController.ButtonClickResult;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Unit tests for {@link AutofillDialogController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillDialogControllerTest {
    private static final String TEST_TITLE = "Test Title";
    private static final String TEST_DESCRIPTION = "Test Description";
    private static final String TEST_NEGATIVE_BUTTON_TEXT = "Test Negative Button";
    private static final String TEST_POSITIVE_BUTTON_TEXT = "Test Positive Button";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private JniRawPtr<NativeAutofillDialogViewAndroid> mNativeAutofillDialogView;
    @Mock private AutofillDialogController.Natives mNativeMock;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ActionConfirmationDialog mDialog;
    @Mock private DismissHandler mDismissHandler;

    @Captor private ArgumentCaptor<ConfirmationDialogParams> mDialogParamsCaptor;
    @Captor private ArgumentCaptor<ConfirmationDialogHandler> mDialogHandlerCaptor;
    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    private Activity mActivity;
    private AutofillDialogController mController;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
        AutofillDialogControllerJni.setInstanceForTesting(mNativeMock);
        mNativeAutofillDialogView = spy(JniRawPtr.createForTesting(100L));
        mController =
                new AutofillDialogController(
                        mNativeAutofillDialogView, mActivity, mModalDialogManager, mDialog);
    }

    @After
    public void tearDown() {
        mController.dismiss();
    }

    @Test
    public void testShowDialog_RendersCorrectly() {
        mController.show(
                TEST_TITLE, TEST_DESCRIPTION, TEST_POSITIVE_BUTTON_TEXT, TEST_NEGATIVE_BUTTON_TEXT);

        verify(mDialog).show(mDialogParamsCaptor.capture(), any());
        ConfirmationDialogParams dialogParams = mDialogParamsCaptor.getValue();

        ConfirmationDialogParams expectedParams =
                new ConfirmationDialogParams.Builder(mActivity)
                        .withTitle(TEST_TITLE)
                        .withDescription(TEST_DESCRIPTION)
                        .withPositiveButton(TEST_POSITIVE_BUTTON_TEXT)
                        .withNegativeButton(TEST_NEGATIVE_BUTTON_TEXT)
                        .build();
        assertEquals(expectedParams, dialogParams);
    }

    @Test
    public void testPositiveButtonClick_CallsNative() {
        mController.show(
                TEST_TITLE, TEST_DESCRIPTION, TEST_POSITIVE_BUTTON_TEXT, TEST_NEGATIVE_BUTTON_TEXT);

        verify(mDialog).show(any(), mDialogHandlerCaptor.capture());
        ConfirmationDialogHandler dialogHelper = mDialogHandlerCaptor.getValue();
        dialogHelper.onDialogInteracted(
                mDismissHandler, ButtonClickResult.POSITIVE, /* stopShowing= */ false);

        // Simulate dismissal after positive button click.
        verify(mNativeMock).onPositiveButtonClicked(same(mNativeAutofillDialogView));
        verify(mNativeMock, never()).onNegativeButtonClicked(any());
        // The onDismissed should only be called if the dialog was dismissed by the user.
        verify(mNativeMock).onDismissed(same(mNativeAutofillDialogView));
        verify(mNativeAutofillDialogView).release();
    }

    @Test
    public void testNegativeButtonClick_CallsNative() {
        mController.show(
                TEST_TITLE, TEST_DESCRIPTION, TEST_POSITIVE_BUTTON_TEXT, TEST_NEGATIVE_BUTTON_TEXT);

        verify(mDialog).show(any(), mDialogHandlerCaptor.capture());
        ConfirmationDialogHandler dialogHelper = mDialogHandlerCaptor.getValue();
        dialogHelper.onDialogInteracted(
                mDismissHandler, ButtonClickResult.NEGATIVE, /* stopShowing= */ false);

        // Simulate dismissal after negative button click.
        verify(mNativeMock, never()).onPositiveButtonClicked(any());
        verify(mNativeMock).onNegativeButtonClicked(same(mNativeAutofillDialogView));
        // The onDismissed should only be called if the dialog was dismissed by the user.
        verify(mNativeMock).onDismissed(same(mNativeAutofillDialogView));
        verify(mNativeAutofillDialogView).release();
    }

    @Test
    public void testDismiss_DismissesDialog() {
        mController.show(
                TEST_TITLE, TEST_DESCRIPTION, TEST_POSITIVE_BUTTON_TEXT, TEST_NEGATIVE_BUTTON_TEXT);
        mController.dismiss();

        verify(mModalDialogManager).dismissAllDialogs(DialogDismissalCause.DISMISSED_BY_NATIVE);
        // The onDismissed should only be called if the dialog was dismissed by the user.
        verify(mNativeMock, never()).onDismissed(any());
        verify(mNativeAutofillDialogView).release();
    }

    @Test
    public void testCreate_NullContextReleasesPointer() {
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.getContext()).thenReturn(new WeakReference<>(null));
        JniRawPtr<NativeAutofillDialogViewAndroid> nativeView =
                spy(JniRawPtr.createForTesting(200L));

        assertNull(AutofillDialogController.create(nativeView, windowAndroid));
        verify(nativeView).release();
    }

    @Test
    public void testCreate_ValidContextCreatesController() {
        Context context =
                mock(Context.class, withSettings().extraInterfaces(ModalDialogManagerHolder.class));
        when(((ModalDialogManagerHolder) context).getModalDialogManager())
                .thenReturn(mModalDialogManager);
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.getContext()).thenReturn(new WeakReference<>(context));
        JniRawPtr<NativeAutofillDialogViewAndroid> nativeView =
                spy(JniRawPtr.createForTesting(200L));

        AutofillDialogController controller =
                AutofillDialogController.create(nativeView, windowAndroid);
        assertNotNull(controller);
        verify(nativeView, never()).release();
        controller.dismiss();
    }

    @Test
    public void testShowAutofillAiLoadingDialog() {
        mController.showAutofillAiLoadingDialog(TEST_TITLE);

        verify(mModalDialogManager)
                .showDialog(
                        mPropertyModelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));
        PropertyModel dialogModel = mPropertyModelCaptor.getValue();

        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        TextView titleView = customView.findViewById(R.id.autofill_ai_loading_title);
        assertEquals(TEST_TITLE, titleView.getText());

        dialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onDismiss(dialogModel, DialogDismissalCause.UNKNOWN);
        verify(mNativeMock).onDismissed(same(mNativeAutofillDialogView));
        verify(mNativeAutofillDialogView).release();
    }
}
