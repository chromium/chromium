// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import androidx.annotation.ColorInt;

/**
 * Allows for sizing the browser controls, as well as manipulating visibility and retrieving state.
 */
public interface BrowserControlsSizer extends BrowserControlsVisibilityManager {
    /** Sets the height of the bottom controls. */
    void setBottomControlsHeight(int bottomControlsHeight, int bottomControlsMinHeight);

    /** Sets the height of the top controls. */
    void setTopControlsHeight(int topControlsHeight, int topControlsMinHeight);

    /**
     * Sets whether the changes to the browser controls heights should be animated.
     *
     * @param animateBrowserControlsHeightChanges True if the height changes should be animated.
     */
    void setAnimateBrowserControlsHeightChanges(boolean animateBrowserControlsHeightChanges);

    /**
     * Notifies the {@BrowserControlsSizer} of the background color that's been set to the browser
     * controls view.
     *
     * @param color The color used for the background of the browser controls view.
     */
    void notifyBackgroundColor(@ColorInt int color);
}
