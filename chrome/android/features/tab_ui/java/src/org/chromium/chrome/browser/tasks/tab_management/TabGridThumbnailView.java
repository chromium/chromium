// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.util.AttributeSet;

import org.chromium.base.MathUtils;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
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
        mAspectRatio = MathUtils.clamp(
                (float) TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO.getValue(), 0.5f, 2.0f);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        int measuredWidth = getMeasuredWidth();
        int measureHeight = getMeasuredHeight();

        int expectedHeight = (int) (measuredWidth * 1.0 / mAspectRatio);
        if ((TabUiFeatureUtilities.isLaunchPolishEnabled()
                    || StartSurfaceConfiguration.isStartSurfaceEnabled())
                && (getDrawable() == null
                        || (measureHeight != expectedHeight
                                && getDrawable() instanceof ColorDrawable))) {
            measureHeight = expectedHeight;
        }

        setMeasuredDimension(measuredWidth, measureHeight);
    }
}