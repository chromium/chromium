// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.StyleRes;
import androidx.core.view.ViewCompat;

import org.jni_zero.internal.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.search_engines.AiModeButtonUiConfig;

@NullMarked
/** View for the composeplate layout which is shown below the fake search box on NTP. */
public class ComposeplateView extends LinearLayout {

    private @Nullable View mComposeplateButton;
    private @Nullable View mOptionalButton;
    private @Nullable View mIncognitoButton;
    private @Nullable ImageView mComposeplateButtonIcon;
    private @Nullable ImageView mOptionalButtonIcon;
    private @Nullable TextView mOptionalButtonText;
    private @Nullable TextView mIncognitoButtonText;

    /** The tint of the icons, initially the one declared in the layout. */
    private @Nullable ColorStateList mIconTint;

    /** Whether the AI Mode button icon should be tinted, see {@link #setAiModeButtonIcon}. */
    private boolean mShouldTintAiModeButtonIcon = true;

    /** Whether the device is a large form factor. */
    private boolean mIsLff;

    public ComposeplateView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mComposeplateButton = findViewById(R.id.composeplate_button);
        mOptionalButton = findViewById(R.id.optional_button);
        mIncognitoButton = findViewById(R.id.incognito_button);
        mComposeplateButtonIcon = findViewById(R.id.composeplate_button_icon);
        mOptionalButtonIcon = findViewById(R.id.optional_button_icon);
        mOptionalButtonText = findViewById(R.id.optional_button_text);
        mIncognitoButtonText = findViewById(R.id.incognito_button_text);
        if (mComposeplateButtonIcon != null) {
            mIconTint = mComposeplateButtonIcon.getImageTintList();
        }
    }

    /**
     * Applies a white background or resets to the default background.
     *
     * @param apply Whether to apply or reset to the default background.
     */
    void applyWhiteBackground(boolean apply) {
        Context context = getContext();
        if (mComposeplateButton != null) {
            ComposeplateUtils.applyComposeplateBackground(context, mComposeplateButton, apply);
        }

        if (mOptionalButton != null) {
            ComposeplateUtils.applyComposeplateBackground(context, mOptionalButton, apply);
        }

        if (mIncognitoButton != null) {
            ComposeplateUtils.applyComposeplateBackground(context, mIncognitoButton, apply);
        }
    }

    /** Sets the ColorStateList to tint the icons on the buttons. */
    void setColorStateList(@Nullable ColorStateList colorStateList) {
        if (colorStateList == null) return;

        mIconTint = colorStateList;
        // A full color AI Mode button icon keeps its own colors.
        if (mShouldTintAiModeButtonIcon) {
            setColorStateList(mComposeplateButtonIcon, colorStateList);
        }

        if (mOptionalButtonIcon != null) {
            setColorStateList(mOptionalButtonIcon, colorStateList);
        }

        if (mIncognitoButton != null) {
            setColorStateList(
                    mIncognitoButton.findViewById(R.id.incognito_button_icon), colorStateList);
        }
    }

    /**
     * Sets the text appearance of the texts on the buttons.
     *
     * @param textStyleResId The resource id of the text appearance.
     */
    void setTextStyle(@StyleRes int textStyleResId) {
        if (mComposeplateButton != null) {
            setTextStyle(
                    mComposeplateButton.findViewById(R.id.composeplate_button_text),
                    textStyleResId);
        }

        setTextStyle(mIncognitoButtonText, textStyleResId);
        setTextStyle(mOptionalButtonText, textStyleResId);
    }

    /**
     * Updates the AI Mode button with the strings provided by the default search engine.
     *
     * @param aiModeButtonUiConfig The {@link AiModeButtonUiConfig} of the default search engine.
     */
    void setAiModeButtonUiConfig(AiModeButtonUiConfig aiModeButtonUiConfig) {
        if (mComposeplateButton == null) return;

        TextView textView = mComposeplateButton.findViewById(R.id.composeplate_button_text);
        if (textView != null) {
            textView.setText(aiModeButtonUiConfig.text);
        }

        ViewCompat.setTooltipText(mComposeplateButton, aiModeButtonUiConfig.tooltip);
        mComposeplateButton.setContentDescription(aiModeButtonUiConfig.a11yLabel);
    }

    /**
     * Updates the icon of the AI Mode button.
     *
     * @param aiModeButtonIcon The icon to show on the AI Mode button, and whether to tint it.
     */
    void setAiModeButtonIcon(ComposeplateProperties.AiModeButtonIcon aiModeButtonIcon) {
        if (mComposeplateButtonIcon == null) return;

        mShouldTintAiModeButtonIcon = aiModeButtonIcon.shouldTint;
        mComposeplateButtonIcon.setImageDrawable(aiModeButtonIcon.drawable);
        mComposeplateButtonIcon.setImageTintList(mShouldTintAiModeButtonIcon ? mIconTint : null);
    }

    /**
     * Updates the icon of the optional button.
     *
     * @param iconResId The resource id of the icon drawable to show on the optional button.
     */
    void setOptionalButtonIcon(@DrawableRes int iconResId) {
        if (mOptionalButtonIcon == null) return;

        mOptionalButtonIcon.setImageResource(iconResId);
    }

    /**
     * Sets the click listener of the optional button.
     *
     * @param listener The click listener to set on the optional button.
     */
    void setOptionalButtonClickListener(@Nullable OnClickListener listener) {
        if (mOptionalButton == null) return;

        mOptionalButton.setOnClickListener(listener);
    }

    /**
     * Updates the text of the optional button. The text is also used as the content description of
     * the button, since the text is only shown on large form factors.
     *
     * @param text The text of the optional button.
     */
    void setOptionalButtonText(@Nullable String text) {
        if (mOptionalButton == null) return;

        mOptionalButton.setContentDescription(text);
        mOptionalButton.setTooltipText(text);
        if (mOptionalButtonText != null) {
            mOptionalButtonText.setText(text);
        }
    }

    /**
     * Sets whether the device is a large form factor, which changes the layout of the buttons when
     * the optional button is visible.
     *
     * @param isLff Whether the device is a large form factor.
     */
    void setIsLff(boolean isLff) {
        mIsLff = isLff;
    }

    /**
     * Updates the visibility of the optional button. When the optional button is visible:
     *
     * <ul>
     *   <li>On large form factors, the optional button shows its text and all three buttons share
     *       the width equally.
     *   <li>Otherwise, the optional button is icon-only, and the optional and incognito buttons
     *       wrap their content, so that the composeplate button takes the rest of the width. The
     *       incognito button's text is hidden via {@link #setIncognitoButtonTextVisibility}.
     * </ul>
     *
     * When the optional button is hidden, the default layout is restored.
     *
     * @param visible Whether the optional button is visible.
     */
    void setOptionalButtonVisibility(boolean visible) {
        if (mOptionalButton != null) {
            mOptionalButton.setVisibility(visible ? View.VISIBLE : View.GONE);
        }

        if (mOptionalButtonText != null) {
            mOptionalButtonText.setVisibility(mIsLff ? View.VISIBLE : View.GONE);
        }

        // The incognito and optional buttons wrap their content only on mobiles.
        // If the width of the button is WRAP_CONTENT, its weight is set to 0; If its width is set
        // to 0dp, the weight is set to 1, i.e., shares the width equally with the other buttons.
        boolean isIncognitoButtonWrapContent = visible && !mIsLff;
        updateButtonLayoutParams(
                mIncognitoButton,
                getWidth(isIncognitoButtonWrapContent),
                getWeight(isIncognitoButtonWrapContent));

        boolean isOptionalButtonWrapContent = !mIsLff;
        updateButtonLayoutParams(
                mOptionalButton,
                getWidth(isOptionalButtonWrapContent),
                getWeight(isOptionalButtonWrapContent));
    }

    /**
     * Returns the layout width of a button.
     *
     * @param wrapContent Whether the button wraps its content instead of sharing the remaining
     *     width by weight.
     * @return {@link LayoutParams#WRAP_CONTENT} if wrapping content, otherwise 0 so the width is
     *     determined by the weight.
     */
    private int getWidth(boolean wrapContent) {
        return wrapContent ? LayoutParams.WRAP_CONTENT : 0;
    }

    /**
     * Returns the layout weight of a button.
     *
     * @param wrapContent Whether the button wraps its content instead of sharing the remaining
     *     width by weight.
     * @return 0 if wrapping content, otherwise 1 so the button shares the remaining width equally
     *     with the other weighted buttons.
     */
    private float getWeight(boolean wrapContent) {
        return wrapContent ? 0f : 1f;
    }

    /**
     * Updates the visibility of the incognito button's text.
     *
     * @param visible Whether the incognito button's text is visible.
     */
    void setIncognitoButtonTextVisibility(boolean visible) {
        if (mIncognitoButtonText == null) return;

        mIncognitoButtonText.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /**
     * Sets the lateral (start and end) padding of the optional and incognito buttons.
     *
     * @param padding The padding in pixels.
     */
    void setButtonLateralPadding(int padding) {
        updateButtonHorizontalPadding(mOptionalButton, padding);
        updateButtonHorizontalPadding(mIncognitoButton, padding);
    }

    /**
     * Sets the end margin of the composeplate and optional buttons.
     *
     * @param marginEnd The end margin in pixels.
     */
    void setButtonMarginEnd(int marginEnd) {
        updateButtonMarginEnd(mComposeplateButton, marginEnd);
        updateButtonMarginEnd(mOptionalButton, marginEnd);
    }

    private void updateButtonLayoutParams(@Nullable View button, int width, float weight) {
        if (button == null) return;

        LayoutParams layoutParams = (LayoutParams) button.getLayoutParams();
        layoutParams.width = width;
        layoutParams.weight = weight;
        button.setLayoutParams(layoutParams);
    }

    private void updateButtonMarginEnd(@Nullable View button, int marginEnd) {
        if (button == null) return;

        LayoutParams layoutParams = (LayoutParams) button.getLayoutParams();
        layoutParams.setMarginEnd(marginEnd);
        button.setLayoutParams(layoutParams);
    }

    private void updateButtonHorizontalPadding(@Nullable View button, int horizontalPadding) {
        if (button == null) return;

        button.setPaddingRelative(
                horizontalPadding,
                button.getPaddingTop(),
                horizontalPadding,
                button.getPaddingBottom());
    }

    private void setColorStateList(@Nullable ImageView view, ColorStateList colorStateList) {
        if (view == null) return;

        view.setImageTintList(colorStateList);
    }

    private void setTextStyle(@Nullable TextView view, @StyleRes int textStyleResId) {
        if (view == null) return;

        view.setTextAppearance(textStyleResId);
    }
}
