// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone.stack;

import android.content.Context;
import android.content.res.Resources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.animation.FloatProperty;
import org.chromium.chrome.browser.compositor.layouts.Layout.Orientation;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;

/**
 * StackTab is used to keep track of a thumbnail's bitmap and position and to
 * draw itself onto the GL canvas at the desired Y Offset.
 * @VisibleForTesting
 */
public class StackTab {
    // Cached values from values/dimens.xml
    public static float sStackedTabVisibleSize; // stacked_tab_visible_size
    public static float sStackBufferWidth; // stack_buffer_width
    public static float sStackBufferHeight; // stack_buffer_height

    // Positioner selector
    private float mXInStackInfluence = 1.0f;
    private float mYInStackInfluence = 1.0f;

    // In stack positioner
    private float mScrollOffset;
    private float mXInStackOffset;
    private float mYInStackOffset;

    // Out of stack positioner
    private float mXOutOfStack;
    private float mYOutOfStack;

    // Values that get animated
    private float mAlpha = 1.0f;
    private float mScale = 1.0f;
    private float mDiscardAmount; // This might alter position, rotation and alpha

    // Discard states
    private float mDiscardOriginX;
    private float mDiscardOriginY;
    private boolean mDiscardFromClick;

    // The index of the tab in the stack
    private int mIndex;

    // True if the tab is currently being removed (while animating).
    protected boolean mDying;

    // The visibility sorting value is used to determine the importance of the tab for
    // texture allocation. It is computed from the area and its position in the stack.
    // Larger values will have more priority for acquiring texture. Negative values "often"
    // means that the tab is not visible at all (but there are no guaranty and it's fine).
    private float mCachedVisibleArea; // Intermediate value
    private float mCachedIndexDistance; // Intermediate value
    private float mCacheStackVisibility = 1.0f; // Intermediate value
    private long mVisiblitySortingValue; // Sorting value based on visible area.
    private int mOrderSortingValue; // Sorting value based on distance to selection.

    private LayoutTab mLayoutTab;

    public static final FloatProperty<StackTab> DISCARD_AMOUNT =
            new FloatProperty<StackTab>("DISCARD_AMOUNT") {
                @Override
                public void setValue(StackTab layoutTab, float v) {
                    layoutTab.setDiscardAmount(v);
                }

                @Override
                public Float get(StackTab layoutTab) {
                    return layoutTab.getDiscardAmount();
                }
            };

    public static final FloatProperty<StackTab> SCALE = new FloatProperty<StackTab>("SCALE") {
        @Override
        public void setValue(StackTab layoutTab, float v) {
            layoutTab.setScale(v);
        }

        @Override
        public Float get(StackTab layoutTab) {
            return layoutTab.getScale();
        }
    };

    public static final FloatProperty<StackTab> SCROLL_OFFSET =
            new FloatProperty<StackTab>("SCROLL_OFFSET") {
                @Override
                public void setValue(StackTab layoutTab, float v) {
                    layoutTab.setScrollOffset(v);
                }

                @Override
                public Float get(StackTab layoutTab) {
                    return layoutTab.getScrollOffset();
                }
            };

    public static final FloatProperty<StackTab> X_IN_STACK_INFLUENCE =
            new FloatProperty<StackTab>("X_IN_STACK_INFLUENCE") {
                @Override
                public void setValue(StackTab layoutTab, float v) {
                    layoutTab.setXInStackInfluence(v);
                }

                @Override
                public Float get(StackTab layoutTab) {
                    return layoutTab.getXInStackInfluence();
                }
            };

    public static final FloatProperty<StackTab> X_IN_STACK_OFFSET =
            new FloatProperty<StackTab>("X_IN_STACK_OFFSET") {
                @Override
                public void setValue(StackTab layoutTab, float v) {
                    layoutTab.setXInStackOffset(v);
                }

                @Override
                public Float get(StackTab layoutTab) {
                    return layoutTab.getXInStackOffset();
                }
            };

    public static final FloatProperty<StackTab> Y_IN_STACK_INFLUENCE =
            new FloatProperty<StackTab>("Y_IN_STACK_INFLUENCE") {
                @Override
                public void setValue(StackTab layoutTab, float v) {
                    layoutTab.setYInStackInfluence(v);
                }

                @Override
                public Float get(StackTab layoutTab) {
                    return layoutTab.getYInStackInfluence();
                }
            };

    public static final FloatProperty<StackTab> Y_IN_STACK_OFFSET =
            new FloatProperty<StackTab>("Y_IN_STACK_OFFSET") {
                @Override
                public void setValue(StackTab layoutTab, float v) {
                    layoutTab.setYInStackOffset(v);
                }

                @Override
                public Float get(StackTab layoutTab) {
                    return layoutTab.getYInStackOffset();
                }
            };

    /**
     * @param tab The tab this instance is supposed to draw.
     */
    public StackTab(LayoutTab tab) {
        mLayoutTab = tab;
    }

