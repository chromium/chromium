// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

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
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ComposeplateViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ComposeplateViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
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
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);
    }

    @Test
    public void testSetVisibility() {
        mPropertyModel.set(ComposeplateProperties.IS_VISIBLE, true);
        assertEquals(View.VISIBLE, mView.getVisibility());

        mPropertyModel.set(ComposeplateProperties.IS_VISIBLE, false);
        assertEquals(View.GONE, mView.getVisibility());
    }

    @Test
    public void testSetComposeplateButtonClickListener() {
        View composeplateButton = mView.findViewById(R.id.composeplate_button);
        mPropertyModel.set(
                ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER, mOnClickListener);
        composeplateButton.performClick();
        verify(mOnClickListener).onClick(eq(composeplateButton));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA + ":change_button_color/false"})
    public void testApplyWhiteBackground() {
        Drawable defaultBackground =
                mContext.getDrawable(R.drawable.home_surface_search_box_background);

        View composeplateButton = mView.findViewById(R.id.composeplate_button);
        View trailingButton = mView.findViewById(R.id.trailing_button);

        mPropertyModel.set(ComposeplateProperties.APPLY_WHITE_BACKGROUND, true);
        verifyApplyBackground(composeplateButton);
        verifyApplyBackground(trailingButton);

        mPropertyModel.set(ComposeplateProperties.APPLY_WHITE_BACKGROUND, false);
        verifyResetBackground(composeplateButton, defaultBackground);
        verifyResetBackground(trailingButton, defaultBackground);
    }

    @Test
    public void testSetColorStateList() {
        ColorStateList colorStateList = mContext.getColorStateList(R.color.default_red);
        mPropertyModel.set(ComposeplateProperties.COLOR_STATE_LIST, colorStateList);

        ImageView composeplateIconView = mView.findViewById(R.id.composeplate_button_icon);
        assertEquals(colorStateList, composeplateIconView.getImageTintList());

        ImageView trailingButtonIconView = mView.findViewById(R.id.trailing_button_icon);
        assertEquals(colorStateList, trailingButtonIconView.getImageTintList());
    }

    @Test
    public void testSetTextStyle() {
        @StyleRes int textStyleResId = R.style.TextAppearance_ComposeplateTextMediumDark;
        TextView expectedTextView = new TextView(mContext);
        expectedTextView.setTextAppearance(textStyleResId);
        int expectedColor = expectedTextView.getCurrentTextColor();
        TextView composeplateButtonText = mView.findViewById(R.id.composeplate_button_text);
        TextView trailingButtonText = mView.findViewById(R.id.trailing_button_text);
        // Preset a different text appearance so that applying the style is observable.
        composeplateButtonText.setTextAppearance(R.style.TextAppearance_TextMedium_Accent1);
        trailingButtonText.setTextAppearance(R.style.TextAppearance_TextMedium_Accent1);
        assertNotEquals(expectedColor, composeplateButtonText.getCurrentTextColor());
        assertNotEquals(expectedColor, trailingButtonText.getCurrentTextColor());

        mPropertyModel.set(ComposeplateProperties.TEXT_STYLE_RES_ID, textStyleResId);

        assertEquals(expectedColor, composeplateButtonText.getCurrentTextColor());
        assertEquals(expectedColor, trailingButtonText.getCurrentTextColor());
    }

    @Test
    public void testSetAiModeButtonUiConfig() {
        AiModeButtonUiConfig aiModeButtonUiConfig = createAiModeButtonUiConfig();
        mPropertyModel.set(ComposeplateProperties.AI_MODE_BUTTON_UI_CONFIG, aiModeButtonUiConfig);

        TextView composeplateButtonText = mView.findViewById(R.id.composeplate_button_text);
        assertEquals(aiModeButtonUiConfig.text, composeplateButtonText.getText().toString());

        View composeplateButton = mView.findViewById(R.id.composeplate_button);
        assertEquals(aiModeButtonUiConfig.tooltip, composeplateButton.getTooltipText());
        assertEquals(aiModeButtonUiConfig.a11yLabel, composeplateButton.getContentDescription());
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
        ImageView iconView = mView.findViewById(R.id.composeplate_button_icon);
        ColorStateList layoutTint = iconView.getImageTintList();

        setAiModeButtonIcon(/* shouldTint= */ false);
        assertNull(iconView.getImageTintList());

        setAiModeButtonIcon(/* shouldTint= */ true);
        assertEquals(layoutTint, iconView.getImageTintList());
    }

    @Test
    public void testSetOptionalButtonClickListener() {
        testSetButtonClickListenerImpl(
                ComposeplateProperties.OPTIONAL_BUTTON_CLICK_LISTENER,
                mView.findViewById(R.id.optional_button));
    }

    @Test
    public void testSetTrailingButtonClickListener() {
        testSetButtonClickListenerImpl(
                ComposeplateProperties.TRAILING_BUTTON_CLICK_LISTENER,
                mView.findViewById(R.id.trailing_button));
    }

    @Test
    public void testSetOptionalButtonText() {
        testSetButtonTextImpl(
                ComposeplateProperties.OPTIONAL_BUTTON_TEXT,
                mView.findViewById(R.id.optional_button),
                mView.findViewById(R.id.optional_button_text));
    }

    @Test
    public void testSetTrailingButtonText() {
        testSetButtonTextImpl(
                ComposeplateProperties.TRAILING_BUTTON_TEXT,
                mView.findViewById(R.id.trailing_button),
                mView.findViewById(R.id.trailing_button_text));
    }

    @Test
    public void testSetOptionalButtonContentDescription() {
        testSetButtonContentDescriptionImpl(
                ComposeplateProperties.OPTIONAL_BUTTON_CONTENT_DESCRIPTION,
                mView.findViewById(R.id.optional_button),
                mView.findViewById(R.id.optional_button_text));
    }

    @Test
    public void testSetTrailingButtonContentDescription() {
        testSetButtonContentDescriptionImpl(
                ComposeplateProperties.TRAILING_BUTTON_CONTENT_DESCRIPTION,
                mView.findViewById(R.id.trailing_button),
                mView.findViewById(R.id.trailing_button_text));
    }

    @Test
    public void testSetOptionalButtonVisibility() {
        testSetViewVisibilityImpl(
                ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE,
                mView.findViewById(R.id.optional_button));
    }

    @Test
    public void testSetOptionalButtonTextVisibility() {
        testSetViewVisibilityImpl(
                ComposeplateProperties.IS_OPTIONAL_BUTTON_TEXT_VISIBLE,
                mView.findViewById(R.id.optional_button_text));
    }

    @Test
    public void testSetTrailingButtonTextVisibility() {
        testSetViewVisibilityImpl(
                ComposeplateProperties.IS_TRAILING_BUTTON_TEXT_VISIBLE,
                mView.findViewById(R.id.trailing_button_text));
    }

    @Test
    public void testSetOptionalButtonWrapContent() {
        testSetButtonWrapContentImpl(
                ComposeplateProperties.IS_OPTIONAL_BUTTON_WRAP_CONTENT,
                mView.findViewById(R.id.optional_button));
    }

    @Test
    public void testSetTrailingButtonWrapContent() {
        testSetButtonWrapContentImpl(
                ComposeplateProperties.IS_TRAILING_BUTTON_WRAP_CONTENT,
                mView.findViewById(R.id.trailing_button));
    }

    @Test
    public void testSetOptionalButtonIcon() {
        testSetButtonIconImpl(
                ComposeplateProperties.OPTIONAL_BUTTON_ICON_RES_ID,
                mView.findViewById(R.id.optional_button_icon));
    }

    @Test
    public void testSetTrailingButtonIcon() {
        testSetButtonIconImpl(
                ComposeplateProperties.TRAILING_BUTTON_ICON_RES_ID,
                mView.findViewById(R.id.trailing_button_icon));
    }

    @Test
    public void testSetButtonLateralPadding() {
        View composeplateButton = mView.findViewById(R.id.composeplate_button);
        View optionalButton = mView.findViewById(R.id.optional_button);
        View trailingButton = mView.findViewById(R.id.trailing_button);
        int composeplatePaddingStart = composeplateButton.getPaddingStart();
        int composeplatePaddingEnd = composeplateButton.getPaddingEnd();

        int padding = 17;
        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_LATERAL_PADDING, padding);

        assertEquals(padding, optionalButton.getPaddingStart());
        assertEquals(padding, optionalButton.getPaddingEnd());
        assertEquals(padding, trailingButton.getPaddingStart());
        assertEquals(padding, trailingButton.getPaddingEnd());
        // The composeplate button's padding isn't changed.
        assertEquals(composeplatePaddingStart, composeplateButton.getPaddingStart());
        assertEquals(composeplatePaddingEnd, composeplateButton.getPaddingEnd());
    }

    @Test
    public void testSetButtonMarginEnd() {
        View composeplateButton = mView.findViewById(R.id.composeplate_button);
        View optionalButton = mView.findViewById(R.id.optional_button);
        View trailingButton = mView.findViewById(R.id.trailing_button);
        int trailingButtonMarginEnd = getMarginEnd(trailingButton);

        int marginEnd = 23;
        mPropertyModel.set(ComposeplateProperties.OPTIONAL_BUTTON_MARGIN_END, marginEnd);

        assertEquals(marginEnd, getMarginEnd(composeplateButton));
        assertEquals(marginEnd, getMarginEnd(optionalButton));
        // The trailing button's margin isn't changed.
        assertEquals(trailingButtonMarginEnd, getMarginEnd(trailingButton));
    }

    private void testSetAiModeButtonIconImpl(boolean shouldTint) {
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

    private void testSetButtonClickListenerImpl(
            WritableObjectPropertyKey<OnClickListener> key, View button) {
        mPropertyModel.set(key, mOnClickListener);
        button.performClick();
        verify(mOnClickListener).onClick(eq(button));

        mPropertyModel.set(key, null);
        assertFalse(button.hasOnClickListeners());
    }

    private void testSetButtonTextImpl(
            WritableObjectPropertyKey<String> key, View button, TextView buttonText) {
        CharSequence defaultContentDescription = button.getContentDescription();
        String text = "Create";

        mPropertyModel.set(key, text);
        assertEquals(text, buttonText.getText().toString());
        // The text doesn't affect the content description.
        assertEquals(defaultContentDescription, button.getContentDescription());

        mPropertyModel.set(key, null);
        assertEquals("", buttonText.getText().toString());
        assertEquals(defaultContentDescription, button.getContentDescription());
    }

    private void testSetButtonContentDescriptionImpl(
            WritableObjectPropertyKey<String> key, View button, TextView buttonText) {
        String defaultText = buttonText.getText().toString();
        String contentDescription = "Create image";

        mPropertyModel.set(key, contentDescription);
        assertEquals(contentDescription, button.getContentDescription());
        assertEquals(contentDescription, button.getTooltipText());
        // The content description doesn't affect the text.
        assertEquals(defaultText, buttonText.getText().toString());

        mPropertyModel.set(key, null);
        assertNull(button.getContentDescription());
        assertNull(button.getTooltipText());
        assertEquals(defaultText, buttonText.getText().toString());
    }

    private void testSetButtonIconImpl(WritableIntPropertyKey key, ImageView iconView) {
        @DrawableRes int iconResId = R.drawable.composeplate_button_foreground;
        mPropertyModel.set(key, iconResId);
        assertEquals(iconResId, Shadows.shadowOf(iconView.getDrawable()).getCreatedFromResId());
    }

    private void testSetViewVisibilityImpl(WritableBooleanPropertyKey key, View view) {
        mPropertyModel.set(key, /* value= */ true);
        assertEquals(View.VISIBLE, view.getVisibility());

        mPropertyModel.set(key, /* value= */ false);
        assertEquals(View.GONE, view.getVisibility());
    }

    private void testSetButtonWrapContentImpl(WritableBooleanPropertyKey key, View button) {
        // If the button wraps its content, it doesn't share the width with the other buttons.
        mPropertyModel.set(key, /* value= */ true);
        verifyButtonWidth(button, /* isWeighted= */ false);

        mPropertyModel.set(key, /* value= */ false);
        verifyButtonWidth(button, /* isWeighted= */ true);
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
