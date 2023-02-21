// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER;
import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_NONE;

import android.animation.ValueAnimator;
import android.app.Activity;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;

/**
 * CustomTabHeightStrategy for Partial Custom Tab Side-Sheet implementation. An instance of this
 * class should be owned by the CustomTabActivity.
 */
public class PartialCustomTabSideSheetStrategy extends PartialCustomTabBaseStrategy {
    private static final float MINIMAL_WIDTH_RATIO = 0.33f;
    private final @Px int mUnclampedInitialWidth;
    private final boolean mShowMaximizeButton;

    private boolean mIsMaximized;
    private int mDecorationType;

    public PartialCustomTabSideSheetStrategy(Activity activity, @Px int initialWidth,
            CustomTabHeightStrategy.OnResizedCallback onResizedCallback,
            FullscreenManager fullscreenManager, boolean isTablet, boolean interactWithBackground,
            boolean showMaximizeButton, boolean startMaximized,
            PartialCustomTabHandleStrategyFactory handleStrategyFactory, int decorationType) {
        super(activity, onResizedCallback, fullscreenManager, isTablet, interactWithBackground,
                handleStrategyFactory);

        mUnclampedInitialWidth = initialWidth;
        mShowMaximizeButton = showMaximizeButton;
        mPositionUpdater = this::updatePosition;
        mIsMaximized = startMaximized;
        mDecorationType = decorationType;

        setupAnimator();
    }

    @Override
    public void onPostInflationStartup() {
        super.onPostInflationStartup();

        if (mIsMaximized) {
            mIsMaximized = false;
            toggleMaximize();
        }
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

    private void configureLayoutBeyondScreen(boolean enable) {
        Window window = mActivity.getWindow();
        if (enable) {
            window.addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        } else {
            window.clearFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        }
    }

    @Override
    public void onToolbarInitialized(
            View coordinatorView, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        super.onToolbarInitialized(coordinatorView, toolbar, toolbarCornerRadius);

        PartialCustomTabHandleStrategy handleStrategy = mHandleStrategyFactory.create(
                getStrategyType(), mActivity, this::isFullHeight, () -> 0, null);
        if (mShowMaximizeButton) {
            toolbar.initSideSheetMaximizeButton(mIsMaximized, this::toggleMaximize);
        }
        toolbar.setHandleStrategy(handleStrategy);
        updateDragBarVisibility(/*dragHandlebarVisibility*/ View.GONE);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    boolean toggleMaximize() {
        mIsMaximized = !mIsMaximized;
        if (mIsMaximized) {
            setTopMargins(0, 0);
        } else {
            updateShadowOffset();
        }
        configureLayoutBeyondScreen(true);

        // For smooth animation, make the window full-width and then translate it
        // rather than resizing the window itself during the animation.
        int displayWidth = mVersionCompat.getDisplayWidth();
        setWindowWidth(displayWidth);
        int start = mActivity.getWindow().getAttributes().x;
        int end = mIsMaximized ? 0 : displayWidth - calculateWidth(mUnclampedInitialWidth);
        startAnimation(start, end, this::onMaximizeProgress, this::onMaximizeEnd);
        return mIsMaximized;
    }

    private void setWindowWidth(int width) {
        var attrs = mActivity.getWindow().getAttributes();
        attrs.width = width;
        mActivity.getWindow().setAttributes(attrs);
    }

    private void onMaximizeProgress(ValueAnimator animator) {
        var attrs = mActivity.getWindow().getAttributes();
        attrs.x = (int) animator.getAnimatedValue();
        mActivity.getWindow().setAttributes(attrs);
    }

    private void onMaximizeEnd() {
        if (isMaximized()) {
            configureLayoutBeyondScreen(false);
            notifyResized();
        } else {
            // System UI dimensions are not settled yet. Post the task.
            new Handler().post(() -> {
                configureLayoutBeyondScreen(false);
                initializeSize();
                notifyResized();
            });
        }
    }

    private void notifyResized() {
        var attrs = mActivity.getWindow().getAttributes();
        mOnResizedCallback.onResized(attrs.height, attrs.width);
    }

    @Override
    protected boolean isMaximized() {
        return mIsMaximized;
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
        if (isFullscreen() || mActivity.findViewById(android.R.id.content) == null) return;

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
        // We remove shadow in maximized mode.
        return isMaximized() || mDecorationType == ACTIVITY_SIDE_SHEET_DECORATION_TYPE_NONE
                || mDecorationType == ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER;
    }

    @Override
    protected void adjustCornerRadius(GradientDrawable d, int radius) {
        d.mutate();
        // Left top corner rounded.
        d.setCornerRadii(new float[] {radius, radius, 0, 0, 0, 0, 0, 0});
    }

    @Override
    protected void cleanupImeStateCallback() {
        mVersionCompat.setImeStateCallback(null);
    }

    @Override
    protected void initializeSize() {
        initializeHeight();
        mHeight = mDisplayHeight - mStatusbarHeight - mNavbarHeight;

        positionOnWindow();
        setCoordinatorLayoutHeight(MATCH_PARENT);

        updateDragBarVisibility(/*dragHandlebarVisibility*/ View.GONE);
    }

    private void positionOnWindow() {
        int width = calculateWidth(mUnclampedInitialWidth);
        WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
        attrs.height = mHeight;
        attrs.width = width;

        attrs.y = mStatusbarHeight;
        attrs.x = mVersionCompat.getDisplayWidth() - attrs.width;
        attrs.gravity = Gravity.TOP | Gravity.START;
        mActivity.getWindow().setAttributes(attrs);
    }

    private int calculateWidth(int unclampedWidth) {
        return MathUtils.clamp(unclampedWidth, mVersionCompat.getDisplayWidth(),
                (int) (mVersionCompat.getDisplayWidth() * MINIMAL_WIDTH_RATIO));
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mShowMaximizeButton) ((CustomTabToolbar) mToolbarView).removeSideSheetMaximizeButton();
    }
}
