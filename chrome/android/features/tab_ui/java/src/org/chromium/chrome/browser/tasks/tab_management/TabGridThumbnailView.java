// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.util.AttributeSet;

import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;

/**
 * A specialized {@link RoundedCornerImageView} that allows to set a width and height aspect ratio
 * to use when the image drawable is null or a {@link ColorDrawable}. The height is varied based on
 * the width and the aspect ratio. If TabUiFeatureUtilities.isLaunchPolishEnabled() is false, the
 * behavior of this Class is the same as the RoundedCornerImageView.
 */
public class TabGridThumbnailView extends RoundedCornerImageView {
    private final float mAspectRatio;

    public TabGridThumbnailView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAspectRatio = TabUtils.getTabThumbnailAspectRatio(context);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        int measuredWidth = getMeasuredWidth();
        int measureHeight = getMeasuredHeight();

        int expectedHeight = (int) (measuredWidth * 1.0 / mAspectRatio);
        if ((TabUiFeatureUtilities.isLaunchPolishEnabled()
                    || ReturnToChromeExperimentsUtil.isStartSurfaceEnabled(getContext()))
                && isPlaceHolder()) {
            measureHeight = expectedHeight;
        }

        setMeasuredDimension(measuredWidth, measureHeight);
    }

    /** Return whether the image drawable is null or a {@link ColorDrawable}. */
    boolean isPlaceHolder() {
        return getDrawable() == null || (getDrawable() instanceof ColorDrawable);
    }

    /**
     * Set the thumbnail placeholder base on whether it is used for an incognito / selected tab.
     * @param isIncognito Whether the thumbnail is on an incognito tab.
     * @param isSelected Whether the thumbnail is on a selected tab.
     */
    void setColorThumbnailPlaceHolder(boolean isIncognito, boolean isSelected) {
        if (!TabUiThemeProvider.themeRefactorEnabled()) {
            setImageResource(TabUiThemeProvider.getThumbnailPlaceHolderColorResource(isIncognito));
            return;
        }

        ColorDrawable placeHolder =
                new ColorDrawable(TabUiThemeProvider.getMiniThumbnailPlaceHolderColor(
                        getContext(), isIncognito, isSelected));
        setImageDrawable(placeHolder);
    }

    /**
     * Adjust the thumbnail height according to tab ui features.
     */
    void maybeAdjustThumbnailHeight() {
        if (TabUiFeatureUtilities.isLaunchPolishEnabled()) {
            return;
        }

        float expectedThumbnailAspectRatio = TabUtils.getTabThumbnailAspectRatio(getContext());
        int height = (int) (getWidth() * 1.0 / expectedThumbnailAspectRatio);
        setMinimumHeight(Math.min(getHeight(), height));
    }
}