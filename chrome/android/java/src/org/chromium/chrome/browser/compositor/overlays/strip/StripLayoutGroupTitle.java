// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.graphics.RectF;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;

import org.chromium.base.MathUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupTitleUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;

/**
 * {@link StripLayoutGroupTitle} is used to keep track of the strip position and rendering
 * information for a particular tab group title indicator on the tab strip so it can draw itself
 * onto the GL canvas.
 */
public class StripLayoutGroupTitle extends StripLayoutView {
    // Position constants.
    // TODO(crbug.com/326492662): Update min/max width once finalized.
    private static final int MIN_WIDTH = 54;
    private static final int MAX_WIDTH = 156;

    // External dependencies.
    private Context mContext;

    // Position variables.
    private float mDrawX;
    private float mDrawY;
    private float mWidth;
    private float mHeight;

    // Tab group variables
    int mRootId;
    String mTitle = "";

    public StripLayoutGroupTitle(Context context, int rootId) {
        assert rootId != Tab.INVALID_TAB_ID : "Tried to create a group title for an invalid group.";

        mContext = context;
        mRootId = rootId;
        updateTitle(TabGroupTitleUtils.getTabGroupTitle(mRootId));
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
        width = MathUtils.clamp(width, MIN_WIDTH, MAX_WIDTH);
        mWidth = width;
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
        return "Tab group: " + mTitle;
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
     * @return The Android resource that represents the tab group title indicator background.
     */
    public @DrawableRes int getResourceId() {
        // TODO(crbug.com/326492662): Replace with new 9-patch if needed.
        return TabUiThemeUtil.getDetachedResource();
    }

    /**
     * @return The tint color resource that represents the tab group title indicator background.
     */
    public @ColorInt int getTint() {
        // TODO(crbug.com/326488897): Pull color from Tab Group API.
        return mContext.getColor(R.color.google_red_600);
    }

    protected void updateTitle(String title) {
        mTitle = title;
        // TODO(crbug.com/326488897): Generate title bitmap and update width if necessary.
        setWidth(0);
    }

    /**
     * @return The group's root ID.
     */
    protected int getRootId() {
        return mRootId;
    }

    /**
     * @param rootId The tab group's new rootId. Should be synced with the {@link
     *     org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter}.
     */
    protected void updateRootId(int rootId) {
        mRootId = rootId;
    }
}
