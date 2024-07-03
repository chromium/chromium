// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.graphics.RectF;
import android.util.FloatProperty;

import androidx.annotation.ColorInt;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.LocalizationUtils;

/**
 * {@link StripLayoutGroupTitle} is used to keep track of the strip position and rendering
 * information for a particular tab group title indicator on the tab strip so it can draw itself
 * onto the GL canvas.
 */
public class StripLayoutGroupTitle extends StripLayoutView {
    /** Delegate for additional group title functionality. */
    public interface StripLayoutGroupTitleDelegate {
        /**
         * Releases the resources associated with this group indicator.
         *
         * @param rootId The root ID of the given group indicator.
         */
        void releaseResourcesForGroupTitle(int rootId);

        /**
         * Handles group title click action.
         *
         * @param groupTitle The group title that was clicked.
         */
        void handleGroupTitleClick(StripLayoutGroupTitle groupTitle);
    }

    /** A property for animations to use for changing the width of the bottom indicator. */
    public static final FloatProperty<StripLayoutGroupTitle> BOTTOM_INDICATOR_WIDTH =
            new FloatProperty<>("bottomIndicatorWidth") {
                @Override
                public void setValue(StripLayoutGroupTitle object, float value) {
                    object.setBottomIndicatorWidth(value);
                }

                @Override
                public Float get(StripLayoutGroupTitle object) {
                    return object.getBottomIndicatorWidth();
                }
            };

    // Position constants.
    private static final int MIN_VISUAL_WIDTH_DP = 24;
    private static final int MAX_VISUAL_WIDTH_DP = 156;

    private static final int MARGIN_TOP_DP = 7;
    private static final int MARGIN_BOTTOM_DP = 9;
    private static final int MARGIN_START_DP = 13;
    private static final int MARGIN_END_DP = 9;

    private static final int TEXT_PADDING_DP = 8;
    private static final int CORNER_RADIUS_DP = 7;
    private static final float BOTTOM_INDICATOR_HEIGHT_DP = 2.f;

    private static final int WIDTH_MARGINS_DP = MARGIN_START_DP + MARGIN_END_DP;
    private static final int EFFECTIVE_MIN_WIDTH = MIN_VISUAL_WIDTH_DP + WIDTH_MARGINS_DP;
    private static final int EFFECTIVE_MAX_WIDTH = MAX_VISUAL_WIDTH_DP + WIDTH_MARGINS_DP;

    // External influences.
    private final StripLayoutGroupTitleDelegate mDelegate;

    // State variables.
    private final boolean mIncognito;

    // Position variables.
    private float mDrawX;
    private float mDrawY;
    private float mWidth;
    private float mHeight;
    private final RectF mTouchTarget = new RectF();

    // Tab group variables.
    private int mRootId;
    private String mTitle;
    @ColorInt private int mColor;

    private String mAccessibilityDescription = "";

    // Bottom indicator variables
    private float mBottomIndicatorWidth;

    /**
     * Create a {@link StripLayoutGroupTitle} that represents the TabGroup for the {@code rootId}.
     *
     * @param delegate The delegate for additional strip group title functionality.
     * @param incognito Whether or not this tab group is Incognito.
     * @param rootId The root ID for the tab group.
     */
    public StripLayoutGroupTitle(
            StripLayoutGroupTitleDelegate delegate, boolean incognito, int rootId) {
        assert rootId != Tab.INVALID_TAB_ID : "Tried to create a group title for an invalid group.";

        mDelegate = delegate;
        mIncognito = incognito;

        updateRootId(rootId);
    }

    @Override
    public float getDrawX() {
        return mDrawX;
    }

    @Override
    public void setDrawX(float x) {
        mDrawX = x;
        mTouchTarget.left = x;
        mTouchTarget.right = x + mWidth;
    }

    @Override
    public float getDrawY() {
        return mDrawY;
    }

    @Override
    public void setDrawY(float y) {
        mDrawY = y;
        mTouchTarget.top = y;
        mTouchTarget.bottom = y + mHeight;
    }

    @Override
    public float getWidth() {
        return mWidth;
    }

    @Override
    public void setWidth(float width) {
        mWidth = width;
        mTouchTarget.right = mDrawX + mWidth;
    }

