// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab_bottom_sheet.WebViewResizingHelper.ResizeLock;

/**
 * Resizing strategy that considers drag direction for showing the placeholder:
 *
 * <ul>
 *   <li>Upward drags (0 -> 100%): Only show placeholder when offset is between 70% and 100%.
 *   <li>Downward drags (100 -> 0% or direction change downward): Immediately show and hold the
 *       placeholder for the remainder of the gesture until finger release.
 * </ul>
 */
@NullMarked
public class DragDirectionResizingStrategy implements ResizingStrategy {
    private static final float INVALID_OFFSET = -1f;
    private static final float PEEK_EPSILON_PX = 1.0f;

    private final WebViewResizingHelper mHelper;
    private @Nullable ResizeLock mResizeLock;

    private boolean mIsResizing;
    private float mOffsetPx = INVALID_OFFSET;
    private float mPreviousOffsetPx = INVALID_OFFSET;
    private float mPeekHeightPx;
    private float mHalfHeightPx;
    private float mFullHeightPx;
    private boolean mHasDraggedDownward;

    /**
     * Creates a drag direction resizing strategy for managing resizing mode.
     *
     * @param helper The {@link WebViewResizingHelper} to control resizing mode on.
     */
    public DragDirectionResizingStrategy(WebViewResizingHelper helper) {
        mHelper = helper;
    }

    @Override
    public void onSheetOffsetChanged(
            float offsetPx, float peekHeightPx, float halfHeightPx, float fullHeightPx) {
        if (mPreviousOffsetPx != INVALID_OFFSET && offsetPx < mPreviousOffsetPx) {
            mHasDraggedDownward = true;
        }
        mPreviousOffsetPx = offsetPx;
        mOffsetPx = offsetPx;
        mPeekHeightPx = peekHeightPx;
        mHalfHeightPx = halfHeightPx;
        mFullHeightPx = fullHeightPx;
        updateLockState();
        if (mResizeLock != null) {
            mHelper.updatePlaceholderHeight((int) offsetPx);
        }
    }

    @Override
    public void onSheetResizingStatusChanged(boolean isResizing) {
        mIsResizing = isResizing;
        if (!isResizing) {
            mPreviousOffsetPx = INVALID_OFFSET;
            mHasDraggedDownward = false;
            if (mResizeLock != null) {
                mResizeLock.unlock();
                mResizeLock = null;
            }
            return;
        }
        updateLockState();
    }

    private void updateLockState() {
        if (!mIsResizing || mOffsetPx == INVALID_OFFSET) {
            if (mResizeLock != null) {
                mResizeLock.unlock();
                mResizeLock = null;
            }
            return;
        }

        if (mOffsetPx <= mPeekHeightPx + PEEK_EPSILON_PX) {
            mHasDraggedDownward = false;
            mPreviousOffsetPx = INVALID_OFFSET;
            if (mResizeLock != null) {
                mResizeLock.unlock();
                mResizeLock = null;
            }
            return;
        }

        boolean shouldShow;
        if (mHasDraggedDownward) {
            // Once a downward drag has occurred, keep the placeholder visible until release or
            // peek.
            shouldShow = true;
        } else {
            // Pure upward drag: only show placeholder between half height (70%) and full height.
            shouldShow = mOffsetPx > mHalfHeightPx && mOffsetPx < mFullHeightPx;
        }

        if (shouldShow) {
            if (mResizeLock == null) {
                mResizeLock = mHelper.requestResize();
            }
        } else if (mResizeLock != null) {
            mResizeLock.unlock();
            mResizeLock = null;
        }
    }

    @Override
    public void destroy() {
        if (mResizeLock != null) {
            mResizeLock.unlock();
            mResizeLock = null;
        }
        mHasDraggedDownward = false;
        mPreviousOffsetPx = INVALID_OFFSET;
        mOffsetPx = INVALID_OFFSET;
    }
}
