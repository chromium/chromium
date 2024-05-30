// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_FULL_SCREEN;

import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.app.Activity;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;

import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.browser.customtabs.CustomTabsCallback;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;

/**
 * CustomTabHeightStrategy for Partial Custom Tab Full-Size implementation. An instance of this
 * class should be owned by the CustomTabActivity.
 */
public class PartialCustomTabFullSizeStrategy extends PartialCustomTabBaseStrategy {
    public PartialCustomTabFullSizeStrategy(
            Activity activity,
            BrowserServicesIntentDataProvider intentData,
            CustomTabHeightStrategy.OnResizedCallback onResizedCallback,
            CustomTabHeightStrategy.OnActivityLayoutCallback onActivityLayoutCallback,
            FullscreenManager fullscreenManager,
            boolean isTablet,
            PartialCustomTabHandleStrategyFactory handleStrategyFactory) {
        super(
                activity,
                intentData,
                onResizedCallback,
                onActivityLayoutCallback,
                fullscreenManager,
                isTablet,
                handleStrategyFactory);

        mPositionUpdater = this::updatePosition;

        setupAnimator();
    }

    @Override
    public int getStrategyType() {
        return PartialCustomTabType.FULL_SIZE;
    }

    @Override
    public @StringRes int getTypeStringId() {
        return R.string.accessibility_partial_custom_tab_full_sheet;
    }

    @Override
    public void onToolbarInitialized(
            View coordinatorView, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        super.onToolbarInitialized(coordinatorView, toolbar, toolbarCornerRadius);
        toolbar.setMinimizeButtonEnabled(true);
        updateDragBarVisibility(/* dragHandlebarVisibility= */ View.GONE);
    }

    @Override
    protected void updatePosition() {
        if (isFullscreen() || mActivity.findViewById(android.R.id.content) == null) return;

        initializeSize();
        updateShadowOffset();
        maybeInvokeResizeCallback();
    }

    @Override
    protected void initializeSize() {
        mHeight = mVersionCompat.getDisplayHeight();
        mWidth = mVersionCompat.getDisplayWidth();

        positionOnWindow();
        setCoordinatorLayoutHeight(MATCH_PARENT);

        updateDragBarVisibility(/* dragHandlebarVisibility= */ View.GONE);
    }

    @Override
    public boolean handleCloseAnimation(Runnable finishRunnable) {
        if (!super.handleCloseAnimation(finishRunnable)) return false;

        configureLayoutBeyondScreen(true);
        AnimatorUpdateListener updater = animator -> setWindowY((int) animator.getAnimatedValue());
        int start = mActivity.getWindow().getAttributes().y;
        startAnimation(start, mHeight, updater, this::onCloseAnimationEnd);
        return true;
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
    protected @CustomTabsCallback.ActivityLayoutState int getActivityLayoutState() {
        return ACTIVITY_LAYOUT_STATE_FULL_SCREEN;
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
    protected int getCustomTabsElevation() {
        return 0;
    }

    @Override
    protected boolean shouldHaveNoShadowOffset() {
        return true;
    }

    @Override
    protected boolean isMaximized() {
        return false;
    }

    @Override
    protected boolean shouldDrawDividerLine() {
        return false;
    }

    @Override
    protected void drawDividerLine() {}

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
