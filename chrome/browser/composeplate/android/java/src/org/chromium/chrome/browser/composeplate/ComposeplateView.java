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

import androidx.annotation.StyleRes;
import androidx.core.view.ViewCompat;

import org.jni_zero.internal.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.search_engines.AiModeButtonUiConfig;

@NullMarked
/** View for the composeplate layout which is shown below the fake search box on NTP. */
public class ComposeplateView extends LinearLayout {

    private @Nullable View mComposeplateButton;
    private @Nullable View mIncognitoButton;
    private @Nullable ImageView mComposeplateButtonIcon;

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
        mIncognitoButton = findViewById(R.id.incognito_button);
        mComposeplateButtonIcon = findViewById(R.id.composeplate_button_icon);
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

    private void setColorStateList(@Nullable ImageView view, ColorStateList colorStateList) {
        if (view == null) return;

        view.setImageTintList(colorStateList);
    }

    private void setTextStyle(@Nullable TextView view, @StyleRes int textStyleResId) {
        if (view == null) return;

        view.setTextAppearance(textStyleResId);
    }
}
