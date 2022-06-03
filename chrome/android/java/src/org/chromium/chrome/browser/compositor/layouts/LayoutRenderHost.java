// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.res.Resources;
import android.graphics.Rect;

import org.chromium.ui.resources.ResourceManager;

/**
 * {@link LayoutRenderHost} is the minimal interface the layouts need to know about its host to
 * render.
 */
public interface LayoutRenderHost {
    /**
     * Request layout and draw.
     */
    void requestRender();

    /**
     * Indicates that we are about to draw and final layout changes should be made.
     */
    void onCompositorLayout();

    /**
     * Indicates that a previously rendered frame has been swapped to the OS.
     */
    void didSwapFrame(int pendingFrameCount);

    /**
     * Indicates that the compositor swapped buffers.
     * @param swappedCurrentSize Whether the swapped buffer size is the same as the current one.
     */
    default void didSwapBuffers(boolean swappedCurrentSize) {}

    /**
     * Indicates that the rendering surface has just been created.
     */
    void onSurfaceCreated();

    /**
     * Indicates that the rendering surface has been resized.
     */
    void onSurfaceResized(int width, int height);

    /**
     * Pushes a debug rectangle that will be drawn.
     *
     * @param rect  The rect to be drawn.
     * @param color The color of the rect.
     */
    void pushDebugRect(Rect rect, int color);

    /**
     * Loads the persistent textures if they are not loaded already.
     */
    void loadPersitentTextureDataIfNeeded();

    /**
     * @return The background color of the toolbar.
     */
    int getBrowserControlsBackgroundColor(Resources resources);

    /**
     * @return The {@link ResourceManager}.
     */
    ResourceManager getResourceManager();

    /**
     * Called when something has changed in the Compositor rendered view system.
     */
    void invalidateAccessibilityProvider();
}
