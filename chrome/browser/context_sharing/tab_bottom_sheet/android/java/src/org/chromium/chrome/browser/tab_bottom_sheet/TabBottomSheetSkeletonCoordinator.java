// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.ui.util.CommonOnLayoutChangeListeners;

/** Coordinator managing the skeleton loader placeholder view for Tab Bottom Sheet. */
@NullMarked
public class TabBottomSheetSkeletonCoordinator implements ResizingPlaceholderCoordinator {
    private final TabBottomSheetSkeletonView mSkeletonView;
    private final @Px int mDefaultPeekHeightPx;
    private final @Px int mBufferPx;
    private final View.OnLayoutChangeListener mLayoutChangeListener;
    private @Px int mLastVisibleHeight;

    /**
     * Constructs a new {@link TabBottomSheetSkeletonCoordinator}.
     *
     * @param context The Android context.
     * @param backgroundColor The background color of the placeholder.
     * @param placeholderElemColor The element color of the skeleton bars and pill box.
     * @param headerIconResId The drawable resource ID for the header icon.
     */
    public TabBottomSheetSkeletonCoordinator(
            Context context,
            @ColorInt int backgroundColor,
            @ColorInt int placeholderElemColor,
            @DrawableRes int headerIconResId) {
        mSkeletonView =
                (TabBottomSheetSkeletonView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.tab_bottom_sheet_skeleton_view, null);
        mSkeletonView.setVisibility(View.INVISIBLE);
        mSkeletonView.setHeaderIcon(headerIconResId);
        mSkeletonView.setPlaceholderElemColor(placeholderElemColor);
        mSkeletonView.setBackground(new ColorDrawable(backgroundColor));

        mDefaultPeekHeightPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.tab_bottom_sheet_peek_height_total);
        mBufferPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.tab_bottom_sheet_skeleton_buffer);

        mLayoutChangeListener =
                CommonOnLayoutChangeListeners.createSizeChangedListener(
                        () -> {
                            if (mLastVisibleHeight > 0) {
                                updateAlpha();
                            }
                        });
        mSkeletonView.addOnLayoutChangeListener(mLayoutChangeListener);
    }

    @Override
    public View getView() {
        return mSkeletonView;
    }

    @Override
    public void updateVisibleHeight(@Px int visibleHeight) {
        mLastVisibleHeight = visibleHeight;
        ViewGroup.LayoutParams params = mSkeletonView.getLayoutParams();
        if (params != null && params.height != visibleHeight) {
            params.height = visibleHeight;
            mSkeletonView.setLayoutParams(params);
        }
        updateAlpha();
    }

    private void updateAlpha() {
        int headerHeight = mSkeletonView.getHeaderHeight();
        int bottomGroupHeight = mSkeletonView.getBottomGroupHeight();
        if (headerHeight == 0 || bottomGroupHeight == 0) {
            return;
        }
        int collisionHeight = headerHeight + bottomGroupHeight + mBufferPx;
        float alpha;
        if (mLastVisibleHeight <= mDefaultPeekHeightPx) {
            alpha = 0f;
        } else if (mLastVisibleHeight >= collisionHeight
                || collisionHeight <= mDefaultPeekHeightPx) {
            alpha = 1f;
        } else {
            alpha =
                    (float) (mLastVisibleHeight - mDefaultPeekHeightPx)
                            / (collisionHeight - mDefaultPeekHeightPx);
        }
        mSkeletonView.setBottomGroupAlpha(alpha);
    }

    @Override
    public void setIsResizing(boolean isResizing) {
        mSkeletonView.setIsResizing(isResizing);
    }

    @Override
    public void destroy() {
        mSkeletonView.setIsResizing(false);
        mSkeletonView.removeOnLayoutChangeListener(mLayoutChangeListener);
    }

    @Px
    int getCollisionHeightForTesting() {
        return mSkeletonView.getHeaderHeight() + mSkeletonView.getBottomGroupHeight() + mBufferPx;
    }

    @Px
    int getDefaultPeekHeightForTesting() {
        return mDefaultPeekHeightPx;
    }

    TabBottomSheetSkeletonView getSkeletonViewForTesting() {
        return mSkeletonView;
    }
}
