// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import androidx.annotation.ColorInt;

/**
 * Allows for sizing the browser controls, as well as manipulating visibility and retrieving state.
 *
 * <p>Note that only top-level UI coordinator classes should change the browser controls size.
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
     * @deprecated Use {@link BottomControlsStacker#notifyBackgroundColor(int)}.
     */
    @Deprecated
    void notifyBackgroundColor(@ColorInt int color);

    /**
     * Changes the current position of the control container to either top or bottom, simultaneously
     * modifying the top and bottom controls heights. The provided new heights must accurately
     * re-allocate the height of the control container to the new position.
     *
     * @param controlsPosition The new position.
     * @param newTopControlsHeight The new height of the top controls after the position change.
     * @param newTopControlsHeight The new min height of the top controls after the position change.
     * @param newBottomControlsHeight The new height of the bottom controls after the position
     *     change.
     * @param newBottomControlsHeight The new min height of the bottom controls after the position
     *     change.
     */
    void setControlsPosition(
            @ControlsPosition int controlsPosition,
            int newTopControlsHeight,
            int newTopControlsMinHeight,
            int newBottomControlsHeight,
            int newBottomControlsMinHeight);
}
