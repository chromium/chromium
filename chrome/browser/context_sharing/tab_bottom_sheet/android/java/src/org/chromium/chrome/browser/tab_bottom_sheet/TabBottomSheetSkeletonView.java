// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;

/**
 * Custom passive view displaying a skeleton loader for the Tab Bottom Sheet during WebContents
 * resize. Comprises a static peek header at the top and a skeleton group at the bottom, managing
 * default title fallback and parent clipping restoration.
 */
@NullMarked
public class TabBottomSheetSkeletonView extends FrameLayout {
    private @Nullable FrameLayout mHeaderContainer;
    private @Nullable ViewGroup mBottomGroup;

    /**
     * Constructor for inflating via XML with attributes.
     *
     * @param context The Context the view is running in.
     * @param attrs The attributes of the XML tag that is inflating the view.
     */
    public TabBottomSheetSkeletonView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs, 0);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mHeaderContainer = findViewById(R.id.skeleton_header_container);
        mBottomGroup = findViewById(R.id.skeleton_bottom_group);
        TextView titleView = findViewById(R.id.peek_title);
        if (titleView != null && TextUtils.isEmpty(titleView.getText())) {
            titleView.setText(R.string.tab_bottom_sheet_resizing_view_text);
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (getParent() instanceof ViewGroup parent) {
            parent.setClipChildren(false);
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (getParent() instanceof ViewGroup parent) {
            parent.setClipChildren(true);
        }
    }

    /**
     * Sets the alpha of the bottom skeleton group.
     *
     * @param alpha The target alpha between 0.0f and 1.0f.
     */
    public void setBottomGroupAlpha(float alpha) {
        if (mBottomGroup != null && mBottomGroup.getAlpha() != alpha) {
            mBottomGroup.setAlpha(alpha);
        }
    }

    /** Returns the measured/laid-out height of the header container. */
    public @Px int getHeaderHeight() {
        return mHeaderContainer != null ? mHeaderContainer.getHeight() : 0;
    }

    /** Returns the measured/laid-out height of the bottom skeleton group. */
    public @Px int getBottomGroupHeight() {
        return mBottomGroup != null ? mBottomGroup.getHeight() : 0;
    }

    @Nullable
    ViewGroup getBottomGroupForTesting() {
        return mBottomGroup;
    }

    @Nullable
    FrameLayout getHeaderContainerForTesting() {
        return mHeaderContainer;
    }
}
