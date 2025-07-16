// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;

import android.content.Context;
import android.content.res.Resources;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.dialogs.OpenDownloadDialog.OpenDownloadDialogEvent;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link OpenDownloadDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OpenDownloadDialogTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Callback<Integer> mResultCallback;

    private Context mContext;
    private Resources mResources;
    private OpenDownloadDialog mDialog;
    private PropertyModel mModalDialogModel;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mResources = mContext.getResources();
    }

    /**
     * Tests the modal dialog properties when no specific app is available and auto-open is
     * disabled.
     */
    @Test
    public void testDialogProperties_noApp_autoOpenDisabled() {
        createAndShowDialog(false, null);

        Assert.assertEquals(
                "Dialog title should be generic.",
                mResources.getString(R.string.open_download_dialog_title),
                mModalDialogModel.get(ModalDialogProperties.TITLE));
        Assert.assertEquals(
                "Positive button text should be 'Continue'.",
                mResources.getString(R.string.open_download_dialog_continue_text),
                mModalDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertEquals(
                "Negative button text should be 'Cancel'.",
                mResources.getString(R.string.open_download_dialog_cancel_text),
                mModalDialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        Assert.assertEquals(
                "Checkbox text should be set.",
                mResources.getString(R.string.open_download_dialog_auto_open_text),
                mModalDialogModel.get(ModalDialogProperties.CHECKBOX_TEXT));
        Assert.assertFalse(
                "Checkbox should be unchecked.",
                mModalDialogModel.get(ModalDialogProperties.CHECKBOX_CHECKED));
    }

    /**
     * Tests the modal dialog properties when a specific app is available and auto-open is enabled.
     */
    @Test
    public void testDialogProperties_withApp_autoOpenEnabled() {
        final String appName = "PDF Reader";
        createAndShowDialog(true, appName);

        Assert.assertEquals(
                "Dialog title should include the app name.",
                mResources.getString(R.string.open_download_with_app_dialog_title, appName),
                mModalDialogModel.get(ModalDialogProperties.TITLE));
        Assert.assertEquals(
                "Positive button text should be 'Open'.",
                mResources.getString(R.string.open_download_dialog_open_text),
                mModalDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertTrue(
                "Checkbox should be checked.",
                mModalDialogModel.get(ModalDialogProperties.CHECKBOX_CHECKED));
    }

    /** Tests that the "Just Once" action is triggered with the positive button. */
    @Test
    public void testPositiveButton_justOnce() {
        createAndShowDialog(false, null);
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);

        // Simulate the click, which will lead to a dismiss call.
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
        Mockito.verify(mModalDialogManager)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        // Simulate the dismissal, which triggers the result callback.
        dialogController.onDismiss(mModalDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        Mockito.verify(mResultCallback)
                .onResult(OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_JUST_ONCE);
        Mockito.verify(mResultCallback, never())
                .onResult(OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_ALWAYS_OPEN);
    }

    /** Tests that the "Always Open" action is triggered with the positive button. */
    @Test
    public void testPositiveButton_alwaysOpen() {
        createAndShowDialog(true, null);
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);

        // Simulate the click, which will lead to a dismiss call.
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
        Mockito.verify(mModalDialogManager)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        // Simulate the dismissal, which triggers the result callback.
        dialogController.onDismiss(mModalDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        Mockito.verify(mResultCallback)
                .onResult(OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_ALWAYS_OPEN);
        Mockito.verify(mResultCallback, never())
                .onResult(OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_JUST_ONCE);
    }

    /** Tests that the dialog is dismissed with the negative button. */
    @Test
    public void testNegativeButton_dismiss() {
        createAndShowDialog(false, null);
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);

        // Simulate the click.
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
        Mockito.verify(mModalDialogManager)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);

        // Simulate the dismissal.
        dialogController.onDismiss(mModalDialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        Mockito.verify(mResultCallback)
                .onResult(OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_DISMISS);
    }

    /** Tests that the dialog dismissal by other means calls the correct callback. */
    @Test
    public void testDismissal_sendsDismissEvent() {
        createAndShowDialog(false, null);
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);

        // Simulate a generic dismissal (e.g., back press).
        dialogController.onDismiss(mModalDialogModel, DialogDismissalCause.NAVIGATE_BACK);
        Mockito.verify(mResultCallback)
                .onResult(OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_DISMISS);
        Mockito.verify(mResultCallback, never())
                .onResult(OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_ALWAYS_OPEN);
        Mockito.verify(mResultCallback, never())
                .onResult(OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_JUST_ONCE);
    }

    /**
     * Helper function that creates an {@link OpenDownloadDialog}, calls show(), and captures the
     * property model for the modal dialog view.
     */
    private void createAndShowDialog(boolean autoOpenEnabled, String appName) {
        mDialog = new OpenDownloadDialog();
        mDialog.show(mContext, mModalDialogManager, autoOpenEnabled, appName, mResultCallback);

        ArgumentCaptor<PropertyModel> captor = ArgumentCaptor.forClass(PropertyModel.class);
        Mockito.verify(mModalDialogManager)
                .showDialog(captor.capture(), eq(ModalDialogManager.ModalDialogType.TAB));
        mModalDialogModel = captor.getValue();
    }
}
