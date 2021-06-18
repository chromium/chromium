// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.WindowManager;

import androidx.annotation.Px;

import org.chromium.base.MathUtils;

/**
 * CustomTabHeightStrategy for Partial Custom Tab. An instance of this class should be
 * owned by the CustomTabActivity.
 */
public class PartialCustomTabHeightStrategy extends CustomTabHeightStrategy {
    private static final float MINIMAL_HEIGHT_RATIO = 0.5f;
    private Activity mActivity;
    private @Px int mInitialHeight;
    private final @Px int mMaxHeight;

    public PartialCustomTabHeightStrategy(Activity activity, @Px int initialHeight) {
        mActivity = activity;
        mInitialHeight = initialHeight;
        mMaxHeight = getDisplayHeight();

        initializeHeight();
    }

    private void initializeHeight() {
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        final @Px int heightInPhysicalPixels = clampHeight(mInitialHeight);

        WindowManager.LayoutParams attributes = mActivity.getWindow().getAttributes();
        // TODO(ctzsm): Consider to handle rotation and resizing when entering/exiting multi-window
        // mode.
        if (attributes.height == heightInPhysicalPixels) return;

        attributes.height = heightInPhysicalPixels;
        attributes.gravity = Gravity.BOTTOM;
        mActivity.getWindow().setAttributes(attributes);
    }

    private @Px int clampHeight(@Px int currentHeight) {
        currentHeight = MathUtils.clamp(
                currentHeight, mMaxHeight, (int) (mMaxHeight * MINIMAL_HEIGHT_RATIO));

        if (currentHeight == mMaxHeight) {
            currentHeight = WindowManager.LayoutParams.MATCH_PARENT;
        }

        return currentHeight;
    }

    private @Px int getDisplayHeight() {
        DisplayMetrics displayMetrics = new DisplayMetrics();
        mActivity.getWindowManager().getDefaultDisplay().getRealMetrics(displayMetrics);
        return displayMetrics.heightPixels;
    }
}
