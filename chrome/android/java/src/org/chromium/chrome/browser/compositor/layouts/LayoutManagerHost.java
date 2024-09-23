// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.graphics.RectF;
import android.view.View;

import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;

/**
 * This is the minimal interface of the host view from the layout side.
 * Any of these functions may be called on the GL thread.
 */
public interface LayoutManagerHost {
    /**
     * If set to true, the time it takes for ContentView to become ready will be
     * logged to the screen.
     */
    static final boolean LOG_CHROME_VIEW_SHOW_TIME = false;

    /** Requests a refresh of the visuals. */
    void requestRender();

    /**
     * Requests a refresh of the visuals.
     * @param onUpdateEffective Callback that will be called when there is a buffer swap for the
     *                          requested update. The rendered frame for this request won't be
     *                          visible until a buffer swap occurs. Note that there is no guarantee
     *                          the updated buffer is the one currently being displayed for pre-Q.
     */
    default void requestRender(Runnable onUpdateEffective) {}

    /**
     * @return The Android context of the host view.
     */
    Context getContext();

    /**
     * @see View#getWidth()
     * @return The width of the host view.
     */
    int getWidth();

    /**
     * @see View#getHeight()
     * @return The height of the host view.
     */
    int getHeight();

    /**
     * Get the window's viewport.
     * @param outRect The RectF object to write the result to.
     */
    void getWindowViewport(RectF outRect);

    /**
     * Get the visible viewport. This viewport accounts for the height of the browser controls.
     * @param outRect The RectF object to write the result to.
     */
    void getVisibleViewport(RectF outRect);

    /**
     * Get the viewport assuming the browser controls are completely shown.
     * @param outRect The RectF object to write the result to.
     */
    void getViewportFullControls(RectF outRect);

    /**
     * @return The associated {@link LayoutRenderHost} to be used from the GL Thread.
     */
    LayoutRenderHost getLayoutRenderHost();

    /**
     * Sets the visibility of the content overlays.
     * @param show True if the content overlays should be shown.
     * @param canBeFocusable Whether the host view can make itself focusable e.g. for accessibility.
     */
    void setContentOverlayVisibility(boolean show, boolean canBeFocusable);

    /**
     * @return The manager providing browser control state.
     */
    BrowserControlsManager getBrowserControlsManager();

    /**
     * @return The manager in charge of handling fullscreen changes.
     */
    FullscreenManager getFullscreenManager();

    /** Called when the currently visible content has been changed. */
    void onContentChanged();

    /**
     * Hides the the keyboard if it was opened for the ContentView.
     * @param postHideTask A task to run after the keyboard is done hiding and the view's
     *         layout has been updated.  If the keyboard was not shown, the task will run
     *         immediately.
     */
    void hideKeyboard(Runnable postHideTask);
}
