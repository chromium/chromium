// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.Tab;

/**
 * {@link LayoutRenderHost} is the minimal interface the layouts need to know about its host to
 * update.
 */
public interface LayoutUpdateHost {
    /**
     * Requests a next update to refresh the transforms and changing properties. The update occurs
     * once a frame. This is requesting a new frame to be updated and rendered (no need to call
     * {@link LayoutRenderHost#requestRender()}).
     */
    void requestUpdate();

    /**
     * Requests a next update to refresh the transforms and changing properties. The update occurs
     * once a frame. This is requesting a new frame to be updated and rendered (no need to call
     * {@link LayoutRenderHost#requestRender()}).
     * @param onUpdateEffective Callback that will be called when there is a buffer swap for the
     *                          updated frame.
     */
    default void requestUpdate(Runnable onUpdateEffective) {}

    /**
     * Tells its host {@link android.view.View} that the hide will be an animation away. This is to
     * be called from a {@link Layout}.
     */
    void startHiding();

    /**
     * Tells its host {@link android.view.View} that the Layout has done all animation so the view
     * can hide. This is to be called from a {@link Layout}.
     */
    void doneHiding();

    /** Tells its host that the Layout is done it's preliminary showing animation. */
    void doneShowing();

    /**
     * @param layout The {@link Layout} being evaluated.
     * @return Whether the given {@link Layout} is being displayed.
     */
    boolean isActiveLayout(Layout layout);

    /**
     * Initializes {@link org.chromium.chrome.browser.compositor.layouts.components.LayoutTab} with
     * data accessible only from the {@link LayoutUpdateHost} such as data extracted out of a
     * {@link Tab}.
     *
     * @param tabId The id of the
     *              {@link org.chromium.chrome.browser.compositor.layouts.components.LayoutTab}
     *              to be initialized from a {@link Tab}.
     */
    void initLayoutTabFromHost(final int tabId);

    /**
     * Creates or recycles a {@Link LayoutTab}.
     *
     * @param id               The id of the reference tab in the
     *                         {@link org.chromium.chrome.browser.tabmodel.TabModel}.
     * @param incognito        Whether the new tab is incognito.
     * @param maxContentWidth  The maximum layout width this tab can be.  Negative numbers will use
     *                         the original content width.
     * @param maxContentHeight The maximum layout height this tab can be.  Negative numbers will use
     *                         the original content height.
     * @return                 The created or recycled {@link LayoutTab}.
     */
    LayoutTab createLayoutTab(
            int id, boolean incognito, float maxContentWidth, float maxContentHeight);

    /**
     * Notifies the host that the {@link LayoutTab} is no longer needed by the layout.
     *
     * @param id The id of the reference tab in the
     *           {@link org.chromium.chrome.browser.tabmodel.TabModel}.
     */
    void releaseTabLayout(int id);

    /**
     * Releases the cached title textures associated with the {@link LayoutTab}.
     *
     * @param tabId The id of the reference tab in the
     *              {@link org.chromium.chrome.browser.tabmodel.TabModel}.
     */
    void releaseResourcesForTab(int tabId);

    /**
     * Get the handler responsible for running the compositor's animations.
     * @return The {@link CompositorAnimationHandler}.
     */
    CompositorAnimationHandler getAnimationHandler();
}
