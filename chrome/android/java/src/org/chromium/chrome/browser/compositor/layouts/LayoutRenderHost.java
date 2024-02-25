// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import org.chromium.ui.resources.ResourceManager;

/**
 * {@link LayoutRenderHost} is the minimal interface the layouts need to know about its host to
 * render.
 */
public interface LayoutRenderHost {
    /** Request layout and draw. */
    void requestRender();

    /** Indicates that we are about to draw and final layout changes should be made. */
    void onCompositorLayout();

    /** Indicates that a previously rendered frame has been swapped to the OS. */
    void didSwapFrame(int pendingFrameCount);

    /**
     * Indicates that the compositor swapped buffers.
     * @param swappedCurrentSize Whether the swapped buffer size is the same as the current one.
     * @param framesUntilHideBackground The number of buffer swaps needed until the incoming surface
     *         has a frame ready. Zero if no incoming surface or if the incoming surface is ready.
     */
    default void didSwapBuffers(boolean swappedCurrentSize, int framesUntilHideBackground) {}

    /** Indicates that the rendering surface has been resized. */
    void onSurfaceResized(int width, int height);

    /**
     * @return The {@link ResourceManager}.
     */
    ResourceManager getResourceManager();

    /** Called when something has changed in the Compositor rendered view system. */
    void invalidateAccessibilityProvider();
}
