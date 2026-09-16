// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.ContextThemeWrapper;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IdRes;

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
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.components.browser_ui.modaldialog.ModalDialogViewBinder;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.ref.WeakReference;
import java.time.Duration;

/** Unit tests for {@link PermissionBlockedDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PermissionBlockedDialogTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private PermissionBlockedDialog.Natives mNativeMock;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ModalDialogManager mModalDialogManager;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    private ActivityController<TestActivity> mActivityController;
    private Activity mActivity;
    private PermissionBlockedDialog mDialog;
    private static final long NATIVE_CONTROLLER = 12345L;

    @Before
    public void setUp() {
        PermissionBlockedDialogJni.setInstanceForTesting(mNativeMock);

        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        mActivity = mActivityController.get();
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);

        mDialog = new PermissionBlockedDialog(NATIVE_CONTROLLER, mWindowAndroid);
    }

    @After
    public void tearDown() {
        mActivityController.destroy();
    }

    @Test
    public void testShow() {
        String title = "Title";
        String content = "Content";
        String positiveButton = "Allow";
        String negativeButton = "Deny";
        String learnMore = "";

        mDialog.show(title, content, positiveButton, negativeButton, learnMore);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mPropertyModelCaptor.getValue();

        assertEquals(title, model.get(ModalDialogProperties.TITLE));
        assertEquals(positiveButton, model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(negativeButton, model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));

        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        assertTrue(customView instanceof TextView);
        assertEquals(content, ((TextView) customView).getText().toString());

        // The dialog can grant a permission, so it must be protected against tapjacking.
        assertTrue(model.get(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY));
        assertEquals(
                UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS,
                model.get(ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS));
    }

    @Test
    public void testPrimaryButton() {
        mDialog.show("Title", "Content", "Allow", "Deny", "");
        verify(mModalDialogManager)
                .showDialog(mPropertyModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mPropertyModelCaptor.getValue();

        mDialog.onClick(model, ButtonType.POSITIVE);

        verify(mNativeMock).onPrimaryButtonClicked(NATIVE_CONTROLLER);
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    public void testNegativeButton() {
        mDialog.show("Title", "Content", "Allow", "Deny", "");
        verify(mModalDialogManager)
                .showDialog(mPropertyModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mPropertyModelCaptor.getValue();

        mDialog.onClick(model, ButtonType.NEGATIVE);

        verify(mNativeMock).onNegativeButtonClicked(NATIVE_CONTROLLER);
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Test
    public void testDismiss() {
        PropertyModel model = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS).build();
        mDialog.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK);

        verify(mNativeMock).onDialogDismissed(NATIVE_CONTROLLER);
    }

    @Test
    public void testLearnMoreLink() {
        String content = "Content";
        String learnMore = "Learn more";
        mDialog.show("Title", content, "Allow", "Deny", learnMore);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mPropertyModelCaptor.getValue();
        TextView message = (TextView) model.get(ModalDialogProperties.CUSTOM_VIEW);

        String fullText = content + " " + learnMore;
        assertEquals(fullText, message.getText().toString());

        Spanned spanned = (Spanned) message.getText();
        ClickableSpan[] spans = spanned.getSpans(0, spanned.length(), ClickableSpan.class);
        assertEquals(1, spans.length);

        spans[0].onClick(message);
        verify(mNativeMock).onLearnMoreClicked(NATIVE_CONTROLLER);
    }

    @Test
    public void testButtonTapProtection_disablesClickDuringProtectionPeriod_positiveButton() {
        checkButtonTapProtection(
                R.id.positive_button,
                () -> verify(mNativeMock).onPrimaryButtonClicked(NATIVE_CONTROLLER),
                DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    public void testButtonTapProtection_disablesClickDuringProtectionPeriod_negativeButton() {
        checkButtonTapProtection(
                R.id.negative_button,
                () -> verify(mNativeMock).onNegativeButtonClicked(NATIVE_CONTROLLER),
                DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    private void checkButtonTapProtection(
            @IdRes int buttonId, Runnable verifyAcceptedClick, int expectedDismissalCause) {
        mDialog.show("Title", "Content", "Allow", "Deny", "");
        verify(mModalDialogManager)
                .showDialog(mPropertyModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mPropertyModelCaptor.getValue();

        ModalDialogView dialogView =
                (ModalDialogView)
                        LayoutInflater.from(
                                        new ContextThemeWrapper(
                                                mActivity,
                                                R.style
                                                        .ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton))
                                .inflate(R.layout.modal_dialog_view, null);
        PropertyModelChangeProcessor.create(model, dialogView, new ModalDialogViewBinder());

        // Attach to window to initialize the protection period timestamp.
        mActivity.setContentView(dialogView);

        View button = dialogView.findViewById(buttonId);

        // Click immediately while within protection period - should be ignored.
        button.performClick();
        verify(mNativeMock, never()).onPrimaryButtonClicked(anyLong());
        verify(mNativeMock, never()).onNegativeButtonClicked(anyLong());
        verify(mModalDialogManager, never()).dismissDialog(any(), anyInt());

        // Advance time by less than the protection period - should still be ignored.
        ShadowSystemClock.advanceBy(
                Duration.ofMillis(UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS / 2));
        button.performClick();
        verify(mNativeMock, never()).onPrimaryButtonClicked(anyLong());
        verify(mNativeMock, never()).onNegativeButtonClicked(anyLong());
        verify(mModalDialogManager, never()).dismissDialog(any(), anyInt());

        // Advance time past the protection period.
        ShadowSystemClock.advanceBy(
                Duration.ofMillis(UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS + 100));

        // Click should now be accepted and invoke the controller callback.
        button.performClick();
        verifyAcceptedClick.run();
        verify(mModalDialogManager).dismissDialog(model, expectedDismissalCause);
    }

    @Test
    public void testTouchFilterForSecurity() {
        mDialog.show("Title", "Content", "Allow", "Deny", "");
        verify(mModalDialogManager)
                .showDialog(mPropertyModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mPropertyModelCaptor.getValue();

        ModalDialogView dialogView =
                (ModalDialogView)
                        LayoutInflater.from(
                                        new ContextThemeWrapper(
                                                mActivity,
                                                R.style
                                                        .ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton))
                                .inflate(R.layout.modal_dialog_view, null);
        PropertyModelChangeProcessor.create(model, dialogView, new ModalDialogViewBinder());

        View positiveButton = dialogView.findViewById(R.id.positive_button);
        View negativeButton = dialogView.findViewById(R.id.negative_button);

        // Verify touch filtering is enabled on both buttons.
        assertTrue(positiveButton.getFilterTouchesWhenObscured());
        assertTrue(negativeButton.getFilterTouchesWhenObscured());

        // Dispatch an obscured touch event to the buttons - they should be consumed/blocked.
        MotionEvent.PointerProperties props = new MotionEvent.PointerProperties();
        props.id = 0;
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        MotionEvent obscuredEvent =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* pointerCount= */ 1,
                        new MotionEvent.PointerProperties[] {props},
                        new MotionEvent.PointerCoords[] {coords},
                        /* metaState= */ 0,
                        /* buttonState= */ 0,
                        /* xPrecision= */ 1.0f,
                        /* yPrecision= */ 1.0f,
                        /* deviceId= */ 0,
                        /* edgeFlags= */ 0,
                        /* source= */ InputDevice.SOURCE_CLASS_POINTER,
                        /* flags= */ MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED);
        assertTrue(positiveButton.dispatchTouchEvent(obscuredEvent));
        assertTrue(negativeButton.dispatchTouchEvent(obscuredEvent));
    }
}
