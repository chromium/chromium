// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.graphics.RectF;
import android.util.FloatProperty;

import org.chromium.base.MathUtils;
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
    private float mOffsetY;

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
    private boolean mIsForegrounded;
    private boolean mIsDraggedOffStrip;

    // A11y variables.
    private String mAccessibilityDescription = "";

    // Event handlers.
    private final StripLayoutViewOnClickHandler mOnClickHandler;

    // Tab group share properties.
    private boolean mShowNotificationBubble;

    /**
     * @param incognito The incognito state of the view.
     * @param clickHandler StripLayoutViewOnClickHandler for this view.
     */
    protected StripLayoutView(boolean incognito, StripLayoutViewOnClickHandler clickHandler) {
        mIsIncognito = incognito;
        mOnClickHandler = clickHandler;
    }

    /**
     * Sets the ideal position, in an infinitely long strip, of this view. This is used to help
     * calculate the view's position and is not used for rendering.
     */
    public void setIdealX(float x) {
        mIdealX = x;
    }

    /**
     * Returns the ideal position, in an infinitely long strip, of this view. This is used to help
     * calculate the view's position and is not used for rendering.
     */
    public float getIdealX() {
        return mIdealX;
    }

    /** Returns the horizontal position of the view. */
    public float getDrawX() {
        return mDrawBounds.left;
    }

    /** Sets the horizontal position of the view. */
    public void setDrawX(float x) {
        mDrawBounds.right = x + mDrawBounds.width();
        mDrawBounds.left = x;
        // Update touch target bounds
        updateTouchTargetBounds(mTouchTargetBounds);
    }

    /** Returns the vertical position of the view. */
    public float getDrawY() {
        return mDrawBounds.top;
    }

    /** Sets the vertical position of the view. */
    public void setDrawY(float y) {
        mDrawBounds.bottom = y + mDrawBounds.height();
        mDrawBounds.top = y;
        // Update touch target bounds
        updateTouchTargetBounds(mTouchTargetBounds);
    }

    /**
     * Represents how much this view's width should be counted when positioning views in the stack.
     * The view has a full width weight of 1 when it is fully translated up onto the strip. This
     * linearly decreases to 0 as it is translated down off the strip. The view visually has the
     * same width, but the other views will smoothly slide out of the way to make/take room.
     *
     * @return The weight from 0 to 1 that the width of this view should have on the stack.
     */
    public float getWidthWeight() {
        return MathUtils.clamp(1.f - (getDrawY() / getHeight()), 0.f, 1.f);
    }

    /** Returns the width of the view. */
    public float getWidth() {
        return mDrawBounds.width();
    }

    /** Sets the width of the view. */
    public void setWidth(float width) {
        mDrawBounds.right = mDrawBounds.left + width;
        // Update touch target bounds
        updateTouchTargetBounds(mTouchTargetBounds);
    }

    /** Returns the height of the view. */
    public float getHeight() {
        return mDrawBounds.height();
    }

    /** Sets the height of the view. */
    public void setHeight(float height) {
        mDrawBounds.bottom = mDrawBounds.top + height;
        // Update touch target bounds
        updateTouchTargetBounds(mTouchTargetBounds);
    }

    /**
     * Sets the signed distance the drawX will be away from the view's idealX. This horizontal
     * offset is used for drag and drop, slide animating, etc.
     */
    public void setOffsetX(float offsetX) {
        mOffsetX = offsetX;
    }

    /**
     * Returns signed distance the drawX will be away from the view's idealX. This horizontal offset
     * is used for drag and drop, slide animating, etc.
     */
    public float getOffsetX() {
        return mOffsetX;
    }

    /**
     * Sets the signed distance the drawY will be away from the view's ideal position. This vertical
     * offset is used for open/close animations.
     */
    public void setOffsetY(float offsetY) {
        mOffsetY = offsetY;
    }

    /**
     * Returns the vertical offset of the view. This vertical offset is used for open/close
     * animations.
     */
    public float getOffsetY() {
        return mOffsetY;
    }

    /** Returns whether or not this {@link StripLayoutView} should be drawn. */
    public boolean isVisible() {
        return mVisible;
    }

    /** Sets whether or not this {@link StripLayoutView} should be drawn. */
    public void setVisible(boolean visible) {
        if (mVisible == visible) return;
        mVisible = visible;
        onVisibilityChanged(mVisible);
    }

    /**
     * Sets whether the notification bubble is shown on the view. When set to true, a notification
     * dot will appear on the view to indicate that this group has been updated by other members in
     * the shared group.
     *
     * @param showBubble Whether to show the bubble.
     */
    public void setNotificationBubbleShown(boolean showBubble) {
        mShowNotificationBubble = showBubble;
    }

    /**
     * Checks whether the notification bubble is shown.
     *
     * @return Whether the notification bubble is shown.
     */
    public boolean getNotificationBubbleShown() {
        return mShowNotificationBubble;
    }

    /**
     * Called if the visibility state has changed.
     *
     * @param newVisibility Whether or not this {@link StripLayoutView} should be drawn.
     */
    void onVisibilityChanged(boolean newVisibility) {}

    /** Returns whether or not this {@link StripLayoutView} is collapsed. */
    public boolean isCollapsed() {
        return mCollapsed;
    }

    /** Sets whether or not this {@link StripLayoutView} is collapsed. */
    public void setCollapsed(boolean collapsed) {
        mCollapsed = collapsed;
    }

    /** Returns the incognito state of the view. */
    public boolean isIncognito() {
        return mIsIncognito;
    }

    /** Sets the incognito state of the view. */
    public void setIncognito(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    /** Returns {@code true} if the view is foregrounded for reorder, {@code false} otherwise. */
    public boolean isForegrounded() {
        return mIsForegrounded;
    }

    /** Sets whether or not the given view should be foregrounded for reorder. */
    public void setIsForegrounded(boolean isForegrounded) {
        mIsForegrounded = isForegrounded;
    }

    /** Sets whether or not the view is dragged off the strip and should be hidden. */
    public void setIsDraggedOffStrip(boolean isDraggedOffStrip) {
        mIsDraggedOffStrip = isDraggedOffStrip;
    }

    /** Returns whether or not the tab is dragged off the strip and should be hidden. */
    public boolean isDraggedOffStrip() {
        return mIsDraggedOffStrip;
    }

    /**
     * Populates the given list with virtual views for accessibility events.
     *
     * @param views A List to populate with virtual views.
     */
    public void getVirtualViews(List<VirtualView> views) {
        views.add(this);
    }

    /** Sets a string describing the resource. */
    public void setAccessibilityDescription(String description) {
        mAccessibilityDescription = description;
    }

    /** {@link org.chromium.chrome.browser.layouts.components.VirtualView} Implementation */
    @Override
    public String getAccessibilityDescription() {
        return mAccessibilityDescription;
    }

    @Override
    public boolean checkClickedOrHovered(float x, float y) {
        return mTouchTargetBounds.contains(x, y);
    }

    @Override
    public void getTouchTarget(RectF outTarget) {
        outTarget.set(mTouchTargetBounds);
    }

    @Override
    public void handleClick(long time) {
        mOnClickHandler.onClick(time, this);
    }

    /** Returns cached touch target bounds. */
    protected RectF getTouchTargetBounds() {
        return mTouchTargetBounds;
    }

    /**
     * Apply insets to touch target bounds.
     *
     * @param left Left inset to apply to touch target.
     * @param top Top inset to apply to touch target.
     * @param right Right inset to apply to touch target.
     * @param bottom Bottom inset to apply to touch target.
     */
    public void setTouchTargetInsets(Float left, Float top, Float right, Float bottom) {
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
