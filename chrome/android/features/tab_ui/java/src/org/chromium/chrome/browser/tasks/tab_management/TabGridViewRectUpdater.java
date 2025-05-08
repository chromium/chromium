// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Rect;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.widget.ViewRectProvider.ViewRectUpdateStrategy;
import org.chromium.ui.widget.ViewRectProvider.ViewRectUpdateStrategyFactory;

/**
 * A specific {@link ViewRectUpdateStrategy} designed for {@link TabGridView}, which handles the
 * view's scaling transformations.
 */
@NullMarked
public class TabGridViewRectUpdater implements ViewRectUpdateStrategy {
    private final int[] mCachedWindowCoordinates = new int[2];
    private final View mView;
    private final Rect mRect;
    private final Runnable mOnRectChanged;
    private int mCachedViewWidth = -1;
    private int mCachedViewHeight = -1;

    /**
     * Creates an instance of a {@link TabGridViewRectUpdater}. See {@link
     * ViewRectUpdateStrategyFactory#create(View, Rect, Runnable)}.
     *
     * @param view The {@link View} whose bounds will be tracked.
     * @param rect The {@link Rect} instance that will be updated by this class with the view's
     *     calculated bounds. This object is modified directly.
     * @param onRectChanged A {@link Runnable} that will be executed whenever the |rect| parameter
     *     is updated.
     */
    public TabGridViewRectUpdater(View view, Rect rect, Runnable onRectChanged) {
        mView = view;
        mRect = rect;
        mOnRectChanged = onRectChanged;
        mCachedWindowCoordinates[0] = -1;
        mCachedWindowCoordinates[1] = -1;
    }

    @Override
    public void refreshRectBounds(boolean forceRefresh) {
        int previousPositionX = mCachedWindowCoordinates[0];
        int previousPositionY = mCachedWindowCoordinates[1];
        int previousWidth = mCachedViewWidth;
        int previousHeight = mCachedViewHeight;
        mView.getLocationInWindow(mCachedWindowCoordinates);

        mCachedWindowCoordinates[0] = Math.max(mCachedWindowCoordinates[0], 0);
        mCachedWindowCoordinates[1] = Math.max(mCachedWindowCoordinates[1], 0);
        int scaledX = (int) (mView.getWidth() * mView.getScaleX());
        int scaledY = (int) (mView.getHeight() * mView.getScaleY());
        mCachedViewWidth = scaledX;
        mCachedViewHeight = scaledY;

        // Return if the window coordinates and view sizes haven't changed.
        if (mCachedWindowCoordinates[0] == previousPositionX
                && mCachedWindowCoordinates[1] == previousPositionY
                && mCachedViewWidth == previousWidth
                && mCachedViewHeight == previousHeight
                && !forceRefresh) {
            return;
        }

        mRect.left = mCachedWindowCoordinates[0];
        mRect.top = mCachedWindowCoordinates[1];
        mRect.right = mRect.left + scaledX;
        mRect.bottom = mRect.top + scaledY;

        // Make sure we still have a valid Rect after applying the inset.
        mRect.right = Math.max(mRect.left, mRect.right);
        mRect.bottom = Math.max(mRect.top, mRect.bottom);

        mRect.right = Math.min(mRect.right, mView.getRootView().getWidth());
        mRect.bottom = Math.min(mRect.bottom, mView.getRootView().getHeight());

        mOnRectChanged.run();
    }
}