    @Override
    public float getHeight() {
        return mHeight;
    }

    @Override
    public void setHeight(float height) {
        mHeight = height;
        mTouchTarget.bottom = mDrawY + mHeight;
    }

    @Override
    public void setVisible(boolean visible) {
        super.setVisible(visible);

        if (!visible) mDelegate.releaseResourcesForGroupTitle(mRootId);
    }

    @Override
    public String getAccessibilityDescription() {
        return mAccessibilityDescription;
    }

    protected void setAccessibilityDescription(String accessibilityDescription) {
        mAccessibilityDescription = accessibilityDescription;
    }

    @Override
    public void getTouchTarget(RectF outTarget) {
        outTarget.set(mTouchTarget);
    }

    @Override
    public boolean checkClickedOrHovered(float x, float y) {
        return mTouchTarget.contains(x, y);
    }

    @Override
    public boolean hasClickAction() {
        return ChromeFeatureList.sTabStripGroupCollapse.isEnabled();
    }

    @Override
    public boolean hasLongClickAction() {
        // TODO(https://crbug.com/333777015): Implement long press to drag tab group.
        return false;
    }

    @Override
    public void handleClick(long time) {
        mDelegate.handleGroupTitleClick(this);
    }

    /**
     * @return Whether the tab group this represents is Incognito or not.
     */
    public boolean isIncognito() {
        return mIncognito;
    }

    /**
     * @return DrawX accounting for padding.
     */
    public float getPaddedX() {
        return mDrawX + (LocalizationUtils.isLayoutRtl() ? MARGIN_END_DP : MARGIN_START_DP);
    }

    /**
     * @return DrawY accounting for padding.
     */
    public float getPaddedY() {
        return mDrawY + MARGIN_TOP_DP;
    }

    /**
     * @return Width accounting for padding.
     */
    public float getPaddedWidth() {
        return mWidth - MARGIN_START_DP - MARGIN_END_DP;
    }

    /**
     * @return Height accounting for padding.
     */
    public float getPaddedHeight() {
        return mHeight - MARGIN_TOP_DP - MARGIN_BOTTOM_DP;
    }

    /**
     * @return The tint color resource that represents the tab group title indicator background.
     */
    public @ColorInt int getTint() {
        return mColor;
    }

    /**
     * @param color The color used when displaying this group.
     */
    public void updateTint(@ColorInt int color) {
        mColor = color;
    }

    /**
     * @return The group's title.
     */
    protected String getTitle() {
        return mTitle;
    }

    protected void updateTitle(String title, float textWidth) {
        mTitle = title;

        // Account for view padding & margins. Increment to prevent off-by-one rounding errors
        // adding a title fade when unnecessary.
        float viewWidth = textWidth + (TEXT_PADDING_DP * 2) + WIDTH_MARGINS_DP + 1;
        setWidth(MathUtils.clamp(viewWidth, EFFECTIVE_MIN_WIDTH, EFFECTIVE_MAX_WIDTH));
    }

    /**
     * @return The group's root ID.
     */
    public int getRootId() {
        return mRootId;
    }

    /**
     * @param rootId The tab group's new rootId. Should be synced with the {@link
     *     org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter}.
     */
    protected void updateRootId(int rootId) {
        mRootId = rootId;
    }

    /**
     * @return The padding for the title text.
     */
    public int getTitleTextPadding() {
        return TEXT_PADDING_DP;
    }

    /**
     * @return The corner radius for the title container.
     */
    public int getCornerRadius() {
        return CORNER_RADIUS_DP;
    }

    /**
     * @return The width of the bottom indicator should be applied to this tab group.
     */
    public float getBottomIndicatorWidth() {
        return mBottomIndicatorWidth;
    }

    /**
     * @param bottomIndicatorWidth The width of the bottom indicator should be applied to this tab
     *     group.
     */
    public void setBottomIndicatorWidth(float bottomIndicatorWidth) {
        mBottomIndicatorWidth = bottomIndicatorWidth;
    }

    /**
     * @return The height of the bottom indicator should be applied to this tab group.
     */
    public float getBottomIndicatorHeight() {
        return BOTTOM_INDICATOR_HEIGHT_DP;
    }
}