    /**
     * @param index The new index in the stack layout.
     */
    public void setNewIndex(int index) {
        mIndex = index;
    }

    /**
     * @return The index in the stack layout.
     */
    public int getIndex() {
        return mIndex;
    }

    /**
     * @return The {@link LayoutTab} this instance is supposed to draw.
     */
    public LayoutTab getLayoutTab() {
        return mLayoutTab;
    }

    /**
     * Set the {@link LayoutTab} this instance is supposed to draw.
     */
    public void setLayoutTab(LayoutTab tab) {
        mLayoutTab = tab;
    }

    /**
     * @return The id of the tab, same as the id from the Tab in TabModel.
     */
    public int getId() {
        return mLayoutTab.getId();
    }

    /**
     * @param y The vertical translation to be applied after the placement in the stack.
     */
    public void setYInStackOffset(float y) {
        mYInStackOffset = y;
    }

    /**
     * @return The vertical translation applied after the placement in the stack.
     */
    public float getYInStackOffset() {
        return mYInStackOffset;
    }

    /**
     * @param x The horizontal translation to be applied after the placement in the stack.
     */
    public void setXInStackOffset(float x) {
        mXInStackOffset = x;
    }

    /**
     * @return The horizontal translation applied after the placement in the stack.
     */
    public float getXInStackOffset() {
        return mXInStackOffset;
    }

    /**
     * @param y The vertical absolute position when out of stack.
     */
    public void setYOutOfStack(float y) {
        mYOutOfStack = y;
    }

    /**
     * @return The vertical absolute position when out of stack.
     */
    public float getYOutOfStack() {
        return mYOutOfStack;
    }

    /**
     * @param x The horizontal absolute position when out of stack.
     */
    public void setXOutOfStack(float x) {
        mXOutOfStack = x;
    }

    /**
     * @return The horizontal absolute position when out of stack.
     */
    public float getXOutOfStack() {
        return mXOutOfStack;
    }

    /**
     * Set the transparency value for all of the tab (the contents,
     * border, etc...).  For components that allow specifying
     * their own alpha values, it will use the min of these two fields.
     *
     * @param f The transparency value for the tab.
     */
    public void setAlpha(float f) {
        mAlpha = f;
    }

    /**
     * @return The transparency value for all of the tab components.
     */
    public float getAlpha() {
        return mAlpha;
    }

    /**
     * @param xInStackInfluence The horizontal blend value between instack
     *                          and out of stack pacement [0 .. 1].
     */
    public void setXInStackInfluence(float xInStackInfluence) {
        mXInStackInfluence = xInStackInfluence;
    }

    /**
     * @return The horizontal blend value between instack and out of stack pacement [0 .. 1].
     */
    public float getXInStackInfluence() {
        return mXInStackInfluence;
    }

    /**
     * @param yInStackInfluence The vertical blend value between instack
     *                          and out of stack pacement [0 .. 1].
     */
    public void setYInStackInfluence(float yInStackInfluence) {
        mYInStackInfluence = yInStackInfluence;
    }

    /**
     * @return The verical blend value between instack and out of stack pacement [0 .. 1].
     */
    public float getYInStackInfluence() {
        return mYInStackInfluence;
    }

    /**
     * @param scale The scale to apply to the tab, compared to the parent.
     */
    public void setScale(float scale) {
        mScale = scale;
    }

    /**
     * @return The scale to apply to the tab, compared to the parent.
     */
    public float getScale() {
        return mScale;
    }

    /**
     * @param offset The offset of the tab along the scrolling direction in scroll space.
     */
    public void setScrollOffset(float offset) {
        mScrollOffset = offset;
    }

    /**
     * @return The offset of the tab along the scrolling direction in scroll space.
     */
    public float getScrollOffset() {
        return mScrollOffset;
    }

    /**
     * @param amount The amount of discard displacement. 0 is no discard. Negative is discard
     *               on the left. Positive is discard on the right.
     */
    public void setDiscardAmount(float amount) {
        mDiscardAmount = amount;
    }

    /**
     * @param deltaAmount The amount of delta discard to be added to the current discard amount.
     */
    public void addToDiscardAmount(float deltaAmount) {
        mDiscardAmount += deltaAmount;
    }

    /**
     * @return The amount of discard displacement. 0 is no discard. Negative is discard
     *         on the left. Positive is discard on the right.
     */
    public float getDiscardAmount() {
        return mDiscardAmount;
    }

    /**
     * @param x The x coordinate in tab space of where the discard transforms should originate.
     */
    public void setDiscardOriginX(float x) {
        mDiscardOriginX = x;
    }

    /**
     * @param y The y coordinate in tab space of where the discard transforms should originate.
     */
    public void setDiscardOriginY(float y) {
        mDiscardOriginY = y;
    }

