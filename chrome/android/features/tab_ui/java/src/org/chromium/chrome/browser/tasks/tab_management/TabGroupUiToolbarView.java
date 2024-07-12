// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.PorterDuff;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.annotation.ColorRes;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.widget.ChromeImageView;

/** Toolbar for the bottom tab strip see {@link TabGroupUiCoordinator}. */
public class TabGroupUiToolbarView extends FrameLayout {
    private ChromeImageView mRightButton;
    private ChromeImageView mLeftButton;
    private ChromeImageView mFadingEdgeStart;
    private ChromeImageView mFadingEdgeEnd;
    private ViewGroup mContainerView;
    private LinearLayout mMainContent;

    public TabGroupUiToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mLeftButton = findViewById(R.id.toolbar_left_button);
        mRightButton = findViewById(R.id.toolbar_right_button);
        mFadingEdgeStart = findViewById(R.id.tab_strip_fading_edge_start);
        mFadingEdgeEnd = findViewById(R.id.tab_strip_fading_edge_end);
        mContainerView = findViewById(R.id.toolbar_container_view);
        mMainContent = findViewById(R.id.main_content);
    }

    void setLeftButtonOnClickListener(OnClickListener listener) {
        mLeftButton.setOnClickListener(listener);
    }

    void setRightButtonOnClickListener(OnClickListener listener) {
        mRightButton.setOnClickListener(listener);
    }

    ViewGroup getViewContainer() {
        return mContainerView;
    }

    void setMainContentVisibility(boolean isVisible) {
        if (mContainerView == null) {
            throw new IllegalStateException("Current Toolbar doesn't have a container view");
        }

        for (int i = 0; i < ((ViewGroup) mContainerView).getChildCount(); i++) {
            View child = ((ViewGroup) mContainerView).getChildAt(i);
            child.setVisibility(isVisible ? View.VISIBLE : View.INVISIBLE);
        }
    }

    void setIsIncognito(boolean isIncognito) {
        @ColorRes
        int tintListRes =
                isIncognito
                        ? R.color.default_icon_color_light_tint_list
                        : R.color.default_icon_color_tint_list;
        ColorStateList tintList = ContextCompat.getColorStateList(getContext(), tintListRes);
        setTint(tintList);
    }

    void setContentBackgroundColor(int color) {
        mMainContent.setBackgroundColor(color);
        if (mFadingEdgeStart == null || mFadingEdgeEnd == null) return;
        mFadingEdgeStart.setColorFilter(color, PorterDuff.Mode.SRC_IN);
        mFadingEdgeEnd.setColorFilter(color, PorterDuff.Mode.SRC_IN);
    }

    void setTint(ColorStateList tint) {
        ImageViewCompat.setImageTintList(mLeftButton, tint);
        ImageViewCompat.setImageTintList(mRightButton, tint);
    }

    void setBackgroundColorTint(int color) {
        DrawableCompat.setTint(getBackground(), color);
    }

    /** Setup the drawable in the left button. */
    void setLeftButtonDrawableId(int drawableId) {
        mLeftButton.setImageResource(drawableId);
    }

    /** Set the content description of the left button. */
    void setLeftButtonContentDescription(String string) {
        mLeftButton.setContentDescription(string);
    }

    /** Set the content description of the right button. */
    void setRightButtonContentDescription(String string) {
        mRightButton.setContentDescription(string);
    }
}
