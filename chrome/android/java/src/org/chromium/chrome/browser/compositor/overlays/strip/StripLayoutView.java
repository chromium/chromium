// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.graphics.RectF;
import android.util.FloatProperty;

import org.chromium.chrome.browser.layouts.components.VirtualView;

import java.util.List;

/**
 * {@link StripLayoutView} is used to keep track of the strip position and rendering information for
 * a particular item on the tab strip (e.g. tab, group indicator, etc.) so it can draw itself onto
 * the GL canvas.
 */
public abstract class StripLayoutView implements VirtualView {

    /** Handler for click actions on VirtualViews. */
    public interface StripLayoutViewOnClickHandler {
        /**
         * Handles the click action.
         *
         * @param time The time of the click action.
         * @param view View that received the click.
         */
        void onClick(long time, StripLayoutView view);
    }

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

    // Position variables.
    protected final RectF mDrawBounds = new RectF();
    private float mIdealX;
    private float mOffsetX;

    // Touch target bound variables.
    private float mTouchTargetInsetLeft;
    private float mTouchTargetInsetRight;
    private float mTouchTargetInsetTop;
    private float mTouchTargetInsetBottom;
    private final RectF mTouchTargetBounds = new RectF();

    // State variables.
    private boolean mVisible = true;
    private boolean mCollapsed;
    private boolean mIsIncognito;

    // A11y variables.
    private String mAccessibilityDescription = "";

    // Event handlers.
    private final StripLayoutViewOnClickHandler mOnClickHandler;

    /**
     * @param incognito The incognito state of the view.
     * @param clickHandler StripLayoutViewOnClickHandler for this view.
     */
    protected StripLayoutView(boolean incognito, StripLayoutViewOnClickHandler clickHandler) {
        mIsIncognito = incognito;
        mOnClickHandler = clickHandler;
    }

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
    public float getDrawX() {
        return mDrawBounds.left;
    }

    /**
     * @param x The horizontal position of the view.
     */
    public void setDrawX(float x) {
        mDrawBounds.right = x + mDrawBounds.width();
        mDrawBounds.left = x;
        // Update touch target bounds
        updateTouchTargetBounds(mTouchTargetBounds);
    }

    /**
     * @return The vertical position of the view.
     */
    public float getDrawY() {
        return mDrawBounds.top;
    }

    /**
     * @param y The vertical position of the view.
     */
    public void setDrawY(float y) {
        mDrawBounds.bottom = y + mDrawBounds.height();
        mDrawBounds.top = y;
        // Update touch target bounds
        updateTouchTargetBounds(mTouchTargetBounds);
    }

    /**
     * @return The width of the view.
     */
    public float getWidth() {
        return mDrawBounds.width();
    }

    /**
     * @param width The width of the view.
     */
    public void setWidth(float width) {
        mDrawBounds.right = mDrawBounds.left + width;
        // Update touch target bounds
        updateTouchTargetBounds(mTouchTargetBounds);
    }

    /**
     * @return The height of the view.
     */
    public float getHeight() {
        return mDrawBounds.height();
    }

    /**
     * @param height The height of the view.
     */
    public void setHeight(float height) {
        mDrawBounds.bottom = mDrawBounds.top + height;
        // Update touch target bounds
        updateTouchTargetBounds(mTouchTargetBounds);
    }

    /**
     * This is used to help calculate the view's position and is not used for rendering.
     *
     * @param offsetX The offset of the view (used for drag and drop, slide animating, etc).
     */
    public void setOffsetX(float offsetX) {
        mOffsetX = offsetX;
    }

    /**
     * This is used to help calculate the view's position and is not used for rendering.
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
        if (mVisible == visible) return;
        mVisible = visible;
        onVisibilityChanged(mVisible);
    }

    /**
     * Called if the visibility state has changed.
     *
     * @param newVisibility Whether or not this {@link StripLayoutView} should be drawn.
     */
    void onVisibilityChanged(boolean newVisibility) {}

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
     * @return The incognito state of the view.
     */
    public boolean isIncognito() {
        return mIsIncognito;
    }

    /**
     * @param state The incognito state of the view.
     */
    public void setIncognito(boolean state) {
        mIsIncognito = state;
    }

    /**
     * Get a list of virtual views for accessibility events.
     *
     * @param views A List to populate with virtual views.
     */
    public void getVirtualViews(List<VirtualView> views) {
        views.add(this);
    }

    /**
     * @param description A string describing the resource.
     */
    public void setAccessibilityDescription(String description) {
        mAccessibilityDescription = description;
    }

    /** {@link org.chromium.chrome.browser.layouts.components.VirtualView} Implementation */
    @Override
    public String getAccessibilityDescription() {
        return mAccessibilityDescription;
    }

    /**
     * @param x The x offset of the click.
     * @param y The y offset of the click.
     * @return Whether or not that gesture occurred inside of the touch target.
     */
    @Override
    public boolean checkClickedOrHovered(float x, float y) {
        return mTouchTargetBounds.contains(x, y);
    }

    /**
     * Get the view's touch target.
     *
     * @param outTarget to set to the touch target bounds.
     */
    @Override
    public void getTouchTarget(RectF outTarget) {
        outTarget.set(mTouchTargetBounds);
    }

    @Override
    public void handleClick(long time) {
        mOnClickHandler.onClick(time, this);
    }

    /**
     * @return Return cached touch target bounds.
     */
    protected RectF getTouchTargetBounds() {
        return mTouchTargetBounds;
    }

    /**
     * Apply insets to touch target bounds.
     *
     * @param left - Left inset to apply to touch target.
     * @param top - Top inset to apply to touch target.
     * @param right - Right inset to apply to touch target.
     * @param bottom - Bottom inset to apply to touch target.
     */
    protected void setTouchTargetInsets(Float left, Float top, Float right, Float bottom) {
        if (left != null) mTouchTargetInsetLeft = left;
        if (right != null) mTouchTargetInsetRight = right;
        if (top != null) mTouchTargetInsetTop = top;
        if (bottom != null) mTouchTargetInsetBottom = bottom;
        updateTouchTargetBounds(mTouchTargetBounds);
    }

    private void updateTouchTargetBounds(RectF outTarget) {
        outTarget.set(mDrawBounds);
        // Get the whole touchable region.
        outTarget.left += mTouchTargetInsetLeft;
        outTarget.right -= mTouchTargetInsetRight;
        outTarget.top += mTouchTargetInsetTop;
        outTarget.bottom -= mTouchTargetInsetBottom;
    }
}
