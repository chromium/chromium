// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.Dialog;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Point;
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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link UploadImagePreviewCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UploadImagePreviewCoordinatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Boolean> mOnClickedCallback;
    @Mock private CropImageView mCropImageView;

    private Dialog mDialog;
    private UploadImagePreviewCoordinator mUploadImagePreviewCoordinator;
    private View mSaveButton;
    private View mCancelButton;
    private NtpCustomizationConfigManager mConfigManager;
    private Activity mActivity;
    private Bitmap mBitmap;
    private Matrix mPortraitMatrix;
    private Matrix mLandscapeMatrix;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        mUploadImagePreviewCoordinator =
                new UploadImagePreviewCoordinator(mActivity, mBitmap, mOnClickedCallback);
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
    public void testMetricThemeUploadImagePreviewShow() {
        String histogramName = "NewTabPage.Customization.Theme.UploadImage.PreviewShow";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        mUploadImagePreviewCoordinator =
                new UploadImagePreviewCoordinator(mActivity, mBitmap, mOnClickedCallback);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testMetricThemeUploadImagePreviewInteractions_cancel() {
        String histogramName = "NewTabPage.Customization.Theme.UploadImage.PreviewInteractions";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, UploadImagePreviewCoordinator.PreviewInteractionType.CANCEL);
        mCancelButton.performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testMetricThemeUploadImagePreviewInteractions_save() {
        String histogramName = "NewTabPage.Customization.Theme.UploadImage.PreviewInteractions";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, UploadImagePreviewCoordinator.PreviewInteractionType.SAVE);
        mSaveButton.performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testMetricThemeUploadImagePreviewInteractions_pinchToResize_cancel() {
        setupCropImageView_pinchToResize();
        String histogramName = "NewTabPage.Customization.Theme.UploadImage.PreviewInteractions";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                histogramName,
                                UploadImagePreviewCoordinator.PreviewInteractionType.CANCEL)
                        .expectIntRecord(
                                histogramName,
                                UploadImagePreviewCoordinator.PreviewInteractionType
                                        .PINCH_TO_RESIZE)
                        .build();
        mCancelButton.performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testMetricThemeUploadImagePreviewInteractions_pinchToResize_save() {
        setupCropImageView_pinchToResize();
        String histogramName = "NewTabPage.Customization.Theme.UploadImage.PreviewInteractions";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                histogramName,
                                UploadImagePreviewCoordinator.PreviewInteractionType.SAVE)
                        .expectIntRecord(
                                histogramName,
                                UploadImagePreviewCoordinator.PreviewInteractionType
                                        .PINCH_TO_RESIZE)
                        .build();
        mSaveButton.performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testMetricThemeUploadImagePreviewInteractions_rotateScreen_cancel() {
        setupCropImageView_rotateScreen();
        String histogramName = "NewTabPage.Customization.Theme.UploadImage.PreviewInteractions";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                histogramName,
                                UploadImagePreviewCoordinator.PreviewInteractionType.CANCEL)
                        .expectIntRecord(
                                histogramName,
                                UploadImagePreviewCoordinator.PreviewInteractionType.ROTATE_SCREEN)
                        .build();
        mCancelButton.performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testMetricThemeUploadImagePreviewInteractions_rotateScreen_save() {
        setupCropImageView_rotateScreen();
        String histogramName = "NewTabPage.Customization.Theme.UploadImage.PreviewInteractions";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                histogramName,
                                UploadImagePreviewCoordinator.PreviewInteractionType.SAVE)
                        .expectIntRecord(
                                histogramName,
                                UploadImagePreviewCoordinator.PreviewInteractionType.ROTATE_SCREEN)
                        .build();
        mSaveButton.performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void
            testMetricThemeUploadImagePreviewInteractions_rotateScreenAndPinchToResize_cancel() {
        setupCropImageView_rotateScreenAndPinchToResize();
        String histogramName = "NewTabPage.Customization.Theme.UploadImage.PreviewInteractions";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                histogramName,
                                UploadImagePreviewCoordinator.PreviewInteractionType.CANCEL)
                        .expectIntRecord(
                                histogramName,
                                UploadImagePreviewCoordinator.PreviewInteractionType
                                        .ROTATE_SCREEN_AND_PINCH_TO_RESIZE)
                        .build();
        mCancelButton.performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testMetricThemeUploadImagePreviewInteractions_rotateScreenAndPinchToResize_save() {
        setupCropImageView_rotateScreenAndPinchToResize();
        String histogramName = "NewTabPage.Customization.Theme.UploadImage.PreviewInteractions";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                histogramName,
                                UploadImagePreviewCoordinator.PreviewInteractionType.SAVE)
                        .expectIntRecord(
                                histogramName,
                                UploadImagePreviewCoordinator.PreviewInteractionType
                                        .ROTATE_SCREEN_AND_PINCH_TO_RESIZE)
                        .build();
        mSaveButton.performClick();
        histogramWatcher.assertExpected();
    }

    private void setupCropImageView() {
        mUploadImagePreviewCoordinator.setCropImageViewForTesting(mCropImageView);
        mPortraitMatrix = new Matrix();
        mPortraitMatrix.setTranslate(100f, 200f); // Translate X=100, Y=200

        mLandscapeMatrix = new Matrix();
        mLandscapeMatrix.setScale(2.5f, 3.5f);

        when(mCropImageView.getPortraitMatrix()).thenReturn(mPortraitMatrix);
        when(mCropImageView.getLandscapeMatrix()).thenReturn(mLandscapeMatrix);

        when(mCropImageView.getPortraitWindowSize()).thenReturn(new Point(1080, 1920));
        when(mCropImageView.getLandscapeWindowSize()).thenReturn(new Point(2000, 1080));
    }

    private void setupCropImageView_pinchToResize() {
        setupCropImageView();
        when(mCropImageView.getIsScaled()).thenReturn(true);
        when(mCropImageView.getIsScrolled()).thenReturn(true);
    }

    private void setupCropImageView_rotateScreen() {
        setupCropImageView();
        when(mCropImageView.getIsScreenRotated()).thenReturn(true);
    }

    private void setupCropImageView_rotateScreenAndPinchToResize() {
        setupCropImageView_pinchToResize();
        when(mCropImageView.getIsScreenRotated()).thenReturn(true);
    }

    @Test
    public void testClickSaveButton() {
        setupCropImageView();

        mSaveButton.performClick();

        // Allows background tasks (like file saving) to complete.
        BaseRobolectricTestRule.runAllBackgroundAndUi();

        BackgroundImageInfo savedInfo = NtpCustomizationUtils.readNtpBackgroundImageInfo();

        // Verifies the portrait image information
        Point portraitSize = savedInfo.getWindowSize(Configuration.ORIENTATION_PORTRAIT);
        assertEquals("Portrait width should match", 1080, portraitSize.x);
        assertEquals("Portrait height should match", 1920, portraitSize.y);
        assertEquals(
                "Portrait matrix should be saved", mPortraitMatrix, savedInfo.getPortraitMatrix());

        // Verifies the landscape image information
        Point landscapeSize = savedInfo.getWindowSize(Configuration.ORIENTATION_LANDSCAPE);
        assertEquals("Landscape width should match", 2000, landscapeSize.x);
        assertEquals("Landscape height should match", 1080, landscapeSize.y);
        assertEquals(
                "Landscape matrix should be saved.",
                mLandscapeMatrix,
                savedInfo.getLandscapeMatrix());

        assertEquals(
                "Background type should be updated to IMAGE_FROM_DISK.",
                NtpBackgroundImageType.IMAGE_FROM_DISK,
                mConfigManager.getBackgroundImageType());
        assertTrue(
                "The background image file should have been saved.",
                NtpCustomizationUtils.getBackgroundImageFile().exists());

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
                NtpCustomizationUtils.readNtpBackgroundImageInfo());

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
