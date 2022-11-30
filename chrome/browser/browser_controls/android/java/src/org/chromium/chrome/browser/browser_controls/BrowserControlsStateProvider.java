// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

/**
 * An interface for retrieving and monitoring browser controls state.
 */
public interface BrowserControlsStateProvider {
    /**
     * An observer to be notified of browser controls changes
     */
    interface Observer {
        /**
         * Called whenever the controls' offset changes.
         *
         * @param topOffset The new value of the offset from the top of the top control in px.
         * @param topControlsMinHeightOffset The current top controls min-height in px. If the
         * min-height is changing with an animation, this will be a value between the old and the
         * new min-heights, which is the current visible min-height. Otherwise, this will be equal
         * to {@link #getTopControlsMinHeight()}.
         * @param bottomOffset The new value of the offset from the top of the bottom control in px.
         * @param bottomControlsMinHeightOffset The current bottom controls min-height in px. If the
         * min-height is changing with an animation, this will be a value between the old and the
         * new min-heights, which is the current visible min-height. Otherwise, this will be equal
         * to {@link #getBottomControlsMinHeight()}.
         * @param needsAnimate Whether the caller is driving an animation with further updates.
         */
        default void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {}

        /**
         * Called when the height of the bottom controls are changed.
         */
        default void onBottomControlsHeightChanged(
                int bottomControlsHeight, int bottomControlsMinHeight) {}

        /**
         * Called when the height of the top controls are changed.
         */
        default void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {}

        /**
         * Called whenever the controls' Android View visibility changes.
         */
        default void onAndroidVisibilityChanged(int visibility) {}
    }

    /**
     * Add an observer to be notified of browser controls events.
     * @param obs The observer to add.
     */
    void addObserver(Observer obs);

    /**
     * Remove a previously added observer.
     * @param obs The observer to remove.
     */
    void removeObserver(Observer obs);

    /**
     * @return The height of the top controls in pixels.
     */
    int getTopControlsHeight();

    /**
     * @return The minimum visible height top controls can have in pixels.
     */
    int getTopControlsMinHeight();

    /**
     * @return The offset of the controls from the top of the screen.
     */
    int getTopControlOffset();

    /**
     * @return The current top controls min-height. If the min-height is changing with an animation,
     * this will return a value between the old min-height and the new min-height, which is equal to
     * the current visible min-height. Otherwise, this will return the same value as
     * {@link #getTopControlsMinHeight()}.
     */
    int getTopControlsMinHeightOffset();

    /**
     * @return The height of the bottom controls in pixels.
     */
    int getBottomControlsHeight();

    /**
     * @return The minimum visible height bottom controls can have in pixels.
     */
    int getBottomControlsMinHeight();

    /**
     * @return The current bottom controls min-height. If the min-height is changing with an
     * animation, this will return a value between the old min-height and the new min-height, which
     * is equal to the current visible min-height. Otherwise, this will return the same value as
     * {@link #getBottomControlsMinHeight()}.
     */
    int getBottomControlsMinHeightOffset();

    /**
     * @return Whether or not the browser controls height changes should be animated.
     */
    boolean shouldAnimateBrowserControlsHeightChanges();

    /**
     * @return The offset of the controls from the bottom of the screen.
     */
    int getBottomControlOffset();

    /**
     * @return The ratio that the browser controls are off screen; this will be a number [0,1]
     *         where 1 is completely hidden and 0 is completely shown.
     */
    float getBrowserControlHiddenRatio();

    /**
     * @return The offset of the content from the top of the screen in px.
     */
    int getContentOffset();

    /**
     * @return The visible offset of the content from the top of the screen.
     */
    float getTopVisibleContentOffset();
}
