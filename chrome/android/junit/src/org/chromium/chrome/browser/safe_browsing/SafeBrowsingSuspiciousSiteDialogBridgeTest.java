// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.text.Spanned;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;

import java.lang.ref.WeakReference;

/** Unit tests for {@link SafeBrowsingSuspiciousSiteDialogBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafeBrowsingSuspiciousSiteDialogBridgeTest {
    private static final String TITLE = "Suspicious site";
    private static final String DETAILS =
            "This site may be unsafe. <link>Learn more</link> about the warning.";
    private static final String DETAILS_WITHOUT_TAGS =
            "This site may be unsafe. Learn more about the warning.";
    private static final String PRIMARY_BUTTON_TEXT = "Back to safety";
    private static final String SECONDARY_BUTTON_TEXT = "Mark as safe";
    private static final long NATIVE_PTR = 1L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private SafeBrowsingSuspiciousSiteDialogBridge.Natives mNativeMock;

    private Activity mActivity;
    private SafeBrowsingSuspiciousSiteDialogBridge mBridge;

    @Before
    public void setUp() {
        SafeBrowsingSuspiciousSiteDialogBridgeJni.setInstanceForTesting(mNativeMock);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);

        mBridge = SafeBrowsingSuspiciousSiteDialogBridge.createForTests(mWindowAndroid, NATIVE_PTR);
    }

    private PropertyModel showDialogAndGetModel() {
        mBridge.showDialog(TITLE, DETAILS, PRIMARY_BUTTON_TEXT, SECONDARY_BUTTON_TEXT);
        PropertyModel model = mBridge.getDialogModelForTesting();
        Assert.assertNotNull("Dialog model should have been created.", model);
        return model;
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowDialog_largeFormFactor_usesStandardDialogProperties() {
        PropertyModel model = showDialogAndGetModel();

        Assert.assertEquals(TITLE, model.get(ModalDialogProperties.TITLE));
        Assert.assertEquals(
                DETAILS_WITHOUT_TAGS,
                model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1).toString());
        Assert.assertEquals(
                PRIMARY_BUTTON_TEXT, model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertEquals(
                SECONDARY_BUTTON_TEXT, model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        Assert.assertNull(model.get(ModalDialogProperties.CONTENT_DESCRIPTION));
        verify(mModalDialogManager).showDialog(model, ModalDialogManager.ModalDialogType.TAB);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowDialog_largeFormFactor_hasNoCustomViewOrCloseButton() {
        PropertyModel model = showDialogAndGetModel();

        // The framework renders the dialog, so there is no custom view and therefore no
        // custom close 'x' affordance.
        Assert.assertNull(model.get(ModalDialogProperties.CUSTOM_VIEW));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowDialog_largeFormFactor_usesFilledPrimaryButtonStyle() {
        PropertyModel model = showDialogAndGetModel();

        Assert.assertEquals(
                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE,
                model.get(ModalDialogProperties.BUTTON_STYLES));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testShowDialog_phone_usesCustomViewWithCloseButton() {
        PropertyModel model = showDialogAndGetModel();

        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        Assert.assertNotNull("Phones should keep the custom view.", customView);
        Assert.assertNotNull(customView.findViewById(R.id.close_button));
        // Title, message and buttons live inside the custom view, not on the model.
        Assert.assertNull(model.get(ModalDialogProperties.TITLE));
        Assert.assertNull(model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1));
        Assert.assertNull(model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertNull(model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowDialog_largeFormFactor_doesNotCancelOnTouchOutside() {
        PropertyModel model = showDialogAndGetModel();

        // Users must interact with the dialog or navigate back; tapping the scrim must not
        // dismiss the warning.
        Assert.assertFalse(model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testShowDialog_phone_doesNotCancelOnTouchOutside() {
        PropertyModel model = showDialogAndGetModel();

        Assert.assertFalse(model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowDialog_largeFormFactor_appliesLearnMoreLink() {
        PropertyModel model = showDialogAndGetModel();

        CharSequence message = model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1);
        Assert.assertTrue(message instanceof Spanned);
        ChromeClickableSpan[] spans =
                ((Spanned) message).getSpans(0, message.length(), ChromeClickableSpan.class);
        Assert.assertEquals("Expected exactly one 'learn more' link span.", 1, spans.length);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testOnClick_positiveButton_dismissesAndCallsGoBack() {
        PropertyModel model = showDialogAndGetModel();

        mBridge.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        mBridge.onDismiss(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        verify(mNativeMock).goBack(NATIVE_PTR);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testOnClick_negativeButton_dismissesAndCallsContinueAnyway() {
        PropertyModel model = showDialogAndGetModel();

        mBridge.onClick(model, ModalDialogProperties.ButtonType.NEGATIVE);
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);

        mBridge.onDismiss(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        verify(mNativeMock).continueAnyway(NATIVE_PTR);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testOnDismiss_nonButtonCause_callsClose() {
        PropertyModel model = showDialogAndGetModel();

        mBridge.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mNativeMock).close(NATIVE_PTR, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
    }
}
