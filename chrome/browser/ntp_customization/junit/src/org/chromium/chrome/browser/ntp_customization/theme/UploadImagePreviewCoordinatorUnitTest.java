// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Bitmap;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowDialog;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link UploadImagePreviewCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UploadImagePreviewCoordinatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Boolean> mOnClickedCallback;

    private Dialog mDialog;
    private UploadImagePreviewCoordinator mUploadImagePreviewCoordinator;
    private View mSaveButton;
    private View mCancelButton;
    private NtpCustomizationConfigManager mConfigManager;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        mUploadImagePreviewCoordinator =
                new UploadImagePreviewCoordinator(activity, bitmap, mOnClickedCallback);
        mDialog = ShadowDialog.getLatestDialog();
        View contentView = mDialog.findViewById(android.R.id.content);
        mSaveButton = contentView.findViewById(R.id.save_button);
        mCancelButton = contentView.findViewById(R.id.cancel_button);

        mConfigManager = NtpCustomizationConfigManager.getInstance();
        BaseRobolectricTestRule.runAllBackgroundAndUi();
    }

    @After
    public void tearDown() {
        // Clean up preferences to not affect other tests.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    public void testConstructor_showsDialog() {
        assertNotNull("Dialog should have been created and shown.", mDialog);
        assertTrue("Dialog should be showing.", mDialog.isShowing());
    }

    @Test
    public void testClickSaveButton() {
        mSaveButton.performClick();

        // Allow background tasks (like file saving) to complete.
        BaseRobolectricTestRule.runAllBackgroundAndUi();

        assertEquals(
                "Background type should be updated to IMAGE_FROM_DISK.",
                NtpBackgroundImageType.IMAGE_FROM_DISK,
                mConfigManager.getBackgroundImageType());
        assertTrue(
                "The background image file should have been saved.",
                NtpCustomizationUtils.getBackgroundImageFile().exists());
        assertNotNull(
                "The matrices should have been saved.",
                NtpCustomizationUtils.readNtpBackgroundImageMatrices());
        assertNotNull(
                "The portrait matrix should have been saved.",
                NtpCustomizationUtils.readNtpBackgroundImageMatrices().portraitMatrix);
        assertNotNull(
                "The landscape matrix should have been saved.",
                NtpCustomizationUtils.readNtpBackgroundImageMatrices().landscapeMatrix);

        // Verify the on clicked callback was invoked.
        verify(mOnClickedCallback).onResult(eq(true));

        // Verify the dialog was dismissed.
        assertFalse("Dialog should be dismissed after clicking save.", mDialog.isShowing());
    }

    @Test
    public void testClickCancelButton() {
        mCancelButton.performClick();

        // Verify the on clicked callback was invoked.
        verify(mOnClickedCallback).onResult(eq(false));
        assertFalse(
                "The background image file should not have been saved.",
                NtpCustomizationUtils.getBackgroundImageFile().exists());
        assertNull(
                "The matrices should not have been saved.",
                NtpCustomizationUtils.readNtpBackgroundImageMatrices());

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
