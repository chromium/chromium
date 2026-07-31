// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab_bottom_sheet.WebViewResizingHelper.ResizeLock;

/**
 * Default resizing strategy that receives sheet state callbacks, calculates placeholder visibility,
 * and fully owns the resizing lock lifecycle on {@link WebViewResizingHelper}.
 */
@NullMarked
public class DefaultResizingStrategy implements ResizingStrategy {
    private final WebViewResizingHelper mHelper;
    private @Nullable ResizeLock mResizeLock;

    private boolean mIsResizing;
    private float mOffsetPx;
    private float mHalfHeightPx;
    private float mFullHeightPx;

    /**
     * Creates a default resizing strategy for managing resizing mode on WebViewResizingHelper.
     *
     * @param helper The {@link WebViewResizingHelper} to control resizing mode on.
     */
    public DefaultResizingStrategy(WebViewResizingHelper helper) {
        mHelper = helper;
    }

    @Override
    public void onSheetOffsetChanged(float offsetPx, float halfHeightPx, float fullHeightPx) {
        mOffsetPx = offsetPx;
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
        updateLockState();
    }

    private void updateLockState() {
        boolean shouldShow = mIsResizing && mOffsetPx > mHalfHeightPx && mOffsetPx < mFullHeightPx;

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
    }
}
