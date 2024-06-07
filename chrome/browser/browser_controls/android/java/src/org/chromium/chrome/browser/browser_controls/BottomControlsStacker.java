// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import androidx.annotation.ColorInt;

import org.chromium.base.Log;

/**
 * Coordinator class for UI layers in the bottom browser controls. This class manages the relative
 * y-axis position for every registered bottom control elements, and their background colors.
 */
public class BottomControlsStacker implements BrowserControlsStateProvider.Observer {
    private static final String TAG = "BotControlsStacker";

    private final BrowserControlsSizer mBrowserControlsSizer;

    private int mTotalHeight;
    private int mTotalMinHeight;

    /**
     * Construct the coordination class that's used to position different UIs into the bottom
     * controls.
     *
     * @param browserControlsSizer {@link BrowserControlsSizer} to request browser controls changes.
     */
    public BottomControlsStacker(BrowserControlsSizer browserControlsSizer) {
        mBrowserControlsSizer = browserControlsSizer;
        mBrowserControlsSizer.addObserver(this);
    }

    /**
     * @return {@link BrowserControlsStateProvider} instance in the current Activity.
     */
    public BrowserControlsStateProvider getBrowserControls() {
        return mBrowserControlsSizer;
    }

    /**
     * Request update the bottom controls height. Internally, the call is routed to the inner {@link
     * BrowserControlsSizer}.
     *
     * @see BrowserControlsSizer#setBottomControlsHeight(int, int)
     */
    public void setBottomControlsHeight(int height, int minHeight) {
        mTotalHeight = height;
        mTotalMinHeight = minHeight;

        // Note: this is a transitional approach of directing all usage of mBrowserControlSizer for
        // the bottom browser controls.
        mBrowserControlsSizer.setBottomControlsHeight(height, minHeight);
    }

    /**
     * @see BrowserControlsSizer#setAnimateBrowserControlsHeightChanges(boolean)
     */
    public void setAnimateBrowserControlsHeightChanges(boolean animate) {
        mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(animate);
    }

    /**
     * @see BrowserControlsSizer#notifyBackgroundColor(int).
     */
    public void notifyBackgroundColor(@ColorInt int color) {
        // TODO(crbug.com/345488108): Handle #notifyBackgroundColor in this class.
        mBrowserControlsSizer.notifyBackgroundColor(color);
    }

    /** Destroy this instance and release the dependencies over the browser controls. */
    public void destroy() {
        mBrowserControlsSizer.removeObserver(this);
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        if (bottomControlsHeight != mTotalHeight || bottomControlsMinHeight != mTotalMinHeight) {
            // Use warning instead of assert, as there are still use cases that's referenced
            // from custom tabs.
            Log.w(TAG, "BottomControls height changed by another class.");
        }
    }
}
