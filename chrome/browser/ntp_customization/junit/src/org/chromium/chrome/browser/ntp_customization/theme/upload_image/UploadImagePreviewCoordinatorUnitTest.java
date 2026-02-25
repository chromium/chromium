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
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.composeplate.ComposeplateUtilsJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
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
    @Mock private ComposeplateUtils.Natives mComposeplateUtilsJni;

    private Dialog mDialog;
    private UploadImagePreviewCoordinator mUploadImagePreviewCoordinator;
    private View mSaveButton;
    private View mCancelButton;
    private NtpCustomizationConfigManager mConfigManager;
    private Activity mActivity;
    private Bitmap mBitmap;
    private Matrix mPortraitMatrix;
    private Matrix mLandscapeMatrix;
    private Bitmap mLogoBitmap;
    private PropertyModel mPropertyModel;
    private int mToolbarHeight;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);

        // Mocks the JNI interface for ComposeplateUtils to avoid Native method not present error.
        ComposeplateUtilsJni.setInstanceForTesting(mComposeplateUtilsJni);
        when(mComposeplateUtilsJni.isAimEntrypointEligible(any())).thenReturn(false);

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
        mLogoBitmap = Bitmap.createBitmap(5, 5, Bitmap.Config.ARGB_8888);
        mToolbarHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);

        mConfigManager = NtpCustomizationConfigManager.getInstance();
        ChromeFeatureList.sNewTabPageCustomizationV2ShowLogoAndSearchBox.setForTesting(true);
        RobolectricUtil.runAllBackgroundAndUi();
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
        mConfigManager.setDefaultSearchEngineLogoBitmap(mLogoBitmap);
        setupCropImageView();

        mSaveButton.performClick();

        // Allows background tasks (like file saving) to complete.
        RobolectricUtil.runAllBackgroundAndUi();

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
                NtpBackgroundType.IMAGE_FROM_DISK,
                mConfigManager.getBackgroundType());
        assertTrue(
                "The background image file should have been saved.",
                NtpCustomizationUtils.createBackgroundImageFile().exists());

        // Verifies the on clicked callback was invoked.
        verify(mOnClickedCallback).onResult(eq(true));

        // Verifies the bitmap is still present and was not set to null.
        assertEquals(
                "The search engine logo bitmap should not be cleared when canceling.",
                mLogoBitmap,
                mConfigManager.getDefaultSearchEngineLogoBitmap());

        // Verifies the dialog was dismissed.
        assertFalse("Dialog should be dismissed after clicking save.", mDialog.isShowing());
    }

    @Test
    public void testClickCancelButton() {
        mConfigManager.setDefaultSearchEngineLogoBitmap(mLogoBitmap);
        mCancelButton.performClick();

        // Verifies the on clicked callback was invoked.
        verify(mOnClickedCallback).onResult(eq(false));
        assertFalse(
                "The background image file should not have been saved.",
                NtpCustomizationUtils.createBackgroundImageFile().exists());
        assertNull(
                "The matrices should not have been saved.",
                NtpCustomizationUtils.readNtpBackgroundImageInfo());

        // Verifies the bitmap is still present and was not set to null.
        assertEquals(
                "The search engine logo bitmap should not be cleared when canceling.",
                mLogoBitmap,
                mConfigManager.getDefaultSearchEngineLogoBitmap());

        // Verifies the dialog was dismissed.
        assertFalse("Dialog should be dismissed after clicking cancel.", mDialog.isShowing());
    }

    @Test
    public void testDestroy() {
        mConfigManager.setDefaultSearchEngineLogoBitmap(mLogoBitmap);

        // Verify that the listeners are initially set and not null.
        assertTrue(
                "Save button should have a click listener before destroy.",
                mSaveButton.hasOnClickListeners());
        assertTrue(
                "Cancel button should have a click listener before destroy.",
                mCancelButton.hasOnClickListeners());
        assertTrue(mDialog.isShowing());

        mUploadImagePreviewCoordinator.destroy();

        // Verifies that listeners are cleared from the model.
        assertFalse(
                "Save button's click listener should be null after destroy.",
                mSaveButton.hasOnClickListeners());
        assertFalse(
                "Cancel button's click listener should be null after destroy.",
                mCancelButton.hasOnClickListeners());

        // Verifies the bitmap is still present and was not set to null.
        assertEquals(
                "The search engine logo bitmap should not be cleared when saving an image.",
                mLogoBitmap,
                mConfigManager.getDefaultSearchEngineLogoBitmap());
        assertFalse(mDialog.isShowing());
    }

    @Test
    public void testLogoAndSearchBox_GoogleDefault() {
        // Sets up the configuration where Google is the default search engine, and the logo
        // service returns a null bitmap. This scenario implies that the default Google drawable
        // should be displayed with non-doodle parameters.
        verifySearchBoxWithLogo(/* isDefaultSearchEngineGoogle= */ true, /* logo= */ null);
    }

    @Test
    public void testLogoAndSearchBox_Doodle_Or_ThirdParty_Loading() {
        // Sets up the configuration where a third-party search engine is selected or doodle should
        // show but the logo bitmap is null.
        // This represents a state where the logo is either currently loading or unavailable. This
        // scenario implies that the view should be hidden.
        verifySearchBoxNoLogo(/* doesSearchEngineHaveLogo= */ true);
    }

    @Test
    public void testLogoAndSearchBox_ThirdParty_Loaded() {
        // Sets up the configuration where a third-party search engine is selected and a valid logo
        // bitmap is available. This scenario implies that the view should
        // be visible and display the bitmap, using doodle layout parameters.
        Bitmap logo = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        verifySearchBoxWithLogo(/* isDefaultSearchEngineGoogle= */ false, /* logo= */ logo);
    }

    @Test
    public void testLogoAndSearchBox_Doodle_Loaded() {
        // Sets up the configuration where a doodle should show and a valid logo bitmap is
        // available. This scenario implies that the view should
        // be visible and display the bitmap, using doodle layout parameters.
        Bitmap logo = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        verifySearchBoxWithLogo(/* isDefaultSearchEngineGoogle= */ true, /* logo= */ logo);
    }

    @Test
    public void testLogoAndSearchBox_NoSearchEngineLogo() {
        // Setup the case where the default search engine does not have a logo at all.
        // This scenario implies that the view should be hidden.
        verifySearchBoxNoLogo(/* doesSearchEngineHaveLogo= */ false);
    }

    @Test
    public void testSearchBoxHeight_ComposeplateV2() {
        // Forces the ComposeplateV2 state
        when(mComposeplateUtilsJni.isAimEntrypointEligible(any())).thenReturn(true);
        ChromeFeatureList.sAndroidComposeplateV2Enabled.setForTesting(true);

        mUploadImagePreviewCoordinator =
                new UploadImagePreviewCoordinator(mActivity, mProfile, mBitmap, mOnClickedCallback);

        PropertyModel model = mUploadImagePreviewCoordinator.getPropertyModelForTesting();

        int expectedTallHeight =
                NtpCustomizationUtils.getSearchBoxHeightWithShadows(
                        mActivity.getResources(),
                        /* showSearchBoxTall= */ true,
                        /* hasShadowApplied= */ true);

        // Verifies the height passed to the model
        assertEquals(
                "The height passed to the property model should be the one returned by"
                        + " getSearchBoxHeightWithShadows()",
                expectedTallHeight,
                model.get(NtpThemeProperty.SEARCH_BOX_HEIGHT));

        // Verify the height of the real view
        ConstraintLayout.LayoutParams layoutParams = getSearchBoxLayoutParams();
        assertEquals(
                "The height of the real search box view should be the one returned by"
                        + " getSearchBoxHeightWithShadows()",
                expectedTallHeight,
                layoutParams.height);
    }

    /** Verifies state when the logo is hidden and search box margins are adjusted accordingly. */
    private void verifySearchBoxNoLogo(boolean doesSearchEngineHaveLogo) {
        setupCoordinatorWithLogoAndSearchBoxState(
                doesSearchEngineHaveLogo, /* isGoogle= */ false, /* logo= */ null);

        // 1. Verifies logo visibility and parameters in model
        assertEquals(
                "Logo visibility mismatch",
                View.GONE,
                mPropertyModel.get(NtpThemeProperty.LOGO_VISIBILITY));
        assertNull(
                "Params should not be set when logo is GONE",
                mPropertyModel.get(NtpThemeProperty.LOGO_PARAMS));

        // 2. Verifies the top margin of the real search box view
        ConstraintLayout.LayoutParams layoutParams = getSearchBoxLayoutParams();
        int expectedGoneMargin =
                mActivity.getResources().getDimensionPixelSize(R.dimen.mvt_container_top_margin);
        assertEquals(
                "The real view should use mvt_container_top_margin for goneTopMargin",
                expectedGoneMargin,
                layoutParams.goneTopMargin);
    }

    /** Verifies state when the logo is visible and search box margins are adjusted accordingly. */
    private void verifySearchBoxWithLogo(
            boolean isDefaultSearchEngineGoogle, @Nullable Bitmap logo) {
        setupCoordinatorWithLogoAndSearchBoxState(
                /* hasLogo= */ true, isDefaultSearchEngineGoogle, logo);

        // 1. Verifies logo visibility and bitmap in model
        assertEquals(
                "Logo visibility mismatch",
                View.VISIBLE,
                mPropertyModel.get(NtpThemeProperty.LOGO_VISIBILITY));
        assertEquals(
                "Logo bitmap mismatch", logo, mPropertyModel.get(NtpThemeProperty.LOGO_BITMAP));

        // 2. Verifies calculated logo layout parameters in model
        boolean isLogoDoodle = (logo != null);
        int doodleSize = LogoUtils.getDoodleSize(mActivity.isInMultiWindowMode());
        int[] expectedParams =
                LogoUtils.getLogoViewLayoutParams(
                        mActivity.getResources(), isLogoDoodle, doodleSize);
        assertArrayEquals(
                "Logo params mismatch",
                expectedParams,
                mPropertyModel.get(NtpThemeProperty.LOGO_PARAMS));

        // 3. Verifies the top margin of the real search box view
        ConstraintLayout.LayoutParams layoutParams = getSearchBoxLayoutParams();
        int expectedTopMargin =
                NtpCustomizationUtils.getLogoViewBottomMarginPx(
                        mActivity.getResources(), /* applyShadow= */ true);
        assertEquals(
                "The real view should use logo bottom margin as top margin",
                expectedTopMargin,
                layoutParams.topMargin);
    }

    @Test
    public void testOnApplyWindowInsets_ThreeButtonNavigation_Portrait() {
        // Setup insets representing 3-button navigation (tappable bottom bar)
        verifyWindowInsetsApplied(
                /* topInset= */ 20,
                /* bottomInset= */ 100,
                /* leftInset= */ 0,
                /* rightInset= */ 0,
                /* navigationBars= */ Insets.of(0, 0, 0, 100),
                /* tappableElement= */ Insets.of(0, 20, 0, 100),
                /* expectTappable= */ true);
    }

    @Test
    public void testOnApplyWindowInsets_GestureNavigation_Portrait() {
        // Setup insets representing Gesture navigation
        verifyWindowInsetsApplied(
                /* topInset= */ 20,
                /* bottomInset= */ 40,
                /* leftInset= */ 0,
                /* rightInset= */ 0,
                /* navigationBars= */ Insets.of(0, 0, 0, 40),
                /* tappableElement= */ Insets.of(0, 20, 0, 0),
                /* expectTappable= */ false);
    }

    @Test
    public void testOnApplyWindowInsets_ThreeButtonNavigation_Landscape() {
        // Setup insets representing 3-button navigation in Landscape (Nav bar on the Right)
        verifyWindowInsetsApplied(
                /* topInset= */ 20,
                /* bottomInset= */ 0,
                /* leftInset= */ 0,
                /* rightInset= */ 100,
                /* navigationBars= */ Insets.of(0, 0, 100, 0),
                /* tappableElement= */ Insets.of(0, 20, 100, 0),
                /* expectTappable= */ true);
    }

    @Test
    public void testOnApplyWindowInsets_GestureNavigation_Landscape() {
        // Setup insets representing Gesture navigation in Landscape
        verifyWindowInsetsApplied(
                /* topInset= */ 20,
                /* bottomInset= */ 20,
                /* leftInset= */ 0,
                /* rightInset= */ 0,
                /* navigationBars= */ Insets.of(0, 0, 0, 20),
                /* tappableElement= */ Insets.of(0, 20, 0, 0),
                /* expectTappable= */ false);
    }

    /** Helper method that centralizes the Arrange/Act/Assert for window insets testing. */
    private void verifyWindowInsetsApplied(
            int topInset,
            int bottomInset,
            int leftInset,
            int rightInset,
            Insets navigationBars,
            Insets tappableElement,
            boolean expectTappable) {

        Insets systemBars = Insets.of(leftInset, topInset, rightInset, bottomInset);
        WindowInsetsCompat insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.systemBars(), systemBars)
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), navigationBars)
                        .setInsets(WindowInsetsCompat.Type.tappableElement(), tappableElement)
                        .build();

        PropertyModel model = mUploadImagePreviewCoordinator.getPropertyModelForTesting();
        int buttonBottomMargin = model.get(NtpThemeProperty.BUTTON_BOTTOM_MARGIN);

        WindowInsetsCompat result =
                mUploadImagePreviewCoordinator.onApplyWindowInsets(mCropImageView, insets);

        // Verifies the edge-to-edge behavior
        assertEquals(
                "Tappable navigation bar evaluation failed.",
                expectTappable,
                EdgeToEdgeUtils.hasTappableNavigationBarFromInsets(insets));

        // Verifies inset consumption
        assertEquals(Insets.NONE, result.getInsets(WindowInsetsCompat.Type.statusBars()));
        assertEquals(Insets.NONE, result.getInsets(WindowInsetsCompat.Type.navigationBars()));
        assertEquals(Insets.NONE, result.getInsets(WindowInsetsCompat.Type.displayCutout()));

        // Verifies the paddings
        int expectedTopGuideline = mToolbarHeight + topInset;
        int expectedBottomPadding = expectTappable ? bottomInset : 0;
        Rect expectedSideAndBottom = new Rect(leftInset, 0, rightInset, expectedBottomPadding);

        assertEquals(
                "Top guideline should include toolbar height + top inset",
                expectedTopGuideline,
                model.get(NtpThemeProperty.TOP_GUIDELINE_BEGIN));

        assertEquals(
                "Padding rect should match expected side and bottom logic",
                expectedSideAndBottom,
                model.get(NtpThemeProperty.SIDE_AND_BOTTOM_INSETS));

        // Verifies the bottom margin of buttons
        if (expectTappable) {
            assertEquals(
                    "Button margin should remain unchanged for 3-button nav, as padding pushes it"
                        + " up.",
                    buttonBottomMargin,
                    model.get(NtpThemeProperty.BUTTON_BOTTOM_MARGIN));
        } else {
            int baseButtonBottomMargin =
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(
                                    R.dimen.ntp_customization_back_button_margin_start);
            assertEquals(
                    "Button margin should be elevated by the inset for gesture nav.",
                    baseButtonBottomMargin + bottomInset,
                    model.get(NtpThemeProperty.BUTTON_BOTTOM_MARGIN));
        }
    }

    /**
     * Helper to set up mocks, re-instantiates the coordinator, and verifies common model properties
     * shared by both visible and hidden logo states.
     */
    private void setupCoordinatorWithLogoAndSearchBoxState(
            boolean hasLogo, boolean isGoogle, @Nullable Bitmap logo) {
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(hasLogo);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(isGoogle);
        mConfigManager.setDefaultSearchEngineLogoBitmap(logo);

        // Re-create coordinator to run constructor logic
        mUploadImagePreviewCoordinator =
                new UploadImagePreviewCoordinator(mActivity, mProfile, mBitmap, mOnClickedCallback);

        mPropertyModel = mUploadImagePreviewCoordinator.getPropertyModelForTesting();

        // Verifies the value of SEARCH_BOX_TOP_MARGIN which is shared
        // by both visible and hidden logo states.
        int expectedModelMargin =
                NtpCustomizationUtils.getLogoViewBottomMarginPx(
                        mActivity.getResources(), /* applyShadow= */ true);
        assertEquals(
                "The model should hold the shadow-adjusted margin",
                expectedModelMargin,
                mPropertyModel.get(NtpThemeProperty.SEARCH_BOX_TOP_MARGIN));
    }

    /** Helper to find the search box container view and return its layout params. */
    private ConstraintLayout.LayoutParams getSearchBoxLayoutParams() {
        View searchBoxContainer =
                ShadowDialog.getLatestDialog().findViewById(R.id.search_box_container);
        return (ConstraintLayout.LayoutParams) searchBoxContainer.getLayoutParams();
    }
}
