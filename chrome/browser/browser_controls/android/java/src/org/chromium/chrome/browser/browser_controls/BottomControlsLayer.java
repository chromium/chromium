// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;

/** Interface represented in the bottom controls stack. */
public interface BottomControlsLayer {
    /** Return the type of the layer. This should not change once the layer is created. */
    @LayerType
    int getType();

    /**
     * Return the behavior when browser controls are scrolling. Can change after the layer is
     * created; call {@link BottomControlsStacker#requestLayerUpdate} to trigger an update.
     */
    @LayerScrollBehavior
    int getScrollBehavior();

    /**
     * Return the current height of the layer. Can change after the layer is created; call {@link
     * BottomControlsStacker#requestLayerUpdate} to trigger an update. When animating a layer
     * transition with SHOWING/HIDING, the height should remain the same throughout the animation.
     */
    int getHeight();

    /**
     * Whether the layer is visible in the UI. Can change after the layer is created; call {@link
     * BottomControlsStacker#requestLayerUpdate} to trigger an update.
     */
    @LayerVisibility
    int getLayerVisibility();

    /**
     * Interface method to receive browser controls update. The goal is each layer will know exactly
     * where it is positioned in the browser controls system by using the given |layerYOffset|.
     *
     * <p>The |layerYOffset| will be a generic position benchmark. This demonstrate how much the
     * layer has to move to be shown. When yOffset = 0, the bottom edge of the layer will be at the
     * bottom of Chrome's window. The larger the yOffset, the more the layer is positioned lower
     * (e.g. a positive |yOffset| means a portion of the layer is drawn below the window.)
     *
     * <pre>
     * Example 1: Three layers in the stacker.
     *
     *                         bottomOffset = 0
     *  |--------------|
     *  |     50       |       L1: yOffset = -100
     *  |--------------|
     *  |   80 minH    |       L2: yOffset = -20
     *  |              |
     *  |--------------|
     *  |   20 minH    |       L3: yOffset = 0
     *  L______________|
     * </pre>
     *
     * <pre>
     * Example 2: Three layers in the stacker, two layers have minHeight, so they don't scroll off;
     * top layer scrolled 10px and it's partially covered by L2.
     *
     *                         bottomOffset = 10
     *  |--------------|
     *  |  40 (50-10)  |       L1: yOffset = -100 + 10 = -90
     *  |--------------|
     *  |   80 minH    |       L2: yOffset = -20
     *  |              |
     *  |--------------|
     *  |   20 minH    |       L3: yOffset = 0
     *  L______________|
     * </pre>
     *
     * <pre>
     * Example 3: L1 and L3 are in the stacker, with L2 being added in between and causing L3 to
     * change from 0 minH to 20 minH. This transition has an animation, causing minHeight to animate
     * from 0 to 100 (combined new minHeight for L2+L3). This case is only relevant for animation,
     * no scrolling is involved.
     *
     * At this stage in the animation, the browser controls have a visible minHeight of 40, a total
     * visible height of 90 (visible minHeight + L1 height), and a hidden height of 60.
     *
     * L3 in this case is fully hidden under the screen, and should have a yOffset 60 (the same as
     * hidden height since it's the bottom layer); however, since L3's full height is 20, we cap
     * the yOffset so the layer does not have to update its position when it's fully hidden.
     *
     * (=== bottom of Chrome's window ===)
     *                             bottomOffset = 60   bottomControlsMinHeightOffset = 40
     *      |--------------|
     *      |     50       |       L1: yOffset = -100 + 60 = -40
     *      |--------------|
     *      |   80 minH    |       L2: yOffset = -20 + 60 = 40
     * ==== | (40 hidden)  |  ====
     *      |--------------|
     *      |   20 minH    |       L3: yOffset = 20 = min(20, 60) - 20 as the layer height
     *      L______________|
     * </pre>
     *
     * @param layerYOffset The yOffset for the layer's position in the bottom controls
     * @see BrowserControlsStateProvider.Observer#onControlsOffsetChanged
     */
    default void onBrowserControlsOffsetUpdate(int layerYOffset) {}
}
