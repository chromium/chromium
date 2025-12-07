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

import org.jni_zero.internal.Nullable;

import org.chromium.build.annotations.NullMarked;

@NullMarked
/** View for the composeplate layout which is shown below the fake search box on NTP. */
public class ComposeplateView extends LinearLayout {
    private final int mPaddingForShadowPx;

    private @Nullable View mComposeplateButton;
    private @Nullable View mIncognitoButton;
    private @Nullable ImageView mLensButton;
    private @Nullable ImageView mVoiceSearchButton;

    public ComposeplateView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mPaddingForShadowPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.composeplate_view_button_padding_for_shadow);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mComposeplateButton = findViewById(R.id.composeplate_button);
        mIncognitoButton = findViewById(R.id.incognito_button);
        mLensButton = findViewById(R.id.lens_camera_button);
        mVoiceSearchButton = findViewById(R.id.voice_search_button);
    }

    /**
     * Applies a white background with shadow or resets to the default background.
     *
     * @param apply Whether to apply or reset to the default background.
     */
    void applyWhiteBackgroundWithShadow(boolean apply) {
        if (apply) {
            // Adds paddings on each sides of the view to prevent shadow from being cut.
            setPadding(
                    mPaddingForShadowPx, getPaddingTop(), mPaddingForShadowPx, getPaddingBottom());
        } else {
            setPadding(0, getPaddingTop(), 0, getPaddingBottom());
        }

        Context context = getContext();
        if (mComposeplateButton != null) {
            ComposeplateUtils.applyWhiteBackgroundAndShadow(context, mComposeplateButton, apply);
        }

        if (mIncognitoButton != null) {
            ComposeplateUtils.applyWhiteBackgroundAndShadow(context, mIncognitoButton, apply);
        }

        if (mLensButton != null) {
            ComposeplateUtils.applyWhiteBackgroundAndShadow(context, mLensButton, apply);
        }

        if (mVoiceSearchButton != null) {
            ComposeplateUtils.applyWhiteBackgroundAndShadow(context, mVoiceSearchButton, apply);
        }
    }

    /** Sets the ColorStateList to tint the icons on the buttons. */
    void setColorStateList(@Nullable ColorStateList colorStateList) {
        if (colorStateList == null) return;

        if (mComposeplateButton != null) {
            setColorStateList(
                    mComposeplateButton.findViewById(R.id.composeplate_button_icon),
                    colorStateList);
        }

        // TODO (https://crbug.com/421944848): Cleans up this class when cleaning up
        //  composeplate_view_layout(_V2).xml.
        if (mIncognitoButton != null) {
            if (mIncognitoButton instanceof ImageView incognitoButtonImageView) {
                incognitoButtonImageView.setImageTintList(colorStateList);
            } else {
                setColorStateList(
                        mIncognitoButton.findViewById(R.id.incognito_button_icon), colorStateList);
            }
        }

        setColorStateList(mLensButton, colorStateList);
        setColorStateList(mVoiceSearchButton, colorStateList);
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

    private void setColorStateList(@Nullable ImageView view, ColorStateList colorStateList) {
        if (view == null) return;

        view.setImageTintList(colorStateList);
    }

    private void setTextStyle(@Nullable TextView view, @StyleRes int textStyleResId) {
        if (view == null) return;

        view.setTextAppearance(textStyleResId);
    }
}
