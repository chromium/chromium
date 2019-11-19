// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.ui.base.ViewAndroidDelegate;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Map.Entry;

/**
 * Implementation of the abstract class {@link ViewAndroidDelegate} for WebView.
 */
public class AwViewAndroidDelegate extends ViewAndroidDelegate {
    /** Used for logging. */
    private static final String TAG = "AwVAD";

    /**
     * List of anchor views stored in the order in which they were acquired mapped
     * to their position.
     */
    private final Map<View, Position> mAnchorViews = new LinkedHashMap<>();

    private final AwContentsClient mContentsClient;
    private final AwScrollOffsetManager mScrollManager;

    /**
     * Represents the position of an anchor view.
     */
    @VisibleForTesting
    private static class Position {
        public final float mX;
        public final float mY;
        public final float mWidth;
        public final float mHeight;
        public final int mLeftMargin;
        public final int mTopMargin;

        public Position(float x, float y, float width, float height, int leftMargin,
                int topMargin) {
            mX = x;
            mY = y;
            mWidth = width;
            mHeight = height;
            mLeftMargin = leftMargin;
            mTopMargin = topMargin;
        }
    }

    @VisibleForTesting
    public AwViewAndroidDelegate(ViewGroup containerView, AwContentsClient contentsClient,
            AwScrollOffsetManager scrollManager) {
        super(containerView);
        mContentsClient = contentsClient;
        mScrollManager = scrollManager;
    }

    @Override
    public View acquireView() {
        ViewGroup containerView = getContainerView();
        if (containerView == null) return null;
        View anchorView = new View(containerView.getContext());
        containerView.addView(anchorView);
        // |mAnchorViews| will be updated with the right view position in |setViewPosition|.
        mAnchorViews.put(anchorView, null);
        return anchorView;
    }

    @Override
    public void removeView(View anchorView) {
        mAnchorViews.remove(anchorView);
        ViewGroup containerView = getContainerView();
        if (containerView != null) {
            containerView.removeView(anchorView);
        }
    }

    @Override
    public void updateAnchorViews(ViewGroup oldContainerView) {
        // Transfer existing anchor views from the old to the new container view.
        for (Entry<View, Position> entry : mAnchorViews.entrySet()) {
            View anchorView = entry.getKey();
            Position position = entry.getValue();
            if (oldContainerView != null) {
                oldContainerView.removeView(anchorView);
            }
            mContainerView.addView(anchorView);
            if (position != null) {
                setViewPosition(anchorView, position.mX, position.mY, position.mWidth,
                        position.mHeight, position.mLeftMargin, position.mTopMargin);
            }
        }
    }

    @SuppressWarnings("deprecation") // AbsoluteLayout
    @Override
    public void setViewPosition(View anchorView, float x, float y, float width, float height,
            int leftMargin, int topMargin) {
        ViewGroup containerView = getContainerView();
        if (!mAnchorViews.containsKey(anchorView) || containerView == null) return;

        mAnchorViews.put(anchorView, new Position(x, y, width, height, leftMargin, topMargin));

        if (containerView instanceof FrameLayout) {
            super.setViewPosition(anchorView, x, y, width, height, leftMargin, topMargin);
            return;
        }
        // This fixes the offset due to a difference in scrolling model of WebView vs. Chrome.
        leftMargin += mScrollManager.getScrollX();
        topMargin += mScrollManager.getScrollY();

        android.widget.AbsoluteLayout.LayoutParams lp =
                new android.widget.AbsoluteLayout.LayoutParams(
                        Math.round(width), Math.round(height), leftMargin, topMargin);
        anchorView.setLayoutParams(lp);
    }

    @Override
    public void onBackgroundColorChanged(int color) {
        mContentsClient.onBackgroundColorChanged(color);
    }
}
