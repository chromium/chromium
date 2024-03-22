// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.graphics.RectF;

import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.ui.resources.ResourceManager;

/**
 * Interface that give access the active layout. This is useful to isolate the renderer of all the
 * layout logic. Called from the GL thread.
 */
public interface LayoutProvider {
    /**
     * @return The layout to be rendered. The caller may not keep a reference of that value
     * internally because the value may change without notice.
     */
    Layout getActiveLayout();

    /**
     * @param rect RectF instance to be used to store the result and return. If null, it uses a new
     *             RectF instance.
     */
    void getViewportPixel(RectF rect);

    /**
     * @return The manager of browser controls.
     */
    BrowserControlsManager getBrowserControlsManager();

    /**
     * Build a {@link SceneLayer} for the active layout if it hasn't already been built, and update
     * it and return it.
     *
     * @param tabContentManager A tab content manager.
     * @param resourceManager   A resource manager.
     * @param browserControlsManager A browser controls manager.
     * @return                  A {@link SceneLayer} that represents the content for this
     *                          {@link Layout}.
     */
    SceneLayer getUpdatedActiveSceneLayer(
            TabContentManager tabContentManager,
            ResourceManager resourceManager,
            BrowserControlsManager browserControlsManager);
}
