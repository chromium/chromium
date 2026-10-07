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
    private static final String LANDSCAPE_QUALIFIER = "+land";

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
                        parentView, DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));
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
                ActionChips.CANVAS, ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_CANVAS_BUTTON);
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
    @Config(qualifiers = "sw600dp")
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
    public void testSetOptionalButtonVisibility_visible() {
        testSetOptionalButtonVisibilityImpl(/* visible= */ true);
    }

    @Test
    public void testSetOptionalButtonVisibility_hidden() {
        testSetOptionalButtonVisibilityImpl(/* visible= */ false);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testSetOptionalButtonVisibility_visible_tablet() {
        testSetOptionalButtonVisibilityImpl(/* visible= */ true);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testSetOptionalButtonVisibility_hidden_tablet() {
        testSetOptionalButtonVisibilityImpl(/* visible= */ false);
    }

    @Test
    public void testSetOptionalButtonText() {
        String text = "Create image";
        mCoordinator.setOptionalButtonText(text);

        assertEquals(text, mPropertyModel.get(ComposeplateProperties.OPTIONAL_BUTTON_TEXT));
        assertEquals(text, mOptionalButton.getContentDescription());
        TextView optionalButtonText = mComposeplateView.findViewById(R.id.optional_button_text);
        assertEquals(text, optionalButtonText.getText().toString());
    }

    @Test
    public void testOnDisplayStyleChanged_optionalButtonVisible() {
        Resources res = mContext.getResources();
        mCoordinator.setOptionalButtonVisibility(/* visible= */ true);
        int portraitPadding =
                res.getDimensionPixelSize(R.dimen.composeplate_view_optional_button_padding);
        assertEquals(
                portraitPadding,
                mPropertyModel.get(ComposeplateProperties.OPTIONAL_BUTTON_LATERAL_PADDING));

        RuntimeEnvironment.setQualifiers(LANDSCAPE_QUALIFIER);
        mCoordinator.onDisplayStyleChanged();

        int landscapePadding =
                res.getDimensionPixelSize(R.dimen.composeplate_view_optional_button_padding);
        assertNotEquals(portraitPadding, landscapePadding);
        verifyButtonSpacing(
                landscapePadding,
                res.getDimensionPixelSize(R.dimen.composeplate_view_optional_button_margin));
        assertEquals(landscapePadding, mOptionalButton.getPaddingStart());
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

    private void testSetOptionalButtonVisibilityImpl(boolean visible) {
        boolean isLff = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        assertEquals(isLff, mPropertyModel.get(ComposeplateProperties.IS_LFF));
        mCoordinator.setOptionalButtonVisibility(visible);

        assertEquals(
                visible, mPropertyModel.get(ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE));
        assertEquals(visible ? View.VISIBLE : View.GONE, mOptionalButton.getVisibility());
        // The optional button's text is only shown on large form factors.
        View optionalButtonText = mComposeplateView.findViewById(R.id.optional_button_text);
        assertEquals(isLff ? View.VISIBLE : View.GONE, optionalButtonText.getVisibility());

        // The incognito button's text is hidden only when the optional button is visible on
        // phones.
        boolean expectedIncognitoButtonTextVisible = !visible || isLff;
        assertEquals(
                expectedIncognitoButtonTextVisible,
                mPropertyModel.get(ComposeplateProperties.IS_INCOGNITO_BUTTON_TEXT_VISIBLE));
        View incognitoButtonText = mComposeplateView.findViewById(R.id.incognito_button_text);
        assertEquals(
                expectedIncognitoButtonTextVisible ? View.VISIBLE : View.GONE,
                incognitoButtonText.getVisibility());

        Resources res = mContext.getResources();
        int defaultSpacing = res.getDimensionPixelSize(R.dimen.composeplate_view_button_margin);
        int expectedOptionalButtonPadding = defaultSpacing;
        int expectedOptionalButtonMarginEnd = defaultSpacing;
        if (visible) {
            expectedOptionalButtonPadding =
                    res.getDimensionPixelSize(R.dimen.composeplate_view_optional_button_padding);
            expectedOptionalButtonMarginEnd =
                    res.getDimensionPixelSize(R.dimen.composeplate_view_optional_button_margin);
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
