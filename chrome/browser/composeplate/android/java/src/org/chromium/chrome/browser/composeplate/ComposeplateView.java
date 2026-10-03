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

    /** The tint of the icons, initially the one declared in the layout. */
    private @Nullable ColorStateList mIconTint;

    /** Whether the AI Mode button icon should be tinted, see {@link #setAiModeButtonIcon}. */
    private boolean mShouldTintAiModeButtonIcon = true;

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

        if (mIncognitoButton != null) {
            setTextStyle(mIncognitoButton.findViewById(R.id.incognito_button_text), textStyleResId);
        }
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
     * Sets the content description of the optional button.
     *
     * @param contentDescription The content description to set on the optional button.
     */
    void setOptionalButtonContentDescription(@Nullable String contentDescription) {
        if (mOptionalButton == null) return;

        mOptionalButton.setContentDescription(contentDescription);
    }

    /**
     * Updates the visibility of the optional button. When the optional button is visible, the
     * incognito button's text is hidden and the incognito button wraps its content, so that the
     * composeplate button takes the rest of the width. Otherwise, the default layout is restored.
     *
     * @param visible Whether the optional button is visible.
     */
    void setOptionalButtonVisibility(boolean visible) {
        if (mOptionalButton != null) {
            mOptionalButton.setVisibility(visible ? View.VISIBLE : View.GONE);
        }

        if (mIncognitoButton != null) {
            View incognitoButtonText = mIncognitoButton.findViewById(R.id.incognito_button_text);
            if (incognitoButtonText != null) {
                incognitoButtonText.setVisibility(visible ? View.GONE : View.VISIBLE);
            }
        }

        // The composeplate button always keeps width 0 and weight 1 as declared in the layout, so
        // it fills the remaining space.
        int width = visible ? LayoutParams.WRAP_CONTENT : 0;
        float weight = visible ? 0f : 1f;
        updateButtonLayoutParams(mIncognitoButton, width, weight);
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
