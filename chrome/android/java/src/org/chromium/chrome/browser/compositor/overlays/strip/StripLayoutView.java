// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.chrome.browser.layouts.components.VirtualView;

/**
 * {@link StripLayoutView} is used to keep track of the strip position and rendering information for
 * a particular item on the tab strip (e.g. tab, group indicator, etc.) so it can draw itself onto
 * the GL canvas.
 */
public interface StripLayoutView extends VirtualView {
    /**
     * @return The horizontal position of the view.
     */
    float getDrawX();

    /**
     * @param x The horizontal position of the view.
     */
    void setDrawX(float x);

    /**
     * @return The vertical position of the view.
     */
    float getDrawY();

    /**
     * @param y The vertical position of the view.
     */
    void setDrawY(float y);

    /**
     * @return The width of the view.
     */
    float getWidth();

    /**
     * @param width The width of the view.
     */
    void setWidth(float width);

    /**
     * @return The height of the view.
     */
    float getHeight();

    /**
     * @param height The height of the view.
     */
    void setHeight(float height);
}
