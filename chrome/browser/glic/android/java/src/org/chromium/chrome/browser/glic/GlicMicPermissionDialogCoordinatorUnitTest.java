// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;
import android.text.Spanned;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;

/** Unit tests for {@link GlicMicPermissionDialogCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicMicPermissionDialogCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager.Presenter mPresenterMock;
    @Mock private Callback<Boolean> mCallbackMock;

    private TestActivity mActivity;
    private ModalDialogManager mModalDialogManager;
    private GlicMicPermissionDialogCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mModalDialogManager = new ModalDialogManager(mPresenterMock, ModalDialogType.APP);
        mCoordinator = new GlicMicPermissionDialogCoordinator(mActivity, mModalDialogManager);
    }

    private PropertyModel showDialog() {
        mCoordinator.show(mCallbackMock);
        PropertyModel model = mModalDialogManager.getCurrentDialogForTest();
        assertNotNull("The dialog should be showing.", model);
        return model;
    }

    @Test
    public void testShow_DialogContents() {
        PropertyModel model = showDialog();

        assertEquals(
                mActivity.getString(R.string.glic_mic_permission_dialog_allow),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                mActivity.getString(R.string.no_thanks),
                model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));

        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        assertNotNull(customView);
        TextView titleView = customView.findViewById(R.id.mic_dialog_title);
        assertEquals(
                mActivity.getString(R.string.glic_mic_permission_dialog_title),
                titleView.getText().toString());
    }

    @Test
    public void testShow_AllowButton_ReturnsTrue() {
        PropertyModel model = showDialog();

        model.get(ModalDialogProperties.CONTROLLER).onClick(model, ButtonType.POSITIVE);

        verify(mCallbackMock).onResult(true);
        assertNull(
                "The dialog should be dismissed.", mModalDialogManager.getCurrentDialogForTest());
    }

    @Test
    public void testShow_NoThanksButton_ReturnsFalse() {
        PropertyModel model = showDialog();

        model.get(ModalDialogProperties.CONTROLLER).onClick(model, ButtonType.NEGATIVE);

        verify(mCallbackMock).onResult(false);
        assertNull(
                "The dialog should be dismissed.", mModalDialogManager.getCurrentDialogForTest());
    }

    @Test
    public void testShow_DismissedWithoutButtonClick_ReturnsFalse() {
        PropertyModel model = showDialog();

        // Tapping outside / backing out of an app-modal dialog.
        mModalDialogManager.dismissDialog(
                model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mCallbackMock).onResult(false);
    }

    @Test
    public void testShow_CallbackRunsAtMostOnce() {
        PropertyModel model = showDialog();

        model.get(ModalDialogProperties.CONTROLLER).onClick(model, ButtonType.POSITIVE);
        // A second dismissal (e.g. the activity going away) must not re-run the native callback.
        mModalDialogManager.dismissDialog(model, DialogDismissalCause.ACTIVITY_DESTROYED);

        verify(mCallbackMock, times(1)).onResult(anyBoolean());
    }

    @Test
    public void testShow_SettingsLink_OpensAppInfoAndDismisses() {
        PropertyModel model = showDialog();
        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        TextView messageView = customView.findViewById(R.id.mic_dialog_message);
        Spanned message = (Spanned) messageView.getText();
        ChromeClickableSpan[] spans =
                message.getSpans(0, message.length(), ChromeClickableSpan.class);
        assertEquals("The message should contain a single link.", 1, spans.length);

        spans[0].onClick(messageView);

        Intent intent = Shadows.shadowOf(mActivity).getNextStartedActivity();
        assertNotNull("App info settings should have been opened.", intent);
        assertEquals(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, intent.getAction());
        assertEquals(Uri.parse("package:" + mActivity.getPackageName()), intent.getData());

        assertNull(
                "The dialog should be dismissed.", mModalDialogManager.getCurrentDialogForTest());
        verify(mCallbackMock).onResult(false);
    }
}
