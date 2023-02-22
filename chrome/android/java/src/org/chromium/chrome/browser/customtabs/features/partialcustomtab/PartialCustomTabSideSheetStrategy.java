// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER;
import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_NONE;

import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
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
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.ui.base.LocalizationUtils;

/**
 * CustomTabHeightStrategy for Partial Custom Tab Side-Sheet implementation. An instance of this
 * class should be owned by the CustomTabActivity.
 */
public class PartialCustomTabSideSheetStrategy extends PartialCustomTabBaseStrategy {
    private static final float MINIMAL_WIDTH_RATIO = 0.33f;
    private static final NoAnimator NO_ANIMATOR = new NoAnimator();

    private final @Px int mUnclampedInitialWidth;
    private final boolean mShowMaximizeButton;

    private boolean mIsMaximized;
    private int mDecorationType;
    private boolean mSlideDownAnimation; // Slide down to bottom when closing the sheet.
    private boolean mSheetOnRight;

    public PartialCustomTabSideSheetStrategy(Activity activity, @Px int initialWidth,
            CustomTabHeightStrategy.OnResizedCallback onResizedCallback,
            FullscreenManager fullscreenManager, boolean isTablet, boolean interactWithBackground,
            boolean showMaximizeButton, boolean startMaximized, int position, int slideInBehavior,
            PartialCustomTabHandleStrategyFactory handleStrategyFactory, int decorationType) {
        super(activity, onResizedCallback, fullscreenManager, isTablet, interactWithBackground,
                handleStrategyFactory);

        mUnclampedInitialWidth = initialWidth;
        mShowMaximizeButton = showMaximizeButton;
        mPositionUpdater = this::updatePosition;
        mIsMaximized = startMaximized;
        mDecorationType = decorationType;
        mSheetOnRight = isSheetOnRight(position);
        mSlideDownAnimation = slideInBehavior
                == CustomTabIntentDataProvider.ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_BOTTOM;
        setupAnimator();
    }

    /**
     * Return {@code true} if the sheet will be positioned on the right side of the window.
     * @param sheetPosition Sheet position from the launching Intent.
     */
    public static boolean isSheetOnRight(int sheetPosition) {
        // Take RTL and position extra from the Intent (start or end) into account to determine
        // the right side (right or left).
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        boolean isAtStart =
                sheetPosition == CustomTabIntentDataProvider.ACTIVITY_SIDE_SHEET_POSITION_START;
        return !(isRtl ^ isAtStart);
    }

    @Override
    public void onPostInflationStartup() {
        super.onPostInflationStartup();

        if (mIsMaximized) {
            mIsMaximized = false;
            toggleMaximize(/*animate=*/false);
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

    @Override
    public void handleCloseAnimation(Runnable finishRunnable) {
        if (mFinishRunnable != null) return;

        mFinishRunnable = finishRunnable;
        configureLayoutBeyondScreen(true);
        Window window = mActivity.getWindow();
        AnimatorUpdateListener closeAnimation;
        int start;
        int end;
        if (mSlideDownAnimation) {
            start = window.getAttributes().y;
            end = mVersionCompat.getDisplayHeight();
            closeAnimation = (animator) -> setWindowY((int) animator.getAnimatedValue());
        } else {
            start = window.getAttributes().x;
            end = mSheetOnRight ? mVersionCompat.getDisplayWidth() : -window.getAttributes().width;
            closeAnimation = (animator) -> setWindowX((int) animator.getAnimatedValue());
        }
        startAnimation(start, end, closeAnimation, this::onCloseAnimationEnd, true);
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
            toolbar.initSideSheetMaximizeButton(mIsMaximized, () -> toggleMaximize(true));
        }
        toolbar.setHandleStrategy(handleStrategy);
        updateDragBarVisibility(/*dragHandlebarVisibility*/ View.GONE);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    boolean toggleMaximize(boolean animate) {
        mIsMaximized = !mIsMaximized;
        if (mIsMaximized) {
            setTopMargins(0, 0);
        } else {
            updateShadowOffset();
        }

        int start;
        int end;
        AnimatorUpdateListener updateListener;
        Window window = mActivity.getWindow();
        if (mSheetOnRight) {
            // For smooth animation, make the window full-width first and then translate it
            // rather than resizing the window itself during the animation.
            configureLayoutBeyondScreen(true);
            setWindowWidth(mVersionCompat.getDisplayWidth());
            start = window.getAttributes().x;
            end = mIsMaximized ? 0 : mVersionCompat.getDisplayWidth() - mUnclampedInitialWidth;
            updateListener = (animator) -> setWindowX((int) animator.getAnimatedValue());
        } else {
            // When the sheet is positioned on left side, making full-width window first causes
            // a visual glitch. Resizing the window while animating may be the best we can do.
            start = window.getAttributes().width;
            end = mIsMaximized ? mVersionCompat.getDisplayWidth() : mUnclampedInitialWidth;
            updateListener = (animator) -> setWindowWidth((int) animator.getAnimatedValue());
        }
        startAnimation(start, end, updateListener, () -> onMaximizeEnd(animate), animate);
        return mIsMaximized;
    }

    private void setWindowX(int x) {
        var attrs = mActivity.getWindow().getAttributes();
        attrs.x = x;
        mActivity.getWindow().setAttributes(attrs);
    }

    private void setWindowY(int y) {
        var attrs = mActivity.getWindow().getAttributes();
        attrs.y = y;
        mActivity.getWindow().setAttributes(attrs);
    }

    private void setWindowWidth(int width) {
        var attrs = mActivity.getWindow().getAttributes();
        attrs.width = width;
        mActivity.getWindow().setAttributes(attrs);
    }

    private void onMaximizeEnd(boolean animate) {
        if (isMaximized()) {
            if (mSheetOnRight) configureLayoutBeyondScreen(false);
            notifyResized();
        } else {
            // System UI dimensions are not settled yet. Post the task.
            new Handler().post(() -> {
                if (mSheetOnRight) configureLayoutBeyondScreen(false);
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

    // ValueAnimator used when no animation should run. Simply lets the animator listener
    // receive only the final value to skip the animation effect.
    private static class NoAnimator extends ValueAnimator {
        private int mValue;

        private ValueAnimator withValue(int value) {
            mValue = value;
            return this;
        }

        @Override
        public Object getAnimatedValue() {
            return mValue;
        }
    }

    private void startAnimation(int start, int end, AnimatorUpdateListener updateListener,
            Runnable endRunnable, boolean animate) {
        if (animate) {
            startAnimation(start, end, updateListener, endRunnable);
        } else {
            updateListener.onAnimationUpdate(NO_ANIMATOR.withValue(end));
            endRunnable.run();
        }
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
        attrs.x = mSheetOnRight ? mVersionCompat.getDisplayWidth() - attrs.width : 0;
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

    @VisibleForTesting
    void setSlideDownAnimationForTesting(boolean slideDown) {
        mSlideDownAnimation = slideDown;
    }

    @VisibleForTesting
    void setSheetOnRightForTesting(boolean right) {
        mSheetOnRight = right;
    }
}
