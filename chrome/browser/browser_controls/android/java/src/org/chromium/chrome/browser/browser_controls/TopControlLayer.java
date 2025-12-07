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
     * Called when Android controls visibility is changed.
     *
     * @see BrowserControlsStateProvider.Observer#onAndroidControlsVisibilityChanged(int)
     */
    default void onAndroidControlsVisibilityChanged(int visibility) {}

    /**
     * Interface method to receive OffsetTag updates. Unlike bottom controls, top controls does not
     * have layers that has additional height that draws beyond its allocated height.
     *
     * @param offsetTagsInfo The latest offset tags info. Null if the layer becomes not scrollable.
     */
    default void updateOffsetTag(@Nullable BrowserControlsOffsetTagsInfo offsetTagsInfo) {}

    /**
     * Interface method to receive top offset for the current layer. The layer will be able to fully
     * utilize this offset so it can correctly position itself on the screen.
     *
     * <p>The |layerYOffset| will be a generic position benchmark. This demonstrate how much the
     * layer has to move to be shown. When yOffset = 0, the top edge of the layer will be at the top
     * of Chrome's window. The larger the yOffset, the more the layer is positioned lower. (e.g. a
     * negative |yOffset| means the layer is partially invisible).
     *
     * <pre>
     * Example 1: Three layers in the stacker at resting position. L1(20 minH), L2(80), L3(50).
     * Top layer L1 has minHeight.
     *
     *    ┌──────────────┐
     *    │   20 minH    │       L1: yOffset = 0
     *    ├──────────────┤
     *    │              │
     *    │      80      │       L2: yOffset = 20
     *    │              │
     *    ├──────────────┤
     *    │      50      │       L3: yOffset = 100
     *    ├──────────────┤
     * </pre>
     *
     * <pre>
     * Example 2: Three layers in the stacker. L1(20 minH), L2(80), L3(50). L2 is partially scroll
     * off by 10px. The top of the layer 2 (10px) is drawn below layer 1.
     *
     *    ┌──────────────┐
     *    │   20 minH    │       L1: yOffset = 0      ┌──────────────┐
     *    ├──────────────┤                            │┄┄┄┄┄┄┄┄┄┄┄┄┄┄│
     *    │      80      │       L2: yOffset = 10     │ 70 (80 - 10) │
     *    │  (10 hidden) │                            │              │
     *    ├──────────────┤                            ├──────────────┤
     *    │      50      │       L3: yOffset = 90
     *    ├──────────────┤
     *
     * </pre>
     *
     * <pre>
     * Example 3: At resting, two layers, L2(80) and L3(50) are in the stacker at resting offset.
     * L1(20 minH) is being added to the stacker, and the height increase requires an animation.
     * The animation has started and L1 is 5px into the screen (15px hidden)
     *
     *    ┌──────────────┐
     *    │  (15 hidden) │
     *  ┄┄│   20 minH    │┄┄      L1: yOffset = -15
     *    ├──────────────┤
     *    │              │
     *    │      80      │        L2: yOffset = 5
     *    │              │
     *    ├──────────────┤
     *    │      50      │        L3: yOffset = 85
     *    ├──────────────┤
     *
     * </pre>
     *
     * @param layerYOffset The offset for the layer in the top controls.
     * @param reachRestingPosition Whether the layer is at its resting position, either fully shown
     *     or hidden. This is used when layer is showing / hiding, so it can change its visibility.
     */
    default void onBrowserControlsOffsetUpdate(int layerYOffset, boolean reachRestingPosition) {}

    /**
     * Interface method to dispatch a signal that the browser controls is expecting an animated
     * height update. This is dispatched in the same loop when a layer has updated their height, and
     * before any {@link #onBrowserControlsOffsetUpdate} is called.
     *
     * <p>This is useful for an layer that need to respond to animation e.g. by removing the offset
     * tags from the scene layer.
     *
     * @param latestYOffset The last Y offset of the current layer known by the stacker.
     */
    default void prepForHeightAdjustmentAnimation(int latestYOffset) {}
}
