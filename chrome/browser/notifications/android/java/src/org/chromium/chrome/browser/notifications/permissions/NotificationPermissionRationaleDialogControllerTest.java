// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.NotificationRationaleResult;
import org.chromium.chrome.browser.notifications.R;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController.RationaleUiResult;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link NotificationPermissionRationaleDialogController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NotificationPermissionRationaleDialogControllerTest {
    private ModalDialogManager mModalDialogManager;
    private Context mContext;

    @Before
    public void setUp() {
        mModalDialogManager =
                new ModalDialogManager(Mockito.mock(ModalDialogManager.Presenter.class), 0);
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testShowDialog() {
        NotificationPermissionRationaleDialogController dialog =
                new NotificationPermissionRationaleDialogController(mContext, mModalDialogManager);

        // Show the dialog, we don't dismiss it so the callback shouldn't be called.
        dialog.showRationaleUi(
                result -> {
                    assert false;
                });

        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();

        assertNotNull(dialogModel);

        // The dialog should have no title or message, because we set those on CUSTOM_VIEW.
        assertNull(dialogModel.get(ModalDialogProperties.TITLE));
        assertNull(dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1));

        assertEquals(
                ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE,
                dialogModel.get(ModalDialogProperties.BUTTON_STYLES));

        assertTrue(dialogModel.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));

        View dialogContentsView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        View dialogTitle =
                dialogContentsView.findViewById(R.id.notification_permission_rationale_title);
        View dialogMessage =
                dialogContentsView.findViewById(R.id.notification_permission_rationale_message);

        // Check that the custom view contains the expected title and message.
        assertThat(dialogTitle, withText(R.string.notification_permission_rationale_dialog_title));
        assertThat(
                dialogMessage, withText(R.string.notification_permission_rationale_dialog_message));
    }

    @Test
    public void testRejectDialog() {
        NotificationPermissionRationaleDialogController dialog =
                new NotificationPermissionRationaleDialogController(mContext, mModalDialogManager);

        Callback<Integer> mockCallback = Mockito.mock(Callback.class);
        dialog.showRationaleUi(mockCallback);

        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();

        mModalDialogManager.dismissDialog(
                dialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        verify(mockCallback).onResult(RationaleUiResult.REJECTED);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.RationaleResult",
                        NotificationRationaleResult.NEGATIVE_BUTTON_CLICKED));
    }

    @Test
    public void testDismissDialog() {
        NotificationPermissionRationaleDialogController dialog =
                new NotificationPermissionRationaleDialogController(mContext, mModalDialogManager);

        Callback<Integer> mockCallback = Mockito.mock(Callback.class);
        dialog.showRationaleUi(mockCallback);

        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();

        mModalDialogManager.dismissDialog(
                dialogModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        verify(mockCallback).onResult(RationaleUiResult.REJECTED);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.RationaleResult",
                        NotificationRationaleResult.NAVIGATE_BACK_OR_TOUCH_OUTSIDE));
    }

    @Test
    public void testAcceptDialog() {
        NotificationPermissionRationaleDialogController dialog =
                new NotificationPermissionRationaleDialogController(mContext, mModalDialogManager);

        Callback<Integer> mockCallback = Mockito.mock(Callback.class);
        dialog.showRationaleUi(mockCallback);

        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();

        mModalDialogManager.dismissDialog(
                dialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        verify(mockCallback).onResult(RationaleUiResult.ACCEPTED);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.RationaleResult",
                        NotificationRationaleResult.POSITIVE_BUTTON_CLICKED));
    }

    @Test
    public void testResultOnActivityDestroy() {
        NotificationPermissionRationaleDialogController dialog =
                new NotificationPermissionRationaleDialogController(mContext, mModalDialogManager);

        Callback<Integer> mockCallback = Mockito.mock(Callback.class);
        dialog.showRationaleUi(mockCallback);

        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();

        mModalDialogManager.dismissDialog(dialogModel, DialogDismissalCause.ACTIVITY_DESTROYED);
        verify(mockCallback).onResult(RationaleUiResult.REJECTED);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.RationaleResult",
                        NotificationRationaleResult.ACTIVITY_DESTROYED));
    }

    @Test
    public void testResultOnViewDetachedFromWindow() {
        NotificationPermissionRationaleDialogController dialog =
                new NotificationPermissionRationaleDialogController(mContext, mModalDialogManager);

        Callback<Integer> mockCallback = Mockito.mock(Callback.class);
        dialog.showRationaleUi(mockCallback);

        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();

        mModalDialogManager.dismissDialog(dialogModel, DialogDismissalCause.NOT_ATTACHED_TO_WINDOW);
        verify(mockCallback).onResult(RationaleUiResult.REJECTED);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.RationaleResult",
                        NotificationRationaleResult.NOT_ATTACHED_TO_WINDOW));
    }
}
