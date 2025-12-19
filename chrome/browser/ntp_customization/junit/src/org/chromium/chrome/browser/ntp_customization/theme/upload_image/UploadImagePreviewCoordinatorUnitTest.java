// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.junit.Assert.assertArrayEquals;
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
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Point;
import android.view.View;

import androidx.annotation.Nullable;

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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link UploadImagePreviewCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UploadImagePreviewCoordinatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Boolean> mOnClickedCallback;
    @Mock private CropImageView mCropImageView;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private Profile mProfile;

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

        // Default to show Google logo in tests.
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);

        mUploadImagePreviewCoordinator =
                new UploadImagePreviewCoordinator(mActivity, mProfile, mBitmap, mOnClickedCallback);
        mDialog = ShadowDialog.getLatestDialog();
        View contentView = mDialog.findViewById(android.R.id.content);
        mSaveButton = contentView.findViewById(R.id.save_button);
        mCancelButton = contentView.findViewById(R.id.cancel_button);

        mConfigManager = NtpCustomizationConfigManager.getInstance();
        ChromeFeatureList.sNewTabPageCustomizationV2ShowLogoAndSearchBox.setForTesting(true);
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
                new UploadImagePreviewCoordinator(mActivity, mProfile, mBitmap, mOnClickedCallback);

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
        Point portraitSize = savedInfo.getPortraitWindowSize();
        assertEquals("Portrait width should match", 1080, portraitSize.x);
        assertEquals("Portrait height should match", 1920, portraitSize.y);
        assertEquals(
                "Portrait matrix should be saved", mPortraitMatrix, savedInfo.getPortraitMatrix());

        // Verifies the landscape image information
        Point landscapeSize = savedInfo.getLandscapeWindowSize();
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
                NtpCustomizationUtils.createBackgroundImageFile().exists());

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
                NtpCustomizationUtils.createBackgroundImageFile().exists());
        assertNull(
                "The matrices should not have been saved.",
                NtpCustomizationUtils.readNtpBackgroundImageInfo());

        // Verify the dialog was dismissed.
        assertFalse("Dialog should be dismissed after clicking cancel.", mDialog.isShowing());
    }

    @Test
    public void testDestroy() {
        PropertyModel propertyModel = mUploadImagePreviewCoordinator.getPropertyModelForTesting();

        // Verify that the listeners are initially set and not null.
        assertTrue(
                "Save button should have a click listener before destroy.",
                mSaveButton.hasOnClickListeners());
        assertTrue(
                "Cancel button should have a click listener before destroy.",
                mCancelButton.hasOnClickListeners());

        mUploadImagePreviewCoordinator.destroy();

        // Verifies that listeners are cleared from the model.
        assertFalse(
                "Save button's click listener should be null after destroy.",
                mSaveButton.hasOnClickListeners());
        assertFalse(
                "Cancel button's click listener should be null after destroy.",
                mCancelButton.hasOnClickListeners());

        // Verifies that the logo bitmap is set to null in the NtpCustomizationConfigManager.
        assertNull(NtpCustomizationConfigManager.getInstance().getDefaultSearchEngineLogoBitmap());
    }

    @Test
    public void testLogoLogic_GoogleDefault() {
        // Sets up the configuration where Google is the default search engine, and the logo
        // service returns a null bitmap. This scenario implies that the default Google drawable
        // should be displayed with non-doodle parameters.
        verifyLogoVisible(/* isDefaultSearchEngineGoogle= */ true, /* logo= */ null);
    }

    @Test
    public void testLogoLogic_Doodle_Or_ThirdParty_Loading() {
        // Sets up the configuration where a third-party search engine is selected or doodle should
        // show but the logo bitmap is null.
        // This represents a state where the logo is either currently loading or unavailable. This
        // scenario implies that the view should be hidden.
        verifyLogoGone(/* doesSearchEngineHaveLogo= */ true);
    }

    @Test
    public void testLogoLogic_ThirdParty_Loaded() {
        // Sets up the configuration where a third-party search engine is selected and a valid logo
        // bitmap is available. This scenario implies that the view should
        // be visible and display the bitmap, using doodle layout parameters.
        Bitmap logo = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        verifyLogoVisible(/* isDefaultSearchEngineGoogle= */ false, /* logo= */ logo);
    }

    @Test
    public void testLogoLogic_Doodle_Loaded() {
        // Sets up the configuration where a doodle should show and a valid logo bitmap is
        // available. This scenario implies that the view should
        // be visible and display the bitmap, using doodle layout parameters.
        Bitmap logo = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        verifyLogoVisible(/* isDefaultSearchEngineGoogle= */ true, /* logo= */ logo);
    }

    @Test
    public void testLogoLogic_SearchEngineHasNoLogo() {
        // Setup the case where the default search engine does not have a logo at all.
        // This scenario implies that the view should be hidden.
        verifyLogoGone(/* doesSearchEngineHaveLogo= */ false);
    }

    /** Helper to verify logo is hidden. */
    private void verifyLogoGone(boolean doesSearchEngineHaveLogo) {
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo())
                .thenReturn(doesSearchEngineHaveLogo);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        mConfigManager.setDefaultSearchEngineLogoBitmap(null);

        // Re-create coordinator to run constructor logic
        mUploadImagePreviewCoordinator =
                new UploadImagePreviewCoordinator(mActivity, mProfile, mBitmap, mOnClickedCallback);

        PropertyModel model = mUploadImagePreviewCoordinator.getPropertyModelForTesting();

        assertEquals(
                "Logo visibility mismatch", View.GONE, model.get(NtpThemeProperty.LOGO_VISIBILITY));

        assertNull(
                "Params should not be set when logo is GONE",
                model.get(NtpThemeProperty.LOGO_PARAMS));
    }

    /** Helper to verify logo is visible with correct bitmap and calculated params. */
    private void verifyLogoVisible(boolean isDefaultSearchEngineGoogle, @Nullable Bitmap logo) {

        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle())
                .thenReturn(isDefaultSearchEngineGoogle);
        mConfigManager.setDefaultSearchEngineLogoBitmap(logo);

        // Re-create coordinator to run constructor logic
        mUploadImagePreviewCoordinator =
                new UploadImagePreviewCoordinator(mActivity, mProfile, mBitmap, mOnClickedCallback);

        PropertyModel model = mUploadImagePreviewCoordinator.getPropertyModelForTesting();

        assertEquals(
                "Logo visibility mismatch",
                View.VISIBLE,
                model.get(NtpThemeProperty.LOGO_VISIBILITY));

        assertEquals("Logo bitmap mismatch", logo, model.get(NtpThemeProperty.LOGO_BITMAP));

        // Verifies layout parameters
        boolean isLogoDoodle = (logo != null);
        int doodleSize = LogoUtils.getDoodleSize(mActivity.isInMultiWindowMode());
        int[] expectedParams =
                LogoUtils.getLogoViewLayoutParams(
                        mActivity.getResources(), isLogoDoodle, doodleSize);

        assertArrayEquals(
                "Logo params mismatch", expectedParams, model.get(NtpThemeProperty.LOGO_PARAMS));
    }
}
