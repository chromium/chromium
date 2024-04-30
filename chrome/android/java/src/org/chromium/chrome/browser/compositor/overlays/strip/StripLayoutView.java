// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.util.FloatProperty;

import org.chromium.chrome.browser.layouts.components.VirtualView;

import java.util.List;

/**
 * {@link StripLayoutView} is used to keep track of the strip position and rendering information for
 * a particular item on the tab strip (e.g. tab, group indicator, etc.) so it can draw itself onto
 * the GL canvas.
 */
public abstract class StripLayoutView implements VirtualView {

    /** A property for animations to use for changing the drawX of the view. */
    public static final FloatProperty<StripLayoutView> DRAW_X =
            new FloatProperty<>("drawX") {
                @Override
                public void setValue(StripLayoutView object, float value) {
                    object.setDrawX(value);
                }

                @Override
                public Float get(StripLayoutView object) {
                    return object.getDrawX();
                }
            };

    /** A property for animations to use for changing the X offset of the view. */
    public static final FloatProperty<StripLayoutView> X_OFFSET =
            new FloatProperty<>("offsetX") {
                @Override
                public void setValue(StripLayoutView object, float value) {
                    object.setOffsetX(value);
                }

                @Override
                public Float get(StripLayoutView object) {
                    return object.getOffsetX();
                }
            };

    private float mIdealX;
    private float mOffsetX;
    private boolean mVisible = true;
    private boolean mCollapsed;

    /**
     * This is used to help calculate the view's position and is not used for rendering.
     *
     * @param x The ideal position, in an infinitely long strip, of this view.
     */
    public void setIdealX(float x) {
        mIdealX = x;
    }

    /**
     * This is used to help calculate the view's position and is not used for rendering.
     *
     * @return The ideal position, in an infinitely long strip, of this view.
     */
    public float getIdealX() {
        return mIdealX;
    }

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
     * This is used to help calculate the view's position and is not used for rendering.
     *
     * @param offsetX The offset of the view (used for drag and drop, slide animating, etc).
     */
    public void setOffsetX(float offsetX) {
        mOffsetX = offsetX;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     *
     * @return The offset of the view (used for drag and drop, slide animating, etc).
     */
    public float getOffsetX() {
        return mOffsetX;
    }

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
     * @return Whether or not this {@link StripLayoutView} is collapsed.
     */
    public boolean isCollapsed() {
        return mCollapsed;
    }

    /**
     * @param collapsed Whether or not this {@link StripLayoutView} is collapsed.
     */
    public void setCollapsed(boolean collapsed) {
        mCollapsed = collapsed;
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
