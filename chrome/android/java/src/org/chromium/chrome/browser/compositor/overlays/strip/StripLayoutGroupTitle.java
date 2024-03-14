// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.graphics.RectF;

import androidx.annotation.ColorInt;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupTitleUtils;

/**
 * {@link StripLayoutGroupTitle} is used to keep track of the strip position and rendering
 * information for a particular tab group title indicator on the tab strip so it can draw itself
 * onto the GL canvas.
 */
public class StripLayoutGroupTitle extends StripLayoutView {
    // Position constants.
    private static final int MIN_VISUAL_WIDTH_DP = 24;
    private static final int MAX_VISUAL_WIDTH_DP = 156;
    private static final int DEFAULT_MARGIN_DP = 9;
    private static final int TOP_MARGIN_DP = 7;
    private static final int TEXT_PADDING_DP = 8;
    private static final int CORNER_RADIUS_DP = 7;

    private final float mEffectiveMinWidth;
    private final float mEffectiveMaxWidth;

    // External dependencies.
    private final Context mContext;
    private final LayoutUpdateHost mUpdateHost;

    // Position variables.
    private float mDrawX;
    private float mDrawY;
    private float mWidth;
    private float mHeight;

    // Tab group variables.
    int mRootId;
    String mTitle;
    @ColorInt int mColor;

    public StripLayoutGroupTitle(
            Context context, LayoutUpdateHost updateHost, int rootId, @ColorInt int color) {
        assert rootId != Tab.INVALID_TAB_ID : "Tried to create a group title for an invalid group.";

        mContext = context;
        mColor = color;
        mUpdateHost = updateHost;
        mEffectiveMinWidth = MIN_VISUAL_WIDTH_DP + (DEFAULT_MARGIN_DP * 2);
        mEffectiveMaxWidth = MAX_VISUAL_WIDTH_DP + (DEFAULT_MARGIN_DP * 2);

        updateRootId(rootId);
        updateTitle(TabGroupTitleUtils.getTabGroupTitle(mRootId), 0);
    }

    @Override
    public float getDrawX() {
        return mDrawX;
    }

    @Override
    public void setDrawX(float x) {
        mDrawX = x;
    }

    @Override
    public float getDrawY() {
        return mDrawY;
    }

    @Override
    public void setDrawY(float y) {
        mDrawY = y;
    }

    @Override
    public float getWidth() {
        return mWidth;
    }

    @Override
    public void setWidth(float width) {
        // Increment to prevent off-by-one rounding errors adding a title fade when unnecessary.
        width = width + (DEFAULT_MARGIN_DP * 2) + 1;
        width = MathUtils.clamp(width, mEffectiveMinWidth, mEffectiveMaxWidth);
        if (mWidth != width) {
            mWidth = width;
            mUpdateHost.requestUpdate();
        }
    }

    @Override
    public float getHeight() {
        return mHeight;
    }

    @Override
    public void setHeight(float height) {
        mHeight = height;
    }

    @Override
    public String getAccessibilityDescription() {
        // TODO(crbug.com/326494015): Update when official descriptions are finalized.
        return "Tab group";
    }

    @Override
    public void getTouchTarget(RectF outTarget) {
        // TODO(crbug.com/326492955): Add touch target.
    }

    @Override
    public boolean checkClickedOrHovered(float x, float y) {
        // TODO(crbug.com/326492955): Implement click to collapse/expand.
        return false;
    }

    @Override
    public void handleClick(long time) {
        // No-op for now. We eventually plan to add functionality, such as collapsing a tab group.
    }

    /**
     * @return The tint color resource that represents the tab group title indicator background.
     */
    public @ColorInt int getTint() {
        // TODO(crbug.com/326492787): Update whenever tab group's color may have changed.
        return mColor;
    }

    protected void updateTitle(String title, float width) {
        mTitle = title;
        setWidth(width + (TEXT_PADDING_DP * 2));
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
     * @return The default margin for the title container. The top margin will be different.
     */
    public int getDefaultMargin() {
        return DEFAULT_MARGIN_DP;
    }

    /**
     * @return The top margin for the title container.
     */
    public int getTopMargin() {
        return TOP_MARGIN_DP;
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
}
