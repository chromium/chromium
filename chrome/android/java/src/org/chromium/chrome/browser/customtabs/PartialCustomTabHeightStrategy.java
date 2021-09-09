// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.graphics.drawable.GradientDrawable;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.WindowManager;
import android.widget.ImageView;

import androidx.annotation.Px;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.MathUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;

/**
 * CustomTabHeightStrategy for Partial Custom Tab. An instance of this class should be
 * owned by the CustomTabActivity.
 */
public class PartialCustomTabHeightStrategy
        extends CustomTabHeightStrategy implements InflationObserver {
    private static final float MINIMAL_HEIGHT_RATIO = 0.5f;
    private Activity mActivity;
    private @Px int mInitialHeight;
    private final @Px int mMaxHeight;

    public PartialCustomTabHeightStrategy(Activity activity, @Px int initialHeight,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mInitialHeight = initialHeight;
        mMaxHeight = getDisplayHeight();

        lifecycleDispatcher.register(this);

        initializeHeight();
    }

    @Override
    public boolean changeBackgroundColorForResizing() {
        GradientDrawable background =
                (GradientDrawable) mActivity.getWindow().getDecorView().getBackground();
        if (background == null) {
            return false;
        }

        final int color = ApiCompatibilityUtils.getColor(
                mActivity.getResources(), R.color.resizing_background_color);
        ((GradientDrawable) background.mutate()).setColor(color);
        return true;
    }

    @Override
    public void onPreInflationStartup() {
        // Intentionally no-op, we registered this class during the pre-inflation startup stage, so
        // this method won't be called.
    }

    @Override
    public void onPostInflationStartup() {
        roundCorners();
    }

    private void roundCorners() {
        final float radius = mActivity.getResources().getDimensionPixelSize(
                R.dimen.custom_tabs_top_corner_round_radius);
        View coordinator = mActivity.findViewById(R.id.coordinator);

        // Inflate the handle View.
        ViewStub handleViewStub = mActivity.findViewById(R.id.custom_tabs_handle_view_stub);
        handleViewStub.inflate();
        ImageView handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);

        // Pass the handle View to CustomTabToolbar for background color management.
        CustomTabToolbar toolbar = mActivity.findViewById(R.id.toolbar);
        toolbar.setHandleView(handleView);

        // Make enough room for the handle View.
        ViewGroup.MarginLayoutParams mlp =
                (ViewGroup.MarginLayoutParams) coordinator.getLayoutParams();
        mlp.setMargins(0, Math.round(radius), 0, 0);
        coordinator.requestLayout();

        mActivity.getWindow().setBackgroundDrawableResource(
                R.drawable.custom_tabs_handle_view_shape);
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
