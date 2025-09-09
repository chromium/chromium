// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowDialog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.UploadImagePreviewCoordinator.CropResultCallback;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link UploadImagePreviewCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UploadImagePreviewCoordinatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CropResultCallback mMockOnConfirm;
    @Mock private Runnable mMockOnCancel;

    private Dialog mDialog;
    private UploadImagePreviewCoordinator mUploadImagePreviewCoordinator;
    private View mSaveButton;
    private View mCancelButton;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        mUploadImagePreviewCoordinator =
                new UploadImagePreviewCoordinator(activity, bitmap, mMockOnConfirm, mMockOnCancel);
        mDialog = ShadowDialog.getLatestDialog();
        View contentView = mDialog.findViewById(android.R.id.content);
        mSaveButton = contentView.findViewById(R.id.save_button);
        mCancelButton = contentView.findViewById(R.id.cancel_button);
    }

    @Test
    public void constructor_showsDialog() {
        assertNotNull("Dialog should have been created and shown.", mDialog);
        assertTrue("Dialog should be showing.", mDialog.isShowing());
    }

    @Test
    public void testClickSaveButton_invokesOnConfirmCallback() {
        mSaveButton.performClick();

        verify(mMockOnConfirm).onCropResult(any(Matrix.class), any(Matrix.class));

        // Verify the other callback was not invoked.
        verify(mMockOnCancel, never()).run();

        // Verify the dialog was dismissed.
        assertFalse("Dialog should be dismissed after clicking save.", mDialog.isShowing());
    }

    @Test
    public void clickCancelButton_invokesOnCancelCallback() {
        mCancelButton.performClick();

        verify(mMockOnCancel).run();

        // Verify the other callback and its side-effects were not invoked.
        verify(mMockOnConfirm, never()).onCropResult(any(), any());

        // Verify the dialog was dismissed.
        assertFalse("Dialog should be dismissed after clicking cancel.", mDialog.isShowing());
    }

    @Test
    public void destroy_clearsListeners() {
        PropertyModel propertyModel = mUploadImagePreviewCoordinator.getPropertyModelForTesting();

        // Verify that the listeners are initially set and not null.
        assertTrue(
                "Save button should have a click listener before destroy.",
                mSaveButton.hasOnClickListeners());
        assertTrue(
                "Cancel button should have a click listener before destroy.",
                mCancelButton.hasOnClickListeners());
        // Use the propertyModel to check if the insets listener is set to null.
        assertNotNull(
                "Insets listener should be set before destroy.",
                propertyModel.get(NtpThemeProperty.PREVIEW_SET_WINDOW_INSETS_LISTENER));

        mUploadImagePreviewCoordinator.destroy();

        assertFalse(
                "Save button's click listener should be null after destroy.",
                mSaveButton.hasOnClickListeners());
        assertFalse(
                "Cancel button's click listener should be null after destroy.",
                mCancelButton.hasOnClickListeners());
        assertNull(
                "Insets listener should be null in the model after destroy.",
                propertyModel.get(NtpThemeProperty.PREVIEW_SET_WINDOW_INSETS_LISTENER));
    }
}
