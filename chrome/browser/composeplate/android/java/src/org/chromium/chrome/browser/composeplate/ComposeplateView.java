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

    private @Nullable View mComposeplateButton;
    private @Nullable View mIncognitoButton;

    public ComposeplateView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mComposeplateButton = findViewById(R.id.composeplate_button);
        mIncognitoButton = findViewById(R.id.incognito_button);
    }

    /**
     * Applies a white background with shadow or resets to the default background.
     *
     * @param apply Whether to apply or reset to the default background.
     */
    void applyWhiteBackgroundWithShadow(boolean apply) {
        if (apply) {
            setElevation(getResources().getDimensionPixelSize(R.dimen.ntp_search_box_elevation));
        } else {
            setElevation(0f);
        }

        Context context = getContext();
        if (mComposeplateButton != null) {
            ComposeplateUtils.applyWhiteBackgroundAndShadow(context, mComposeplateButton, apply);
        }

        if (mIncognitoButton != null) {
            ComposeplateUtils.applyWhiteBackgroundAndShadow(context, mIncognitoButton, apply);
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

    private void setColorStateList(@Nullable ImageView view, ColorStateList colorStateList) {
        if (view == null) return;

        view.setImageTintList(colorStateList);
    }

    private void setTextStyle(@Nullable TextView view, @StyleRes int textStyleResId) {
        if (view == null) return;

        view.setTextAppearance(textStyleResId);
    }
}
