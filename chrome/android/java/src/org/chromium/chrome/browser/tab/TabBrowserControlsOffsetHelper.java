// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.UserData;
import org.chromium.ui.base.WindowAndroid;

/**
 * Helper that coordinates the browser controls offsets from the perspective of a particular Tab.
 */
public class TabBrowserControlsOffsetHelper extends EmptyTabObserver implements UserData {
    @VisibleForTesting
    public static final Class<TabBrowserControlsOffsetHelper> USER_DATA_KEY =
            TabBrowserControlsOffsetHelper.class;

    private TabImpl mTab;

    private int mTopControlsOffset;
    private int mBottomControlsOffset;
    private int mContentOffset;
    private int mTopControlsMinHeightOffset;
    private int mBottomControlsMinHeightOffset;

    /** {@code true} if offset was changed by compositor. */
    private boolean mOffsetInitialized;

    /**
     * Get (or lazily create) the offset helper for a particular Tab.
     * @param tab The tab whose helper is being retrieved.
     * @return The offset helper for a given tab.
     */
    public static @NonNull TabBrowserControlsOffsetHelper get(Tab tab) {
        TabBrowserControlsOffsetHelper helper = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (helper == null) {
            helper = new TabBrowserControlsOffsetHelper(tab);
            tab.getUserDataHost().setUserData(USER_DATA_KEY, helper);
        }
        return helper;
    }

    private TabBrowserControlsOffsetHelper(Tab tab) {
        mTab = (TabImpl) tab;
        tab.addObserver(this);
    }

    @Override
    public void destroy() {
        mTab.removeObserver(this);
        mTab = null;
    }

    /**
     * Sets new top control, content, and min-height offset from renderer.
     *
     * @param topControlsOffset Top control offset.
     * @param contentOffset Content offset.
     * @param topControlsMinHeightOffset Current min-height offset for the top controls that may be
     *     changing as a result of an in-progress min-height change animation in the renderer.
     * @param bottomControlsOffset Bottom control offset.
     * @param bottomControlsMinHeightOffset Current min-height offset for the bottom controls that
     *     may be changing as a result of an in-progress min-height change animation in the
     *     renderer.
     */
    void setOffsets(
            int topControlsOffset,
            int contentOffset,
            int topControlsMinHeightOffset,
            int bottomControlsOffset,
            int bottomControlsMinHeightOffset) {
        if (mOffsetInitialized
                && topControlsOffset == mTopControlsOffset
                && mContentOffset == contentOffset
                && mTopControlsMinHeightOffset == topControlsMinHeightOffset
                && mBottomControlsOffset == bottomControlsOffset
                && mBottomControlsMinHeightOffset == bottomControlsMinHeightOffset) {
            return;
        }
        mTopControlsOffset = topControlsOffset;
        mContentOffset = contentOffset;
        mTopControlsMinHeightOffset = topControlsMinHeightOffset;
        mBottomControlsOffset = bottomControlsOffset;
        mBottomControlsMinHeightOffset = bottomControlsMinHeightOffset;
        notifyControlsOffsetChanged();
    }

    private void notifyControlsOffsetChanged() {
        mOffsetInitialized = true;
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) {
            observers
                    .next()
                    .onBrowserControlsOffsetChanged(
                            mTab,
                            mTopControlsOffset,
                            mBottomControlsOffset,
                            mContentOffset,
                            mTopControlsMinHeightOffset,
                            mBottomControlsMinHeightOffset);
        }
    }

    @Override
    public void onCrash(Tab tab) {
        super.onCrash(tab);
        mTopControlsOffset = 0;
        mBottomControlsOffset = 0;
        mContentOffset = 0;
        mOffsetInitialized = false;
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.
    }

    /** @return Top control offset */
    public int topControlsOffset() {
        return mTopControlsOffset;
    }

    /** @return Bottom control offset */
    public int bottomControlsOffset() {
        return mBottomControlsOffset;
    }

    /** @return Content offset */
    public int contentOffset() {
        return mContentOffset;
    }

    /** @return Top controls min-height offset */
    public int topControlsMinHeightOffset() {
        return mTopControlsMinHeightOffset;
    }

    /** @return Bottom controls min-height offset */
    public int bottomControlsMinHeightOffset() {
        return mBottomControlsMinHeightOffset;
    }

    /** @return Whether the control offset is initialized by compositor. */
    public boolean offsetInitialized() {
        return mOffsetInitialized;
    }
}
