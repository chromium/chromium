// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.util.AttributeSet;

import org.chromium.components.browser_ui.widget.RoundedCornerImageView;

/**
 * A specialized {@link RoundedCornerImageView} that allows to set a width and height aspect ratio
 * to use when the image drawable is null or a {@link ColorDrawable}. The height is varied based on
 * the width and the aspect ratio. If TabUiFeatureUtilities.isLaunchPolishEnabled() is false, the
 * behavior of this Class is the same as the RoundedCornerImageView.
 */
public class TabGridThumbnailView extends RoundedCornerImageView {
    private static final float DEFAULT_RATIO = 1.0f;
    private float mRatio = DEFAULT_RATIO;

    public TabGridThumbnailView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Set width and height aspect ratio.
     * @param ratio The width and height aspect ratio, width over height.
     */
    public void setAspectRatio(float ratio) {
        mRatio = ratio;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        int measuredWidth = getMeasuredWidth();
        int measureHeight = getMeasuredHeight();

        if (TabUiFeatureUtilities.isLaunchPolishEnabled()
                && (getDrawable() == null
                        || (mRatio != DEFAULT_RATIO && getDrawable() instanceof ColorDrawable))) {
            measureHeight = (int) (measuredWidth * 1.0 / mRatio);
        }

        setMeasuredDimension(measuredWidth, measureHeight);
    }
}