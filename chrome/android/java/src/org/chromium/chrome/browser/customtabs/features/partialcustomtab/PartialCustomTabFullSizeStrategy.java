// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import android.app.Activity;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;

import androidx.annotation.Px;

import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;

/**
 * CustomTabHeightStrategy for Partial Custom Tab Full-Size implementation. An instance of this
 * class should be owned by the CustomTabActivity.
 */
public class PartialCustomTabFullSizeStrategy extends PartialCustomTabBaseStrategy {
    public PartialCustomTabFullSizeStrategy(Activity activity,
            CustomTabHeightStrategy.OnResizedCallback onResizedCallback,
            FullscreenManager fullscreenManager, boolean isTablet, boolean interactWithBackground,
            PartialCustomTabHandleStrategyFactory handleStrategyFactory) {
        super(activity, onResizedCallback, fullscreenManager, isTablet, interactWithBackground,
                handleStrategyFactory);

        mPositionUpdater = this::updatePosition;

        setupAnimator();
    }

    @Override
    public int getStrategyType() {
        return PartialCustomTabType.FULL_SIZE;
    }

    @Override
    public void onToolbarInitialized(
            View coordinatorView, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        super.onToolbarInitialized(coordinatorView, toolbar, toolbarCornerRadius);

        PartialCustomTabHandleStrategy handleStrategy = mHandleStrategyFactory.create(
                getStrategyType(), mActivity, this::isFullHeight, () -> 0, null);
        toolbar.setHandleStrategy(handleStrategy);
        updateDragBarVisibility(/*dragHandlebarVisibility*/ View.GONE);
    }

    @Override
    protected void updatePosition() {
        if (isFullscreen() || mActivity.findViewById(android.R.id.content) == null) return;

        initializeSize();
        updateShadowOffset();
        // TODO(crbug.com/1406107): Check if we should invoke the resize callback
    }

    @Override
    protected void initializeSize() {
        mHeight = mVersionCompat.getDisplayHeight();
        mWidth = mVersionCompat.getDisplayWidth();

        positionOnWindow();
        setCoordinatorLayoutHeight(MATCH_PARENT);

        updateDragBarVisibility(/*dragHandlebarVisibility*/ View.GONE);
    }

    @Override
    protected int getHandleHeight() {
        return 0;
    }

    @Override
    protected boolean isFullHeight() {
        return true;
    }

    @Override
    protected void cleanupImeStateCallback() {
        mVersionCompat.setImeStateCallback(null);
    }

    @Override
    protected void adjustCornerRadius(GradientDrawable d, int radius) {}

    @Override
    protected void setTopMargins(int shadowOffset, int handleOffset) {
        // No offset as we will not have handle view in full-screen
        View handleView = mActivity.findViewById(org.chromium.chrome.R.id.custom_tabs_handle_view);
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) handleView.getLayoutParams();
        lp.setMargins(0, 0, 0, 0);

        ViewGroup.MarginLayoutParams mlp =
                (ViewGroup.MarginLayoutParams) mToolbarCoordinator.getLayoutParams();
        mlp.setMargins(0, 0, 0, 0);
    }

    @Override
    protected boolean shouldHaveNoShadowOffset() {
        return true;
    }

    @Override
    protected boolean isMaximized() {
        return false;
    }

    private void positionOnWindow() {
        WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
        attrs.height = MATCH_PARENT;
        attrs.width = MATCH_PARENT;

        attrs.y = 0;
        attrs.x = 0;
        attrs.gravity = Gravity.TOP;
        mActivity.getWindow().setAttributes(attrs);
    }
}
