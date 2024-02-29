// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.chrome.browser.layouts.components.VirtualView;

import java.util.List;

/**
 * {@link StripLayoutView} is used to keep track of the strip position and rendering information for
 * a particular item on the tab strip (e.g. tab, group indicator, etc.) so it can draw itself onto
 * the GL canvas.
 */
public abstract class StripLayoutView implements VirtualView {
    boolean mVisible = true;

    /**
     * @return The horizontal position of the view.
     */
    public abstract float getDrawX();

    /**
     * @param x The horizontal position of the view.
     */
    public abstract void setDrawX(float x);

    /**
     * @return The vertical position of the view.
     */
    public abstract float getDrawY();

    /**
     * @param y The vertical position of the view.
     */
    public abstract void setDrawY(float y);

    /**
     * @return The width of the view.
     */
    public abstract float getWidth();

    /**
     * @param width The width of the view.
     */
    public abstract void setWidth(float width);

    /**
     * @return The height of the view.
     */
    public abstract float getHeight();

    /**
     * @param height The height of the view.
     */
    public abstract void setHeight(float height);

    /**
     * @return Whether or not this {@link StripLayoutView} should be drawn.
     */
    public boolean isVisible() {
        return mVisible;
    }

    /**
     * @param visible Whether or not this {@link StripLayoutView} should be drawn.
     */
    public void setVisible(boolean visible) {
        mVisible = visible;
    }

    /**
     * Get a list of virtual views for accessibility events.
     *
     * @param views A List to populate with virtual views.
     */
    public void getVirtualViews(List<VirtualView> views) {
        views.add(this);
    }
}
