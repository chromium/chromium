// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowMetrics;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.dragdrop.DragStateTracker;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Map.Entry;

/** Implementation of the abstract class {@link ViewAndroidDelegate} for WebView. */
@Lifetime.WebView
public class AwViewAndroidDelegate extends ViewAndroidDelegate {
    /**
     * List of anchor views stored in the order in which they were acquired mapped to their
     * position.
     */
    private final Map<View, Position> mAnchorViews = new LinkedHashMap<>();

    private final AwContentsClient mContentsClient;
    private final AwScrollOffsetManager mScrollManager;
    private final WebContents mWebContents;

    // The amount the IME is currently imposing into the parent Window.
    private int mBottomImeInset;
    // The last bottom inset we calculated that should be applied to the visual viewport.
    private int mLastBottomInset;

    /** Represents the position of an anchor view. */
    @VisibleForTesting
    private static class Position {
        public final float mX;
        public final float mY;
        public final float mWidth;
        public final float mHeight;
        public final int mLeftMargin;
        public final int mTopMargin;

        public Position(
                float x, float y, float width, float height, int leftMargin, int topMargin) {
            mX = x;
            mY = y;
            mWidth = width;
            mHeight = height;
            mLeftMargin = leftMargin;
            mTopMargin = topMargin;
        }
    }

    @VisibleForTesting
    public AwViewAndroidDelegate(
            ViewGroup containerView,
            AwContentsClient contentsClient,
            AwScrollOffsetManager scrollManager,
            WebContents webContents) {
        super(containerView);
        mContentsClient = contentsClient;
        mScrollManager = scrollManager;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            mContainerView.setOnApplyWindowInsetsListener(this::onApplyWindowInsets);
        }
        mWebContents = webContents;
    }

    @Override
    public @Nullable DragStateTracker getDragStateTracker() {
        return getDragStateTrackerInternal();
    }

    @Override
    public View acquireView() {
        ViewGroup containerView = getContainerViewGroup();
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
        ViewGroup containerView = getContainerViewGroup();
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
                setViewPosition(
                        anchorView,
                        position.mX,
                        position.mY,
                        position.mWidth,
                        position.mHeight,
                        position.mLeftMargin,
                        position.mTopMargin);
            }
        }
    }

    @SuppressWarnings("deprecation") // AbsoluteLayout
    @Override
    public void setViewPosition(
            View anchorView,
            float x,
            float y,
            float width,
            float height,
            int leftMargin,
            int topMargin) {
        ViewGroup containerView = getContainerViewGroup();
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

    /**
     * @return The Visual Viewport bottom inset in pixels.
     */
    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public int getViewportInsetBottom() {
        if (!AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_REPORT_IME_INSETS)
                || Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return 0;
        }
        View containerView = getContainerView();
        if (!containerView.isAttachedToWindow()) {
            // View is not attached yet, no insets needed.
            return 0;
        }

        WindowMetrics wm =
                containerView
                        .getContext()
                        .getSystemService(WindowManager.class)
                        .getCurrentWindowMetrics();
        // These are the bounds of the current Window on the screen. These are absolute coordinates.
        Rect windowBounds = wm.getBounds();
        int[] pos = new int[2];
        containerView.getLocationInWindow(pos);
        // This represents the size and position of the WebView *relative* to the Window. These are
        // relative coordinates (to the top left corner of the Window).
        Rect viewRectInWindow =
                new Rect(
                        pos[0],
                        pos[1],
                        pos[0] + containerView.getWidth(),
                        pos[1] + containerView.getHeight());

        // This is the positive difference between the bottom of the WebView and the top of the IME.
        // For cases where the bottom of the WebView is higher than the top of the IME, return 0.
        // Otherwise, calculate the overlap by taking the bottom of the WebView (regardless of
        // whether this is obscured by the visible portion of the Window) and subtract the height of
        // the Window after deducting the IME overlap. This gives us the highest point in the
        // Window's coordinates that the IME reaches. In the case where the IME is not present
        // (mBottomImeInset is 0), this ensures that the visual viewport shows only the part of the
        // WebView that is visible in the Window.
        int result =
                Math.max(0, (viewRectInWindow.bottom - (windowBounds.height() - mBottomImeInset)));
        if (result != mLastBottomInset) {
            mLastBottomInset = result;
            // This does cause an extra round trip but allows us to recover in cases like bottom
            // sheets where the WebView is moved and the area that's obscured may have changed.
            // The alternative was attaching a scroll listener which probably would've been worse.
            if (mWebContents != null && mWebContents.getRenderWidgetHostView() != null) {
                mWebContents.getRenderWidgetHostView().onViewportInsetBottomChanged();
            }
        }

        return result;
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
        if (!AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_REPORT_IME_INSETS)) {
            return insets;
        }
        Insets imeInsets = insets.getInsets(WindowInsets.Type.ime());
        mBottomImeInset = imeInsets.bottom;
        if (mWebContents != null && mWebContents.getRenderWidgetHostView() != null) {
            mWebContents.getRenderWidgetHostView().onViewportInsetBottomChanged();
        }
        // Remove the bottom IME inset as we've consumed that one.
        return new WindowInsets.Builder(insets)
                .setInsets(
                        WindowInsets.Type.ime(),
                        Insets.of(imeInsets.left, imeInsets.top, imeInsets.right, 0))
                .build();
    }
}
