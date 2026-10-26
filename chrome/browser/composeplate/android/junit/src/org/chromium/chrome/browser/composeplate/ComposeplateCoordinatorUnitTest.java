// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.ntp.NewTabPageUtils.ActionChips;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.components.search_engines.AiModeButtonUiConfig;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ComposeplateCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ComposeplateCoordinatorUnitTest {
    // A phone in full screen portrait mode.
    private static final String PORTRAIT_QUALIFIER = "+w411dp-h731dp-port";
    // A phone in full screen landscape mode.
    private static final String LANDSCAPE_QUALIFIER = "+w731dp-h411dp-land";
    // A phone in top/bottom split-screen mode: the window is in landscape, but as narrow as in
    // portrait mode.
    private static final String SPLIT_SCREEN_LANDSCAPE_QUALIFIER = "+w411dp-h360dp-land";
    // A phone in left/right split-screen mode resized to a very narrow window.
    private static final String NARROW_SPLIT_SCREEN_QUALIFIER = "+w300dp-h411dp-port";
    // A tablet in a window wide enough for the large form factor layout.
    private static final String TABLET_QUALIFIER = "sw600dp-w600dp";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnClickListener mOriginalOnClickListener;

    private Context mContext;
    private ComposeplateView mComposeplateView;
    private View mIncognitoButton;
    private View mComposeplateButton;
    private View mOptionalButton;
    private ComposeplateCoordinator mCoordinator;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        IncognitoUtils.setEnabledForTesting(true);

        FrameLayout parentView = new FrameLayout(mContext);
        mComposeplateView =
                (ComposeplateView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.composeplate_view_layout, parentView, false);
        parentView.addView(mComposeplateView);
        mIncognitoButton = mComposeplateView.findViewById(R.id.incognito_button);
        mComposeplateButton = mComposeplateView.findViewById(R.id.composeplate_button);
        mOptionalButton = mComposeplateView.findViewById(R.id.optional_button);

        mCoordinator =
                new ComposeplateCoordinator(
                        parentView,
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext),
                        /* horizontalInsetDp= */ 0);
        mPropertyModel = mCoordinator.getModelForTesting();
    }

    @Test
    public void testSetVisibility() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ComposeplateMetricsUtils.HISTOGRAM_COMPOSEPLATE_IMPRESSION, true);
        mCoordinator.setVisibility(/* visible= */ true, /* isCurrentPage= */ true);
        assertEquals(View.VISIBLE, mComposeplateView.getVisibility());
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ComposeplateMetricsUtils.HISTOGRAM_COMPOSEPLATE_IMPRESSION, false);
        mCoordinator.setVisibility(/* visible= */ false, /* isCurrentPage= */ true);
        assertEquals(View.GONE, mComposeplateView.getVisibility());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordButtonsImpression_optionalButtonHidden() {
        testRecordButtonsImpressionImpl(
                ActionChips.CREATE_IMAGE, /* isOptionalButtonVisible= */ false);
    }

    @Test
    public void testRecordButtonsImpression_createImageButtonVisible() {
        testRecordButtonsImpressionImpl(
                ActionChips.CREATE_IMAGE, /* isOptionalButtonVisible= */ true);
    }

    @Test
    public void testRecordButtonsImpression_galleryButtonVisible() {
        testRecordButtonsImpressionImpl(ActionChips.GALLERY, /* isOptionalButtonVisible= */ true);
    }

    @Test
    public void testRecordButtonsImpression_optionalButtonTypeNotSet() {
        mCoordinator.setVisibility(/* visible= */ true, /* isCurrentPage= */ false);
        mCoordinator.setOptionalButtonVisibility(/* visible= */ true);

        String histogramName = ComposeplateMetricsUtils.HISTOGRAM_COMPOSEPLATE_BUTTONS_IMPRESSION;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(histogramName, ActionChips.DEFAULT, ActionChips.INCOGNITO)
                        .build();

        mCoordinator.recordButtonsImpression();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordButtonsImpression_composeplateHidden() {
        mCoordinator.setOptionalButtonClickListener(
                mOriginalOnClickListener, ActionChips.CREATE_IMAGE);
        mCoordinator.setOptionalButtonVisibility(/* visible= */ true);
        mCoordinator.setVisibility(/* visible= */ false, /* isCurrentPage= */ false);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                ComposeplateMetricsUtils.HISTOGRAM_COMPOSEPLATE_BUTTONS_IMPRESSION)
                        .build();

        mCoordinator.recordButtonsImpression();

        histogramWatcher.assertExpected();
    }

    private void testRecordButtonsImpressionImpl(
            @ActionChips int optionalButtonType, boolean isOptionalButtonVisible) {
        mCoordinator.setVisibility(/* visible= */ true, /* isCurrentPage= */ false);
        mCoordinator.setOptionalButtonClickListener(mOriginalOnClickListener, optionalButtonType);
        mCoordinator.setOptionalButtonVisibility(isOptionalButtonVisible);

        String histogramName = ComposeplateMetricsUtils.HISTOGRAM_COMPOSEPLATE_BUTTONS_IMPRESSION;
        HistogramWatcher.Builder builder =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                histogramName, ActionChips.DEFAULT, ActionChips.INCOGNITO);
        if (isOptionalButtonVisible) {
            builder.expectIntRecord(histogramName, optionalButtonType);
        }
        HistogramWatcher histogramWatcher = builder.build();

        mCoordinator.recordButtonsImpression();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testSetIncognitoClickListener() {
        mCoordinator.setIncognitoClickListener(mOriginalOnClickListener);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Module.Click",
                        ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_INCOGNITO_BUTTON);

        mIncognitoButton.performClick();

        histogramWatcher.assertExpected();
        verify(mOriginalOnClickListener).onClick(mIncognitoButton);
    }

    @Test
    public void testComposeplateButtonClickListener() {
        mCoordinator.setComposeplateButtonClickListener(mOriginalOnClickListener);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Module.Click", ModuleTypeOnStartAndNtp.COMPOSEPLATE_BUTTON);

        mComposeplateButton.performClick();

        histogramWatcher.assertExpected();
        verify(mOriginalOnClickListener).onClick(mComposeplateButton);
    }

    @Test
    public void testOptionalButtonClickListener_CreateImage() {
        testOptionalButtonClickListenerImpl(
                ActionChips.CREATE_IMAGE,
                ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_CREATE_IMAGE_BUTTON);
    }

    @Test
    public void testOptionalButtonClickListener_Canvas() {
        testOptionalButtonClickListenerImpl(
                ActionChips.GALLERY, ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_GALLERY_BUTTON);
    }

    private void testOptionalButtonClickListenerImpl(
            @ActionChips int actionChipsType, @ModuleTypeOnStartAndNtp int expectedModuleType) {
        mCoordinator.setOptionalButtonClickListener(mOriginalOnClickListener, actionChipsType);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Module.Click", expectedModuleType);

        mOptionalButton.performClick();

        histogramWatcher.assertExpected();
        verify(mOriginalOnClickListener).onClick(mOptionalButton);
    }

    @Test
    public void testDestroy() {
        mCoordinator.setIncognitoClickListener(mOriginalOnClickListener);
        mCoordinator.setComposeplateButtonClickListener(mOriginalOnClickListener);

        assertNotNull(mPropertyModel.get(ComposeplateProperties.INCOGNITO_CLICK_LISTENER));
        assertNotNull(
                mPropertyModel.get(ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER));

        mCoordinator.destroy();
        assertNull(mPropertyModel.get(ComposeplateProperties.INCOGNITO_CLICK_LISTENER));
        assertNull(mPropertyModel.get(ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER));
    }

    @Test
    public void testApplyWhiteBackgroundWithShadow() {
        // Tests the case to apply a white background with shadow.
        boolean apply = true;
        int textStyleResId = ComposeplateUtils.getSearchBoxTextStyleResId(apply);
        ColorStateList colorStateList =
                ComposeplateUtils.getSearchBoxIconColorTint(mContext, apply);
        mCoordinator.applyWhiteBackground(true);
        assertTrue(mPropertyModel.get(ComposeplateProperties.APPLY_WHITE_BACKGROUND));
        assertEquals(colorStateList, mPropertyModel.get(ComposeplateProperties.COLOR_STATE_LIST));
        assertEquals(textStyleResId, mPropertyModel.get(ComposeplateProperties.TEXT_STYLE_RES_ID));

        // Tests the case to remove the white background with shadow.
        apply = false;
        textStyleResId = ComposeplateUtils.getSearchBoxTextStyleResId(apply);
        colorStateList = ComposeplateUtils.getSearchBoxIconColorTint(mContext, apply);
        mCoordinator.applyWhiteBackground(false);
        assertFalse(mPropertyModel.get(ComposeplateProperties.APPLY_WHITE_BACKGROUND));
        assertEquals(colorStateList, mPropertyModel.get(ComposeplateProperties.COLOR_STATE_LIST));
        assertEquals(textStyleResId, mPropertyModel.get(ComposeplateProperties.TEXT_STYLE_RES_ID));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NTP_AURORA + ":change_button_color/true")
    public void testSetLayoutWidth_buttonColorEnabled_phone() {
        verifyComposeplateWidth(
                /* lateralMargin= */ mContext.getResources()
                        .getDimensionPixelSize(R.dimen.composeplate_view_lateral_margin));
    }

    @Test
    @Config(qualifiers = TABLET_QUALIFIER)
    @EnableFeatures(ChromeFeatureList.NTP_AURORA + ":change_button_color/true")
    public void testSetLayoutWidth_buttonColorEnabled_tablet() {
        verifyComposeplateWidth(
                /* lateralMargin= */ mContext.getResources()
                        .getDimensionPixelSize(R.dimen.composeplate_view_lateral_margin));
    }

    @Test
    public void testUpdateAiModeButtonUiConfig() {
        assertNull(mPropertyModel.get(ComposeplateProperties.AI_MODE_BUTTON_UI_CONFIG));

        AiModeButtonUiConfig aiModeButtonUiConfig =
                new AiModeButtonUiConfig(
                        "AI Mode",
                        "Ask AI Mode",
                        "AI Mode button",
                        "Always show AI Mode",
                        "Ask AI Mode",
                        /* faviconUrl= */ JUnitTestGURLs.RED_1,
                        /* navigationUrl= */ "https://www.red.com/search?q={searchTerms}",
                        /* navigationUrlEmpty= */ JUnitTestGURLs.URL_2);
        mCoordinator.updateAiModeButtonUiConfig(aiModeButtonUiConfig);

        assertEquals(
                aiModeButtonUiConfig,
                mPropertyModel.get(ComposeplateProperties.AI_MODE_BUTTON_UI_CONFIG));
        TextView composeplateButtonText =
                mComposeplateView.findViewById(R.id.composeplate_button_text);
        assertEquals(aiModeButtonUiConfig.text, composeplateButtonText.getText().toString());
    }

    @Test
    public void testUpdateAiModeButtonIcon() {
        assertNull(mPropertyModel.get(ComposeplateProperties.AI_MODE_BUTTON_ICON));

        Drawable drawable = new ColorDrawable(Color.RED);
        mCoordinator.updateAiModeButtonIcon(drawable, /* shouldTint= */ false);

        ComposeplateProperties.AiModeButtonIcon icon =
                mPropertyModel.get(ComposeplateProperties.AI_MODE_BUTTON_ICON);
        assertEquals(drawable, icon.drawable);
        assertFalse(icon.shouldTint);
        ImageView iconView = mComposeplateView.findViewById(R.id.composeplate_button_icon);
        assertEquals(drawable, iconView.getDrawable());
    }

    @Test
    @Config(qualifiers = PORTRAIT_QUALIFIER)
    public void testSetOptionalButtonVisibility_visible() {
        testSetOptionalButtonVisibilityImpl(/* visible= */ true);
    }

    @Test
    @Config(qualifiers = PORTRAIT_QUALIFIER)
    public void testSetOptionalButtonVisibility_hidden() {
        testSetOptionalButtonVisibilityImpl(/* visible= */ false);
    }

    @Test
    @Config(qualifiers = TABLET_QUALIFIER)
    public void testSetOptionalButtonVisibility_visible_tablet() {
        testSetOptionalButtonVisibilityImpl(/* visible= */ true);
    }

    @Test
    @Config(qualifiers = TABLET_QUALIFIER)
    public void testSetOptionalButtonVisibility_hidden_tablet() {
        testSetOptionalButtonVisibilityImpl(/* visible= */ false);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NTP_AURORA,
        ChromeFeatureList.NTP_AURORA_V2 + ":layout_type/1"
    })
    public void testSetOptionalButtonVisibility_besideMvtEnabled_skipsButtonSpacingUpdate() {
        mCoordinator.setOptionalButtonVisibility(/* visible= */ true);

        assertTrue(mPropertyModel.get(ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE));
        assertEquals(0, mPropertyModel.get(ComposeplateProperties.OPTIONAL_BUTTON_LATERAL_PADDING));
        assertEquals(0, mPropertyModel.get(ComposeplateProperties.OPTIONAL_BUTTON_MARGIN_END));
    }

    @Test
    public void testSetOptionalButtonText() {
        String text = "Create";
        CharSequence defaultContentDescription = mOptionalButton.getContentDescription();
        mCoordinator.setOptionalButtonText(text);

        assertEquals(text, mPropertyModel.get(ComposeplateProperties.OPTIONAL_BUTTON_TEXT));
        TextView optionalButtonText = mComposeplateView.findViewById(R.id.optional_button_text);
        assertEquals(text, optionalButtonText.getText().toString());
        // The text doesn't affect the content description.
        assertEquals(defaultContentDescription, mOptionalButton.getContentDescription());
    }

    @Test
    public void testSetOptionalButtonContentDescription() {
        String contentDescription = "Create image";
        mCoordinator.setOptionalButtonContentDescription(contentDescription);

        assertEquals(
                contentDescription,
                mPropertyModel.get(ComposeplateProperties.OPTIONAL_BUTTON_CONTENT_DESCRIPTION));
        assertEquals(contentDescription, mOptionalButton.getContentDescription());
        assertEquals(contentDescription, mOptionalButton.getTooltipText());
    }

    @Test
    public void testOnDisplayStyleChanged_optionalButtonVisible() {
        mCoordinator.setOptionalButtonVisibility(/* visible= */ true);
        int portraitPadding = testOnDisplayStyleChangedImpl(PORTRAIT_QUALIFIER);

        int landscapePadding = testOnDisplayStyleChangedImpl(LANDSCAPE_QUALIFIER);
        assertNotEquals(portraitPadding, landscapePadding);
    }

    @Test
    public void testOnDisplayStyleChanged_optionalButtonHidden() {
        mCoordinator.setOptionalButtonVisibility(/* visible= */ false);
        int paddingStart = mOptionalButton.getPaddingStart();
        int marginEnd = getMarginEnd(mOptionalButton);

        RuntimeEnvironment.setQualifiers(LANDSCAPE_QUALIFIER);
        mCoordinator.onDisplayStyleChanged();

        int defaultSpacing =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.composeplate_view_button_margin);
        verifyButtonSpacing(defaultSpacing, defaultSpacing);
        assertEquals(paddingStart, mOptionalButton.getPaddingStart());
        assertEquals(marginEnd, getMarginEnd(mOptionalButton));
    }

    @Test
    public void testOnDisplayStyleChanged_fromLandscapeToSplitScreen_usesPortraitSpacing() {
        mCoordinator.setOptionalButtonVisibility(/* visible= */ true);
        int portraitPadding = testOnDisplayStyleChangedImpl(PORTRAIT_QUALIFIER);

        int landscapePadding = testOnDisplayStyleChangedImpl(LANDSCAPE_QUALIFIER);
        assertNotEquals(portraitPadding, landscapePadding);

        int splitScreenPadding = testOnDisplayStyleChangedImpl(SPLIT_SCREEN_LANDSCAPE_QUALIFIER);
        assertEquals(portraitPadding, splitScreenPadding);
    }

    @Test
    public void testOnDisplayStyleChanged_toNarrowSplitScreen_reducesPadding() {
        mCoordinator.setOptionalButtonVisibility(/* visible= */ true);
        int portraitPadding = testOnDisplayStyleChangedImpl(PORTRAIT_QUALIFIER);

        int narrowPadding = testOnDisplayStyleChangedImpl(NARROW_SPLIT_SCREEN_QUALIFIER);
        assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.composeplate_view_button_margin),
                narrowPadding);
        assertTrue(narrowPadding < portraitPadding);

        // The regular padding is restored once the window is wide enough again.
        assertEquals(portraitPadding, testOnDisplayStyleChangedImpl(PORTRAIT_QUALIFIER));
    }

    /**
     * Applies the given qualifiers, notifies the coordinator of the display style change, and
     * verifies the optional button's spacing matches the dimensions of the new configuration.
     *
     * @param qualifiers The Robolectric qualifiers to apply.
     * @return The optional button's lateral padding in the new configuration.
     */
    private int testOnDisplayStyleChangedImpl(String qualifiers) {
        RuntimeEnvironment.setQualifiers(qualifiers);
        mCoordinator.onDisplayStyleChanged();

        Resources res = mContext.getResources();
        int padding = res.getDimensionPixelSize(R.dimen.composeplate_view_optional_button_padding);
        int marginEnd = res.getDimensionPixelSize(R.dimen.composeplate_view_optional_button_margin);
        verifyButtonSpacing(padding, marginEnd);
        assertEquals(padding, mOptionalButton.getPaddingStart());
        assertEquals(marginEnd, getMarginEnd(mOptionalButton));
        return padding;
    }

    @Test
    @Config(qualifiers = TABLET_QUALIFIER)
    public void testUpdateIsInNarrowWindow_tablet_optionalButtonVisible() {
        testUpdateIsInNarrowWindowOnTabletImpl(/* isOptionalButtonVisible= */ true);
    }

    @Test
    @Config(qualifiers = TABLET_QUALIFIER)
    public void testUpdateIsInNarrowWindow_tablet_optionalButtonHidden() {
        testUpdateIsInNarrowWindowOnTabletImpl(/* isOptionalButtonVisible= */ false);
    }

    @Test
    @Config(qualifiers = TABLET_QUALIFIER)
    public void testConstructor_tablet_initializesNarrowWindowState() {
        // The horizontal inset makes the window narrower than the min window width of the large
        // form factor layout.
        int horizontalInsetDp =
                mContext.getResources().getConfiguration().screenWidthDp
                        - getLffLayoutMinWindowWidthDp()
                        + 1;
        mCoordinator =
                new ComposeplateCoordinator(
                        (ViewGroup) mComposeplateView.getParent(),
                        /* isLff= */ true,
                        horizontalInsetDp);
        mPropertyModel = mCoordinator.getModelForTesting();

        mCoordinator.setOptionalButtonVisibility(/* visible= */ true);

        verifyButtonLayout(/* isOptionalButtonVisible= */ true, /* expectedUseLffLayout= */ false);
    }

    private void testUpdateIsInNarrowWindowOnTabletImpl(boolean isOptionalButtonVisible) {
        mCoordinator.setOptionalButtonVisibility(isOptionalButtonVisible);
        int minWindowWidthDp = getLffLayoutMinWindowWidthDp();

        updateWindowWidth(minWindowWidthDp, /* horizontalInsetDp= */ 0);
        verifyButtonLayout(isOptionalButtonVisible, /* expectedUseLffLayout= */ true);

        // In narrow windows, the buttons use the same layout as on phones.
        updateWindowWidth(minWindowWidthDp - 1, /* horizontalInsetDp= */ 0);
        verifyButtonLayout(isOptionalButtonVisible, /* expectedUseLffLayout= */ false);

        // The horizontal inset is excluded from the window width.
        updateWindowWidth(minWindowWidthDp, /* horizontalInsetDp= */ 1);
        verifyButtonLayout(isOptionalButtonVisible, /* expectedUseLffLayout= */ false);

        updateWindowWidth(minWindowWidthDp, /* horizontalInsetDp= */ 0);
        verifyButtonLayout(isOptionalButtonVisible, /* expectedUseLffLayout= */ true);
    }

    /**
     * Applies the given window width to the resource configuration and notifies the coordinator to
     * update whether it is in a narrow window.
     *
     * @param windowWidthDp The width of the window in dp.
     * @param horizontalInsetDp The horizontal inset of the window in dp.
     */
    private void updateWindowWidth(int windowWidthDp, int horizontalInsetDp) {
        RuntimeEnvironment.setQualifiers("+w" + windowWidthDp + "dp");
        mCoordinator.updateIsInNarrowWindow(horizontalInsetDp);
    }

    /** Returns the min window width in dp to use the large form factor layout on tablets. */
    private int getLffLayoutMinWindowWidthDp() {
        Resources res = mContext.getResources();
        return Math.round(
                res.getDimension(R.dimen.composeplate_view_lff_layout_min_window_width)
                        / res.getDisplayMetrics().density);
    }

    private void testSetOptionalButtonVisibilityImpl(boolean visible) {
        boolean isLff = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        mCoordinator.setOptionalButtonVisibility(visible);

        assertEquals(
                visible, mPropertyModel.get(ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE));
        assertEquals(visible ? View.VISIBLE : View.GONE, mOptionalButton.getVisibility());
        verifyButtonLayout(visible, /* expectedUseLffLayout= */ isLff);
    }

    /**
     * Verifies the layout and spacing of the buttons.
     *
     * @param isOptionalButtonVisible Whether the optional button is visible.
     * @param expectedUseLffLayout Whether the buttons are expected to use the large form factor
     *     layout, i.e., all buttons show their texts and share the width equally. Otherwise, the
     *     optional button is icon-only and wraps its content, and so does the incognito button when
     *     the optional button is visible.
     */
    private void verifyButtonLayout(boolean isOptionalButtonVisible, boolean expectedUseLffLayout) {
        assertEquals(
                expectedUseLffLayout,
                mPropertyModel.get(ComposeplateProperties.IS_OPTIONAL_BUTTON_TEXT_VISIBLE));
        assertEquals(
                expectedUseLffLayout ? View.VISIBLE : View.GONE,
                mComposeplateView.findViewById(R.id.optional_button_text).getVisibility());
        assertEquals(
                !expectedUseLffLayout,
                mPropertyModel.get(ComposeplateProperties.IS_OPTIONAL_BUTTON_WRAP_CONTENT));
        assertEquals(
                expectedUseLffLayout ? 0 : ViewGroup.LayoutParams.WRAP_CONTENT,
                mOptionalButton.getLayoutParams().width);

        boolean isIncognitoButtonCompact = isOptionalButtonVisible && !expectedUseLffLayout;
        assertEquals(
                !isIncognitoButtonCompact,
                mPropertyModel.get(ComposeplateProperties.IS_INCOGNITO_BUTTON_TEXT_VISIBLE));
        assertEquals(
                isIncognitoButtonCompact ? View.GONE : View.VISIBLE,
                mComposeplateView.findViewById(R.id.incognito_button_text).getVisibility());
        assertEquals(
                isIncognitoButtonCompact,
                mPropertyModel.get(ComposeplateProperties.IS_INCOGNITO_BUTTON_WRAP_CONTENT));
        assertEquals(
                isIncognitoButtonCompact ? ViewGroup.LayoutParams.WRAP_CONTENT : 0,
                mIncognitoButton.getLayoutParams().width);

        // The composeplate button always takes the remaining width.
        assertEquals(0, mComposeplateButton.getLayoutParams().width);

        Resources res = mContext.getResources();
        int expectedOptionalButtonPadding =
                res.getDimensionPixelSize(R.dimen.composeplate_view_button_margin);
        int expectedOptionalButtonMarginEnd = expectedOptionalButtonPadding;
        if (isOptionalButtonVisible) {
            expectedOptionalButtonPadding =
                    res.getDimensionPixelSize(
                            expectedUseLffLayout
                                    ? R.dimen.composeplate_view_optional_button_padding_lff
                                    : R.dimen.composeplate_view_optional_button_padding);
            expectedOptionalButtonMarginEnd =
                    res.getDimensionPixelSize(
                            expectedUseLffLayout
                                    ? R.dimen.composeplate_view_optional_button_margin_lff
                                    : R.dimen.composeplate_view_optional_button_margin);
        }
        verifyButtonSpacing(expectedOptionalButtonPadding, expectedOptionalButtonMarginEnd);
        assertEquals(expectedOptionalButtonPadding, mOptionalButton.getPaddingStart());
        assertEquals(expectedOptionalButtonMarginEnd, getMarginEnd(mOptionalButton));
    }

    private void verifyButtonSpacing(int optionalButtonPadding, int optionalButtonMarginEnd) {
        assertEquals(
                optionalButtonPadding,
                mPropertyModel.get(ComposeplateProperties.OPTIONAL_BUTTON_LATERAL_PADDING));
        assertEquals(
                optionalButtonMarginEnd,
                mPropertyModel.get(ComposeplateProperties.OPTIONAL_BUTTON_MARGIN_END));
    }

    private void verifyComposeplateWidth(int lateralMargin) {
        ViewGroup.MarginLayoutParams layoutParams = new ViewGroup.MarginLayoutParams(100, 100);
        mComposeplateView.setLayoutParams(layoutParams);

        int searchBoxWidth = 400;
        mCoordinator.setLayoutWidth(searchBoxWidth);

        assertEquals(searchBoxWidth - 2 * lateralMargin, layoutParams.width);
    }

    private static int getMarginEnd(View view) {
        return ((ViewGroup.MarginLayoutParams) view.getLayoutParams()).getMarginEnd();
    }
}
