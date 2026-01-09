// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Unit tests for {@link PermissionBlockedDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PermissionBlockedDialogTest {
    @Mock private PermissionBlockedDialog.Natives mNativeMock;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ModalDialogManager mModalDialogManager;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    private Activity mActivity;
    private PermissionBlockedDialog mDialog;
    private static final long NATIVE_CONTROLLER = 12345L;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PermissionBlockedDialogJni.setInstanceForTesting(mNativeMock);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);

        mDialog = new PermissionBlockedDialog(NATIVE_CONTROLLER, mWindowAndroid);
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
}
