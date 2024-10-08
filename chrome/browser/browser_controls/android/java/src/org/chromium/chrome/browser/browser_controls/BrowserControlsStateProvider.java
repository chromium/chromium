// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

import org.chromium.cc.input.BrowserControlsOffsetTagsInfo;
import org.chromium.cc.input.BrowserControlsState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** An interface for retrieving and monitoring browser controls state. */
public interface BrowserControlsStateProvider {
    /**
     * The possible positions of the control container, which contains the browsing mode toolbar.
     */
    @IntDef({ControlsPosition.TOP, ControlsPosition.BOTTOM, ControlsPosition.NONE})
    @Retention(RetentionPolicy.SOURCE)
    @interface ControlsPosition {
        /** Controls are top-anchored. */
        int TOP = 0;

        /** Controls are bottom-anchored. */
        int BOTTOM = 1;

        /** Controls are not present, eg NoTouchActivity. */
        int NONE = 2;
    }

    /** An observer to be notified of browser controls changes */
    interface Observer {
        /**
         * Called whenever the controls' offset changes.
         *
         * @param topOffset The new value of the offset from the top of the top control in px.
         * @param topControlsMinHeightOffset The current top controls min-height in px. If the
         *     min-height is changing with an animation, this will be a value between the old and
         *     the new min-heights, which is the current visible min-height. Otherwise, this will be
         *     equal to {@link #getTopControlsMinHeight()}.
         * @param bottomOffset The new value of the offset from the top of the bottom control in px.
         * @param bottomControlsMinHeightOffset The current bottom controls min-height in px. If the
         *     min-height is changing with an animation, this will be a value between the old and
         *     the new min-heights, which is the current visible min-height. Otherwise, this will be
         *     equal to {@link #getBottomControlsMinHeight()}.
         * @param needsAnimate Whether the caller is driving an animation with further updates.
         */
        default void onControlsOffsetChanged(
                int topOffset,
                int topControlsMinHeightOffset,
                int bottomOffset,
                int bottomControlsMinHeightOffset,
                boolean needsAnimate,
                boolean isVisibilityForced) {}

        /** Called when the height of the bottom controls are changed. */
        default void onBottomControlsHeightChanged(
                int bottomControlsHeight, int bottomControlsMinHeight) {}

        /** Called when the height of the top controls are changed. */
        default void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {}

        /** Called when the visibility of the controls container changes. */
        default void onAndroidControlsVisibilityChanged(int visibility) {}

        /**
         * Called when the visibility constraints of the controls are changed. Visibility here
         * refers to if the browser is forcing the controls to be fully shown/hidden, which is not
         * the same as the visibility of the controls container, which is observed by
         * onAndroidControlsVisibilityChanged.
         *
         * @param oldOffsetTagsInfo the old OffsetTags for moving browser controls in viz.
         * @param offsetTagsInfo the new OffsetTags moving browser controls in viz. A null tag means
         *     the controls will no longer be moved by viz, which happens only when the browser is
         *     forcing the controls to be fully shown/hidden.
         * @param constraints the visibility constraints of the browser controls.
         */
        default void onControlsConstraintsChanged(
                BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
                BrowserControlsOffsetTagsInfo offsetTagsInfo,
                @BrowserControlsState int constraints) {}

        /** Called when the background color of the controls container changes. */
        default void onBottomControlsBackgroundColorChanged(@ColorInt int color) {}

        /** Called when the current position of the control container changes. */
        default void onControlsPositionChanged(@ControlsPosition int controlsPosition) {}
    }

    /**
     * Add an observer to be notified of browser controls events.
     *
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

    /** Returns the View visibility of the controls container. */
    int getAndroidControlsVisibility();

    /**
     * Get the current position of the controls, one of {@link ControlsPosition}. This value can
     * change at runtime; changes can be observed with {@link Observer#onControlsPositionChanged}
     */
    @ControlsPosition
    int getControlsPosition();
}
