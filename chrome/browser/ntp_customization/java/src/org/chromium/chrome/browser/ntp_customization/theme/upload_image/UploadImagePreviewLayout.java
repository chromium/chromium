// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.logo.LogoUtils.getGoogleLogoDrawable;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
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
    private static final int GOOGLE_LOGO_TINT_COLOR = Color.WHITE;
    private ImageView mLogoView;
    private Guideline mGuidelineTop;
    private View mCancelButton;
    private @Nullable View mSearchBoxView;

    public UploadImagePreviewLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mLogoView = findViewById(R.id.default_search_engine_logo);
        mGuidelineTop = findViewById(R.id.guideline_top);
        mSearchBoxView = findViewById(R.id.search_box_container);
        mCancelButton = findViewById(R.id.cancel_button);

        View searchBox = findViewById(R.id.search_box);
        Drawable background = searchBox.getBackground();

        if (background instanceof GradientDrawable) {
            GradientDrawable shape = (GradientDrawable) background.mutate();
            shape.setColor(Color.WHITE);
        }
    }

    void setLogo(@Nullable Bitmap logoBitmap) {
        if (logoBitmap == null) {
            Drawable defaultGoogleLogoDrawable = getGoogleLogoDrawable(getContext());
            assumeNonNull(defaultGoogleLogoDrawable);
            Drawable tintedDrawable = defaultGoogleLogoDrawable.mutate();
            tintedDrawable.setTint(GOOGLE_LOGO_TINT_COLOR);
            mLogoView.setImageDrawable(tintedDrawable);
        } else {
            mLogoView.setImageBitmap(logoBitmap);
        }
    }

    void setLogoVisibility(int visibility) {
        mLogoView.setVisibility(visibility);
    }

    void setLogoViewLayoutParams(int logoHeight, int logoTopMargin) {
        LogoUtils.setLogoViewLayoutParamsForDoodle(mLogoView, logoHeight, logoTopMargin);
    }

    void setSearchBoxTopMargin(int marginPx) {
        if (mSearchBoxView == null) return;

        ViewGroup.LayoutParams params = mSearchBoxView.getLayoutParams();

        if (params instanceof MarginLayoutParams marginParams) {
            marginParams.topMargin = marginPx;
            mSearchBoxView.setLayoutParams(marginParams);
        }
    }

    void setTopGuidelineBegin(int topGuidelineBegin) {
        ConstraintLayout.LayoutParams params =
                (ConstraintLayout.LayoutParams) mGuidelineTop.getLayoutParams();
        params.guideBegin = topGuidelineBegin;
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

    void setButtonBottomMargin(int bottomMargin) {
        if (mCancelButton == null) return;

        ViewGroup.LayoutParams params = mCancelButton.getLayoutParams();
        if (params instanceof MarginLayoutParams marginParams) {
            if (marginParams.bottomMargin != bottomMargin) {
                marginParams.bottomMargin = bottomMargin;
                mCancelButton.setLayoutParams(marginParams);
            }
        }
    }

    void setSearchBoxWidth(int widthPx) {
        if (mSearchBoxView == null) return;

        mSearchBoxView.setVisibility(View.VISIBLE);
        ViewGroup.LayoutParams params = mSearchBoxView.getLayoutParams();
        params.width = widthPx;
        mSearchBoxView.setLayoutParams(params);
    }

    void setSearchBoxHeight(int heightPx) {
        if (mSearchBoxView == null) return;

        mSearchBoxView.setVisibility(View.VISIBLE);
        ViewGroup.LayoutParams params = mSearchBoxView.getLayoutParams();
        params.height = heightPx;
        mSearchBoxView.setLayoutParams(params);
    }
}
