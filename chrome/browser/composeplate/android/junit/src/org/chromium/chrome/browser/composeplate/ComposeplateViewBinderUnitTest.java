// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.StyleRes;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.search_engines.AiModeButtonUiConfig;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ComposeplateViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // TODO(567604165): Remove mocking of Views / Activities
public class ComposeplateViewBinderUnitTest {
    private static final String OPTIONAL_BUTTON_LABEL = "Create image";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ComposeplateView mViewMock;
    @Mock private ImageView mIncognitoButtonView;
    @Mock private View mComposeplateButtonView;
    @Mock private OnClickListener mOnClickListener;

    private Context mContext;
    private PropertyModel mPropertyModel;
    private ComposeplateView mView;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mView =
                (ComposeplateView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.composeplate_view_layout, null);
        mView.setLayoutParams(new ViewGroup.MarginLayoutParams(120, 120));

        mPropertyModel = new PropertyModel.Builder(ComposeplateProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mViewMock, ComposeplateViewBinder::bind);
    }

    @Test
    public void testSetVisibility() {
        mPropertyModel.set(ComposeplateProperties.IS_VISIBLE, true);
        verify(mViewMock).setVisibility(eq(View.VISIBLE));

        mPropertyModel.set(ComposeplateProperties.IS_VISIBLE, false);
        verify(mViewMock).setVisibility(eq(View.GONE));
    }

    @Test
    public void testSetIncognitoButtonClickListener() {
        when(mViewMock.findViewById(eq(R.id.incognito_button))).thenReturn(mIncognitoButtonView);
        mPropertyModel.set(ComposeplateProperties.INCOGNITO_CLICK_LISTENER, mOnClickListener);
        verify(mIncognitoButtonView).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetComposeplateButtonClickListener() {
        when(mViewMock.findViewById(eq(R.id.composeplate_button)))
                .thenReturn(mComposeplateButtonView);
        mPropertyModel.set(
                ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER, mOnClickListener);
        verify(mComposeplateButtonView).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testApplyWhiteBackground_withMockView() {
        mPropertyModel.set(ComposeplateProperties.APPLY_WHITE_BACKGROUND, true);
        verify(mViewMock).applyWhiteBackground(eq(true));

        mPropertyModel.set(ComposeplateProperties.APPLY_WHITE_BACKGROUND, false);
        verify(mViewMock).applyWhiteBackground(eq(false));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA + ":change_button_color/false"})
    public void testApplyWhiteBackground() {
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);

        Drawable defaultBackground =
                mContext.getDrawable(R.drawable.home_surface_search_box_background);

        View composeplateButton = mView.findViewById(R.id.composeplate_button);
        View incognitoButton = mView.findViewById(R.id.incognito_button);

        mPropertyModel.set(ComposeplateProperties.APPLY_WHITE_BACKGROUND, true);
        verifyApplyBackground(composeplateButton);
        verifyApplyBackground(incognitoButton);

        mPropertyModel.set(ComposeplateProperties.APPLY_WHITE_BACKGROUND, false);
        verifyResetBackground(composeplateButton, defaultBackground);
        verifyResetBackground(incognitoButton, defaultBackground);
    }

    @Test
    public void testSetColorStateList_withMockView() {
        ColorStateList colorStateList = mock(ColorStateList.class);
        mPropertyModel.set(ComposeplateProperties.COLOR_STATE_LIST, colorStateList);
        verify(mViewMock).setColorStateList(eq(colorStateList));
    }

    @Test
    public void testSetColorStateList() {
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);

        ColorStateList colorStateList = mContext.getColorStateList(R.color.default_red);
        mPropertyModel.set(ComposeplateProperties.COLOR_STATE_LIST, colorStateList);

        ImageView composeplateIconView = mView.findViewById(R.id.composeplate_button_icon);
        assertEquals(colorStateList, composeplateIconView.getImageTintList());

        ImageView incognitoIconView = mView.findViewById(R.id.incognito_button_icon);
        assertEquals(colorStateList, incognitoIconView.getImageTintList());
    }

    @Test
    public void testSetTextStyle() {
        @StyleRes int textStyleResId = R.style.TextAppearance_MediumStyle;
        mPropertyModel.set(ComposeplateProperties.TEXT_STYLE_RES_ID, textStyleResId);
        verify(mViewMock).setTextStyle(eq(textStyleResId));
    }

    @Test
    public void testSetAiModeButtonUiConfig_withMockView() {
        AiModeButtonUiConfig aiModeButtonUiConfig = createAiModeButtonUiConfig();
        mPropertyModel.set(ComposeplateProperties.AI_MODE_BUTTON_UI_CONFIG, aiModeButtonUiConfig);
        verify(mViewMock).setAiModeButtonUiConfig(eq(aiModeButtonUiConfig));
    }

    @Test
    public void testSetAiModeButtonUiConfig() {
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);

        AiModeButtonUiConfig aiModeButtonUiConfig = createAiModeButtonUiConfig();
        mPropertyModel.set(ComposeplateProperties.AI_MODE_BUTTON_UI_CONFIG, aiModeButtonUiConfig);

        TextView composeplateButtonText = mView.findViewById(R.id.composeplate_button_text);
        assertEquals(aiModeButtonUiConfig.text, composeplateButtonText.getText().toString());

        View composeplateButton = mView.findViewById(R.id.composeplate_button);
        assertEquals(aiModeButtonUiConfig.tooltip, composeplateButton.getTooltipText());
        assertEquals(aiModeButtonUiConfig.a11yLabel, composeplateButton.getContentDescription());
    }

    @Test
    public void testSetAiModeButtonIcon_withMockView() {
        ComposeplateProperties.AiModeButtonIcon icon =
                new ComposeplateProperties.AiModeButtonIcon(
                        new ColorDrawable(Color.RED), /* shouldTint= */ true);
        mPropertyModel.set(ComposeplateProperties.AI_MODE_BUTTON_ICON, icon);
        verify(mViewMock).setAiModeButtonIcon(eq(icon));
    }

    @Test
    public void testSetAiModeButtonIcon_Tinted() {
        testSetAiModeButtonIconImpl(/* shouldTint= */ true);
    }

    @Test
    public void testSetAiModeButtonIcon_NotTinted() {
        testSetAiModeButtonIconImpl(/* shouldTint= */ false);
    }

    /**
     * Verifies that switching from a full color icon back to a monochrome one restores the tint
     * declared in the layout, even if no ColorStateList has been set.
     */
    @Test
    public void testSetAiModeButtonIcon_RestoresLayoutTint() {
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);
        ImageView iconView = mView.findViewById(R.id.composeplate_button_icon);
        ColorStateList layoutTint = iconView.getImageTintList();

        setAiModeButtonIcon(/* shouldTint= */ false);
        assertNull(iconView.getImageTintList());

        setAiModeButtonIcon(/* shouldTint= */ true);
        assertEquals(layoutTint, iconView.getImageTintList());
    }

    @Test
    public void testSetOptionalButtonClickListener_withMockView() {
        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_CLICK_LISTENER, mOnClickListener);
        verify(mViewMock).setOptionalButtonClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetOptionalButtonClickListener() {
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);
        View optionalButton = mView.findViewById(R.id.optional_button);

        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_CLICK_LISTENER, mOnClickListener);
        optionalButton.performClick();
        verify(mOnClickListener).onClick(eq(optionalButton));

        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_CLICK_LISTENER, null);
        assertFalse(optionalButton.hasOnClickListeners());
    }

    @Test
    public void testSetOptionalButtonText_withMockView() {
        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_TEXT, OPTIONAL_BUTTON_LABEL);
        verify(mViewMock).setOptionalButtonText(eq(OPTIONAL_BUTTON_LABEL));
    }

    @Test
    public void testSetOptionalButtonText() {
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);
        View optionalButton = mView.findViewById(R.id.optional_button);
        TextView optionalButtonText = mView.findViewById(R.id.optional_button_text);

        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_TEXT, OPTIONAL_BUTTON_LABEL);
        assertEquals(OPTIONAL_BUTTON_LABEL, optionalButtonText.getText().toString());
        assertEquals(OPTIONAL_BUTTON_LABEL, optionalButton.getContentDescription());

        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_TEXT, null);
        assertEquals("", optionalButtonText.getText().toString());
        assertNull(optionalButton.getContentDescription());
    }

    @Test
    public void testSetOptionalButtonVisibility_withMockView() {
        mPropertyModel.set(ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE, true);
        verify(mViewMock).setOptionalButtonVisibility(eq(true));

        mPropertyModel.set(ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE, false);
        verify(mViewMock).setOptionalButtonVisibility(eq(false));
    }

    @Test
    public void testSetIsLff_withMockView() {
        PropertyModel model =
                new PropertyModel.Builder(ComposeplateProperties.ALL_KEYS)
                        .with(ComposeplateProperties.IS_LFF, true)
                        .build();
        PropertyModelChangeProcessor.create(model, mViewMock, ComposeplateViewBinder::bind);
        verify(mViewMock).setIsLff(eq(true));
    }

    @Test
    public void testSetOptionalButtonVisibility_phone() {
        testSetOptionalButtonVisibilityImpl(/* isLff= */ false);
    }

    @Test
    public void testSetOptionalButtonVisibility_lff() {
        testSetOptionalButtonVisibilityImpl(/* isLff= */ true);
    }

    @Test
    public void testSetIncognitoButtonTextVisibility_withMockView() {
        mPropertyModel.set(ComposeplateProperties.IS_INCOGNITO_BUTTON_TEXT_VISIBLE, true);
        verify(mViewMock).setIncognitoButtonTextVisibility(eq(true));

        mPropertyModel.set(ComposeplateProperties.IS_INCOGNITO_BUTTON_TEXT_VISIBLE, false);
        verify(mViewMock).setIncognitoButtonTextVisibility(eq(false));
    }

    @Test
    public void testSetIncognitoButtonTextVisibility() {
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);
        View incognitoButtonText = mView.findViewById(R.id.incognito_button_text);

        mPropertyModel.set(ComposeplateProperties.IS_INCOGNITO_BUTTON_TEXT_VISIBLE, false);
        assertEquals(View.GONE, incognitoButtonText.getVisibility());

        mPropertyModel.set(ComposeplateProperties.IS_INCOGNITO_BUTTON_TEXT_VISIBLE, true);
        assertEquals(View.VISIBLE, incognitoButtonText.getVisibility());
    }

    @Test
    public void testSetOptionalButtonIcon_withMockView() {
        @DrawableRes int iconResId = R.drawable.composeplate_button_foreground;
        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_ICON_RES_ID, iconResId);
        verify(mViewMock).setOptionalButtonIcon(eq(iconResId));
    }

    @Test
    public void testSetOptionalButtonIcon() {
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);
        ImageView iconView = mView.findViewById(R.id.optional_button_icon);

        @DrawableRes int iconResId = R.drawable.composeplate_button_foreground;
        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_ICON_RES_ID, iconResId);
        assertEquals(iconResId, Shadows.shadowOf(iconView.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testSetButtonLateralPadding_withMockView() {
        int padding = 10;
        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_LATERAL_PADDING, padding);
        verify(mViewMock).setButtonLateralPadding(eq(padding));
    }

    @Test
    public void testSetButtonLateralPadding() {
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);
        View composeplateButton = mView.findViewById(R.id.composeplate_button);
        View optionalButton = mView.findViewById(R.id.optional_button);
        View incognitoButton = mView.findViewById(R.id.incognito_button);
        int composeplatePaddingStart = composeplateButton.getPaddingStart();
        int composeplatePaddingEnd = composeplateButton.getPaddingEnd();

        int padding = 17;
        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_LATERAL_PADDING, padding);

        assertEquals(padding, optionalButton.getPaddingStart());
        assertEquals(padding, optionalButton.getPaddingEnd());
        assertEquals(padding, incognitoButton.getPaddingStart());
        assertEquals(padding, incognitoButton.getPaddingEnd());
        // The composeplate button's padding isn't changed.
        assertEquals(composeplatePaddingStart, composeplateButton.getPaddingStart());
        assertEquals(composeplatePaddingEnd, composeplateButton.getPaddingEnd());
    }

    @Test
    public void testSetButtonMarginEnd_withMockView() {
        int marginEnd = 10;
        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_MARGIN_END, marginEnd);
        verify(mViewMock).setButtonMarginEnd(eq(marginEnd));
    }

    @Test
    public void testSetButtonMarginEnd() {
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);
        View composeplateButton = mView.findViewById(R.id.composeplate_button);
        View optionalButton = mView.findViewById(R.id.optional_button);
        View incognitoButton = mView.findViewById(R.id.incognito_button);
        int incognitoMarginEnd = getMarginEnd(incognitoButton);

        int marginEnd = 23;
        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_MARGIN_END, marginEnd);

        assertEquals(marginEnd, getMarginEnd(composeplateButton));
        assertEquals(marginEnd, getMarginEnd(optionalButton));
        // The incognito button's margin isn't changed.
        assertEquals(incognitoMarginEnd, getMarginEnd(incognitoButton));
    }

    private void testSetAiModeButtonIconImpl(boolean shouldTint) {
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);
        ImageView iconView = mView.findViewById(R.id.composeplate_button_icon);
        ColorStateList colorStateList = mContext.getColorStateList(R.color.default_red);
        mPropertyModel.set(ComposeplateProperties.COLOR_STATE_LIST, colorStateList);

        Drawable drawable = setAiModeButtonIcon(shouldTint);

        assertEquals(drawable, iconView.getDrawable());
        assertEquals(shouldTint ? colorStateList : null, iconView.getImageTintList());

        // A later ColorStateList, e.g. from a background change, must not tint a full color icon.
        ColorStateList newColorStateList =
                mContext.getColorStateList(R.color.default_icon_color_dark);
        mPropertyModel.set(ComposeplateProperties.COLOR_STATE_LIST, newColorStateList);
        assertEquals(shouldTint ? newColorStateList : null, iconView.getImageTintList());
    }

    private Drawable setAiModeButtonIcon(boolean shouldTint) {
        Drawable drawable = new ColorDrawable(Color.RED);
        mPropertyModel.set(
                ComposeplateProperties.AI_MODE_BUTTON_ICON,
                new ComposeplateProperties.AiModeButtonIcon(drawable, shouldTint));
        return drawable;
    }

    private void testSetOptionalButtonVisibilityImpl(boolean isLff) {
        mPropertyModel =
                new PropertyModel.Builder(ComposeplateProperties.ALL_KEYS)
                        .with(ComposeplateProperties.IS_LFF, isLff)
                        .build();
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);
        View optionalButton = mView.findViewById(R.id.optional_button);
        View optionalButtonText = mView.findViewById(R.id.optional_button_text);
        View incognitoButton = mView.findViewById(R.id.incognito_button);
        View composeplateButton = mView.findViewById(R.id.composeplate_button);

        // On phones, the optional and incognito buttons wrap their content. On LFF, all buttons
        // show their text and share the width equally.
        mPropertyModel.set(ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, optionalButton.getVisibility());
        assertEquals(isLff ? View.VISIBLE : View.GONE, optionalButtonText.getVisibility());
        verifyButtonWidth(composeplateButton, /* isWeighted= */ true);
        verifyButtonWidth(optionalButton, /* isWeighted= */ isLff);
        verifyButtonWidth(incognitoButton, /* isWeighted= */ isLff);

        // Hiding the optional button restores the default layout.
        mPropertyModel.set(ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE, false);
        assertEquals(View.GONE, optionalButton.getVisibility());
        verifyButtonWidth(composeplateButton, /* isWeighted= */ true);
        verifyButtonWidth(incognitoButton, /* isWeighted= */ true);
    }

    private void verifyButtonWidth(View button, boolean isWeighted) {
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) button.getLayoutParams();
        assertEquals(isWeighted ? 0 : ViewGroup.LayoutParams.WRAP_CONTENT, layoutParams.width);
        assertEquals(isWeighted ? 1f : 0f, layoutParams.weight, /* delta= */ 0f);
    }

    private static int getMarginEnd(View view) {
        return ((ViewGroup.MarginLayoutParams) view.getLayoutParams()).getMarginEnd();
    }

    private static AiModeButtonUiConfig createAiModeButtonUiConfig() {
        return new AiModeButtonUiConfig(
                "AI Mode",
                "Ask AI Mode",
                "AI Mode button",
                "Always show AI Mode",
                "Ask AI Mode",
                /* faviconUrl= */ JUnitTestGURLs.RED_1,
                /* navigationUrl= */ "https://www.red.com/search?q={searchTerms}",
                /* navigationUrlEmpty= */ JUnitTestGURLs.URL_2);
    }

    private void verifyApplyBackground(View view) {
        // Verifies that the background is set to color white.
        Drawable whiteBackground = view.getBackground();
        assertTrue(whiteBackground instanceof GradientDrawable);
        assertEquals(
                Color.WHITE, ((GradientDrawable) whiteBackground).getColor().getDefaultColor());
    }

    private void verifyResetBackground(View view, Drawable defaultBackground) {
        // Verifies that the background of the view is to reset.
        assertEquals(
                ((GradientDrawable) defaultBackground).getColor().getDefaultColor(),
                ((GradientDrawable) view.getBackground()).getColor().getDefaultColor());
    }
}
