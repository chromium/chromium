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

import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.widget.ChromeImageView;

/** Toolbar for the bottom tab strip see {@link TabGroupUiCoordinator}. */
@NullMarked
public class TabGroupUiToolbarView extends FrameLayout {
    private ChromeImageView mNewTabButton;
    private ChromeImageView mShowGroupDialogButton;
    private ChromeImageView mFadingEdgeStart;
    private ChromeImageView mFadingEdgeEnd;
    private ViewGroup mContainerView;
    private ViewGroup mMainContent;
    private @Nullable FrameLayout mImageTilesContainer;
    private @Nullable Callback<Integer> mWidthPxCallback;

    public TabGroupUiToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mShowGroupDialogButton = findViewById(R.id.toolbar_show_group_dialog_button);
        mNewTabButton = findViewById(R.id.toolbar_new_tab_button);
        mFadingEdgeStart = findViewById(R.id.tab_strip_fading_edge_start);
        mFadingEdgeEnd = findViewById(R.id.tab_strip_fading_edge_end);
        mContainerView = findViewById(R.id.toolbar_container_view);
        mMainContent = findViewById(R.id.main_content);
        mImageTilesContainer = findViewById(R.id.toolbar_image_tiles_container);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (mWidthPxCallback != null) {
            mWidthPxCallback.onResult(getWidth());
        }
    }

    /* package */ void setShowGroupDialogButtonOnClickListener(OnClickListener listener) {
        mShowGroupDialogButton.setOnClickListener(listener);
    }

    /* package */ void setImageTilesContainerOnClickListener(OnClickListener listener) {
        if (mImageTilesContainer == null) return;

        // TODO(crbug.com/362280397): Possibly attach to a child view instead for ripple and instead
        // add a valid TouchDelegate.
        mImageTilesContainer.setOnClickListener(listener);
    }

    /* package */ void setNewTabButtonOnClickListener(OnClickListener listener) {
        mNewTabButton.setOnClickListener(listener);
    }

    /* package */ ViewGroup getViewContainer() {
        return mContainerView;
    }

    /* package */ void setMainContentVisibility(boolean isVisible) {
        if (mContainerView == null) {
            throw new IllegalStateException("Current Toolbar doesn't have a container view");
        }

        for (int i = 0; i < mContainerView.getChildCount(); i++) {
            View child = mContainerView.getChildAt(i);
            child.setVisibility(isVisible ? View.VISIBLE : View.INVISIBLE);
        }
    }

    /* package */ void setContentBackgroundColor(int color) {
        mMainContent.setBackgroundColor(color);
        if (mFadingEdgeStart == null || mFadingEdgeEnd == null) return;
        mFadingEdgeStart.setColorFilter(color, PorterDuff.Mode.SRC_IN);
        mFadingEdgeEnd.setColorFilter(color, PorterDuff.Mode.SRC_IN);
    }

    /* package */ void setTint(ColorStateList tint) {
        ImageViewCompat.setImageTintList(mShowGroupDialogButton, tint);
        ImageViewCompat.setImageTintList(mNewTabButton, tint);
    }

    /* package */ void setShowGroupDialogButtonVisible(boolean visible) {
        mShowGroupDialogButton.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /* package */ void setImageTilesContainerVisible(boolean visible) {
        if (mImageTilesContainer == null) return;

        mImageTilesContainer.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setWidthPxCallback(Callback<Integer> widthPxCallback) {
        mWidthPxCallback = widthPxCallback;
        mWidthPxCallback.onResult(getWidth());
    }
}