    /**
     * @return The x coordinate in tab space of where the discard transforms should originate.
     */
    public float getDiscardOriginX() {
        return mDiscardOriginX;
    }

    /**
     * @return The y coordinate in tab space of where the discard transforms should originate.
     */
    public float getDiscardOriginY() {
        return mDiscardOriginY;
    }

    /**
     * @param fromClick Whether or not this discard was from a click event.
     */
    public void setDiscardFromClick(boolean fromClick) {
        mDiscardFromClick = fromClick;
    }

    /**
     * @return Whether or not this discard was from a click event.
     */
    public boolean getDiscardFromClick() {
        return mDiscardFromClick;
    }

    /**
     * @param dying True if the Tab/ContentView will be destroyed, and we are still animating its
     *              visible representation.
     */
    public void setDying(boolean dying) {
        mDying = dying;
    }

    /**
     * @return True if the Tab/ContentView is destroyed, but we are still animating its
     *         visible representation.
     */
    public boolean isDying() {
        return mDying;
    }

    /**
     * @param orientation The orientation to choose to get the size.
     * @return            The size of the content along the provided orientation.
     */
    public float getSizeInScrollDirection(@Orientation int orientation) {
        if (orientation == Orientation.PORTRAIT) {
            return mLayoutTab.getScaledContentHeight();
        } else {
            return mLayoutTab.getScaledContentWidth();
        }
    }

    /**
     * Helper function that gather the static constants from values/dimens.xml.
     * @param context The Android Context.
     */
    public static void resetDimensionConstants(Context context) {
        Resources res = context.getResources();
        final float pxToDp = 1.0f / res.getDisplayMetrics().density;
        sStackedTabVisibleSize =
                res.getDimensionPixelOffset(R.dimen.stacked_tab_visible_size) * pxToDp;
        sStackBufferWidth = res.getDimensionPixelOffset(R.dimen.stack_buffer_width) * pxToDp;
        sStackBufferHeight = res.getDimensionPixelOffset(R.dimen.stack_buffer_height) * pxToDp;
    }

    /**
     * Reset the offset to factory default.
     */
    public void resetOffset() {
        mXInStackInfluence = 1.0f;
        mYInStackInfluence = 1.0f;
        mScrollOffset = 0.0f;
        mXInStackOffset = 0.0f;
        mYInStackOffset = 0.0f;
        mXOutOfStack = 0.0f;
        mYOutOfStack = 0.0f;
        mDiscardOriginX = 0.f;
        mDiscardOriginY = 0.f;
        mDiscardFromClick = false;
    }

    /**
     * Updates the cached visible area value to be used to sort tabs by visibility.
     * @param referenceIndex The index that has the highest priority.
     */
    public void updateVisiblityValue(int referenceIndex) {
        mCachedVisibleArea = mLayoutTab.computeVisibleArea();
        mCachedIndexDistance = Math.abs(mIndex - referenceIndex);
        mOrderSortingValue = computeOrderSortingValue(mCachedIndexDistance, mCacheStackVisibility);
        mVisiblitySortingValue = computeVisibilitySortingValue(
                mCachedVisibleArea, mOrderSortingValue, mCacheStackVisibility);
    }

    /**
     * Updates the cached visible area value to be used to sort tabs by visibility.
     * @param stackVisibility   Multiplier that represents how much the stack fills the screen.
     */
    public void updateStackVisiblityValue(float stackVisibility) {
        mCacheStackVisibility = stackVisibility;
        mOrderSortingValue = computeOrderSortingValue(mCachedIndexDistance, mCacheStackVisibility);
        mVisiblitySortingValue = computeVisibilitySortingValue(
                mCachedVisibleArea, mOrderSortingValue, mCacheStackVisibility);
    }

    /**
     * Computes the visibility sorting value based on the tab visible area, its distance to the
     * central index and the overall visibility of the stack.
     * The '-index' index factor need to be smaller for stack that have small visibility.
     * Multiplying by a small stackVisibility makes it bigger (because it is negative), hence the
     * division. To avoid dividing by 0 it need to be offset a bit. 0.1f is the 'a bit' part of
     * the explanation.
     */
    private static long computeVisibilitySortingValue(
            float area, float orderSortingValue, float stackVisibility) {
        return (long) (area * stackVisibility - orderSortingValue);
    }

    /**
     * @return The cached visible sorting value. Call updateCachedVisibleArea to update it.
     */
    public long getVisiblitySortingValue() {
        return mVisiblitySortingValue;
    }

    /**
     * Computes the ordering value only based on the distance of the tab to the center one.
     * Low values have higher priority.
     */
    private static int computeOrderSortingValue(float indexDistance, float stackVisibility) {
        return (int) ((indexDistance + 1) / (0.1f + 0.9f * stackVisibility));
    }

    /**
     * @return The cached order sorting value. Used to sort based on the tab ordering rather than
     *         visible area.
     */
    public int getOrderSortingValue() {
        return mOrderSortingValue;
    }
}
