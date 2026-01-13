// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.chromium.chrome.browser.logo.LogoUtils.getGoogleLogoDrawable;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.Guideline;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.ntp_customization.R;

/** A layout for previewing a custom NTP background image along with a save and a cancel button. */
@NullMarked
public class UploadImagePreviewLayout extends ConstraintLayout {
    private ImageView mLogoView;
    private View mSearchBoxView;
    private Guideline mGuidelineTop;
    private int mDefaultLogoTopMarginPx;

    public UploadImagePreviewLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mLogoView = findViewById(R.id.default_search_engine_logo);
        mGuidelineTop = findViewById(R.id.guideline_top);
        mSearchBoxView = findViewById(R.id.search_box);
    }

    void setLogo(@Nullable Bitmap logoBitmap) {
        if (logoBitmap == null) {
            mLogoView.setImageDrawable(getGoogleLogoDrawable(getContext()));
        } else {
            mLogoView.setImageBitmap(logoBitmap);
        }
    }

    void setLogoVisibility(int visibility) {
        mLogoView.setVisibility(visibility);
    }

    void setLogoViewLayoutParams(int logoHeight, int logoTopMargin) {
        mDefaultLogoTopMarginPx = logoTopMargin;
        LogoUtils.setLogoViewLayoutParamsForDoodle(mLogoView, logoHeight, logoTopMargin);
    }

    void setTopInsets(int topInsetAndToolBarHeight) {
        int totalTopPadding = topInsetAndToolBarHeight + mDefaultLogoTopMarginPx;

        ConstraintLayout.LayoutParams params =
                (ConstraintLayout.LayoutParams) mGuidelineTop.getLayoutParams();
        params.guideBegin = totalTopPadding;
        mGuidelineTop.setLayoutParams(params);
    }

    /**
     * Applies padding to the sides and bottom of the layout. The top padding is preserved as it is
     * managed via the top guideline.
     *
     * @param insets A Rect where left, right, and bottom represent the required padding.
     */
    void setSideAndBottomInsets(Rect insets) {
        setPadding(insets.left, getPaddingTop(), insets.right, insets.bottom);
    }

    void setSearchBoxWidth(int width) {
        if (mSearchBoxView == null) return;

        mSearchBoxView.setVisibility(View.VISIBLE);
        ViewGroup.LayoutParams params = mSearchBoxView.getLayoutParams();
        params.width = width;
        mSearchBoxView.setLayoutParams(params);
    }

    void setSearchBoxHeight(int height) {
        if (mSearchBoxView == null) return;

        mSearchBoxView.setVisibility(View.VISIBLE);
        ViewGroup.LayoutParams params = mSearchBoxView.getLayoutParams();
        params.height = height;
        mSearchBoxView.setLayoutParams(params);
    }
}
