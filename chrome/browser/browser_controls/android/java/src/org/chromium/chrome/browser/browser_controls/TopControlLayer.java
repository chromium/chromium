// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.ScrollBehavior;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;

/** Interface definition for any View registering itself as a top control. */
@NullMarked
public interface TopControlLayer {
    /** Return the type of the layer. This should not change once the layer is created. */
    @TopControlType
    int getTopControlType();

    /** Return the current height of the layer. */
    int getTopControlHeight();

    /** Whether the layer is visible in the UI. */
    @TopControlVisibility
    int getTopControlVisibility();

    /**
     * Return true if the layer should contribute to the total height, which a view may not if it
     * draws over other views, for example the progress bar. Returns true by default since most of
     * the Top Controls will always contribute to the total height.
     */
    default boolean contributesToTotalHeight() {
        return true;
    }

    /**
     * Returns the scroll behavior of the layer. By default, all layers are scrollable.
     *
     * @return The {@link ScrollBehavior} of the layer.
     */
    default @ScrollBehavior int getScrollBehavior() {
        return ScrollBehavior.DEFAULT_SCROLLABLE;
    }

    /**
     * Called whenever {@link BrowserControlsManager} provides a signal that the height of the top
     * controls are changed.
     *
     * @param topControlsHeight The new height of the top controls.
     * @param topControlsMinHeight The new minimum height of the top controls.
     */
    default void onTopControlLayerHeightChanged(int topControlsHeight, int topControlsMinHeight) {}

    /**
     * Interface method to receive OffsetTag updates. Unlike bottom controls, top controls does not
     * have layers that has additional height that draws beyond its allocated height.
     *
     * @param offsetTagsInfo The latest offset tags info. Null if the layer becomes not scrollable.
     */
    default void updateOffsetTag(@Nullable BrowserControlsOffsetTagsInfo offsetTagsInfo) {}
}
