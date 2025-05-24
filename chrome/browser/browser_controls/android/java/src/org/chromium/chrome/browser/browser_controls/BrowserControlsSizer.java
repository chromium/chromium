// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;

/**
 * Allows for sizing the browser controls, as well as manipulating visibility and retrieving state.
 *
 * <p>Note that only top-level UI coordinator classes should change the browser controls size.
 */
@NullMarked
public interface BrowserControlsSizer extends BrowserControlsVisibilityManager {
    /** Sets the height of the bottom controls. */
    void setBottomControlsHeight(int bottomControlsHeight, int bottomControlsMinHeight);

    /**
     * Sets the additional height of the bottom controls. This is the extra distance on top of
     * the bottom controls height that represent visual effects that extend past the top of the
     * bottom controls.
     *
     * For example, the bottom tabgroup has a shadow. The height is the distance from the
     * bottom of the screen to the top of the tabgroup (not including the shadow) while the
     * additional height is the distance from the top of the tabgroup to the end of the shadow.
     * When changing the position of the tabgroup, only the height should be changed. The
     * additional height should only be changed when the shadow's height changes.
     */
    void setBottomControlsAdditionalHeight(int height);

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
     * @param newTopControlsMinHeight The new min height of the top controls after the position
     *     change.
     * @param newRendererTopControlsOffset The new offset of the top controls with respect to the
     *     top of the content. This will only be respected if the current page supports browser
     *     controls animation, i.e. isn't a native page.
     * @param newBottomControlsHeight The new height of the bottom controls after the position
     *     change.
     * @param newBottomControlsMinHeight The new min height of the bottom controls after the
     *     position change.
     * @param newRendererBottomControlsOffset The new offset of the bottom controls with respect to
     *     the bottom of the content. This will only be respected if the current page supports
     *     browser controls animation, i.e. isn't a native page.
     */
    void setControlsPosition(
            @ControlsPosition int controlsPosition,
            int newTopControlsHeight,
            int newTopControlsMinHeight,
            int newRendererTopControlsOffset,
            int newBottomControlsHeight,
            int newBottomControlsMinHeight,
            int newRendererBottomControlsOffset);
}
