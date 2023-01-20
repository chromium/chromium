// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.animation.AccelerateInterpolator;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;

/**
 * CustomTabHeightStrategy for Partial Custom Tab Side-Sheet implementation. An instance of this
 * class should be owned by the CustomTabActivity.
 */
public class PartialCustomTabSideSheetStrategy extends PartialCustomTabBaseStrategy {
    private final @Px int mUnclampedInitialWidth;

    private ValueAnimator mCloseAnimator;

    public PartialCustomTabSideSheetStrategy(Activity activity, @Px int initialHeight,
            boolean isFixedHeight, CustomTabHeightStrategy.OnResizedCallback onResizedCallback,
            FullscreenManager fullscreenManager, boolean isTablet, boolean interactWithBackground) {
        super(activity, initialHeight, isFixedHeight, onResizedCallback, fullscreenManager,
                isTablet, interactWithBackground);

        // TODO(crbug.com/1406104) Allow customization for the side-sheet width.
        //  For now it's half of the window.
        mUnclampedInitialWidth = mVersionCompat.getDisplayWidth() / 2;
        mPositionUpdater = this::updatePosition;
        setupCloseAnimation();
    }

    @Override
    @PartialCustomTabType
    public int getStrategyType() {
        return PartialCustomTabType.SIDE_SHEET;
    }

    @Override
    public void onShowSoftInput(Runnable softKeyboardRunnable) {
        softKeyboardRunnable.run();
    }

    @Override
    public void handleCloseAnimation(Runnable finishRunnable) {
        if (mFinishRunnable != null) return;

        mFinishRunnable = finishRunnable;

        Window window = mActivity.getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        WindowManager.LayoutParams attrs = window.getAttributes();

        int start = attrs.y;
        int end = mHeight;
        mCloseAnimator.setIntValues(start, end);
        mCloseAnimator.start();
    }

    @Override
    public void onToolbarInitialized(
            View coordinatorView, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        super.onToolbarInitialized(coordinatorView, toolbar, toolbarCornerRadius);

        toolbar.setHandleStrategy(null);
        updateDragBarVisibility(/*dragHandlebarVisibility*/ View.GONE);
    }

    @Override
    protected int getHandleHeight() {
        // TODO(crbug.com/1408288) by default the side-sheet will have no round corners so this will
        // just return 0. We will implement the handle height logic as part of adding customization
        // support.
        return 0;
    }

    @Override
    protected boolean isFullHeight() {
        return false;
    }

    @Override
    protected void updatePosition() {
        if (mActivity.findViewById(android.R.id.content) == null) return;

        initializeSize();
        updateShadowOffset();
        // TODO(crbug.com/1406107): Check if we should invoke the resize callback
    }

    @Override
    protected void setTopMargins(int shadowOffset, int handleOffset) {
        View handleView = mActivity.findViewById(org.chromium.chrome.R.id.custom_tabs_handle_view);
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) handleView.getLayoutParams();
        lp.setMargins(shadowOffset, 0, 0, 0);

        // Make enough room for the handle View.
        int topOffset = Math.max(handleOffset - shadowOffset, 0);
        ViewGroup.MarginLayoutParams mlp =
                (ViewGroup.MarginLayoutParams) mToolbarCoordinator.getLayoutParams();
        mlp.setMargins(shadowOffset, topOffset, 0, 0);
    }

    @Override
    protected boolean shouldHaveNoShadowOffset() {
        return false;
    }

    @Override
    protected void adjustCornerRadius(GradientDrawable d, int radius) {
        d.mutate();
        // Left top corner rounded.
        d.setCornerRadii(new float[] {radius, radius, 0, 0, 0, 0, 0, 0});
    }

    private void setupCloseAnimation() {
        mCloseAnimator = new ValueAnimator();
        mCloseAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {}
            @Override
            public void onAnimationEnd(Animator animation) {
                assert mFinishRunnable != null;

                mFinishRunnable.run();
                mFinishRunnable = null;
            }
        });

        int animTime = mActivity.getResources().getInteger(android.R.integer.config_mediumAnimTime);
        mCloseAnimator.setDuration(animTime);
        mCloseAnimator.setInterpolator(new AccelerateInterpolator());
    }

    private void initializeSize() {
        initializeHeight();
        mHeight = mDisplayHeight - mStatusbarHeight;

        positionOnWindow();
        setCoordinatorLayoutHeight(MATCH_PARENT);

        updateDragBarVisibility(/*dragHandlebarVisibility*/ View.GONE);
    }

    private void positionOnWindow() {
        WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
        attrs.height = mHeight;
        attrs.width = mUnclampedInitialWidth;
        attrs.y = mNavbarHeight;
        attrs.x = 0;
        attrs.gravity = Gravity.END;
        mActivity.getWindow().setAttributes(attrs);
    }

    @VisibleForTesting
    void setMockViewForTesting(View toolbar, View toolbarCoordinator) {
        mPositionUpdater = this::updatePosition;
        mToolbarView = toolbar;
        mToolbarCoordinator = toolbarCoordinator;

        onPostInflationStartup();
    }
}