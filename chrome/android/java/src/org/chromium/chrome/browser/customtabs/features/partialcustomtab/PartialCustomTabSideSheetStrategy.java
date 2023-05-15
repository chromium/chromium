// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_LAYOUT_STATE_FULL_SCREEN;
import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_LAYOUT_STATE_SIDE_SHEET;
import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_LAYOUT_STATE_SIDE_SHEET_MAXIMIZED;
import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER;
import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_NONE;
import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW;
import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_NONE;

import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Handler;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ActivityLayoutState;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.ui.base.LocalizationUtils;

/**
 * CustomTabHeightStrategy for Partial Custom Tab Side-Sheet implementation. An instance of this
 * class should be owned by the CustomTabActivity.
 */
public class PartialCustomTabSideSheetStrategy extends PartialCustomTabBaseStrategy {
    private static final int WINDOW_WIDTH_EXPANDED_CUTOFF_DP = 840;
    private static final float MINIMAL_WIDTH_RATIO_EXPANDED = 0.33f;
    private static final float MINIMAL_WIDTH_RATIO_MEDIUM = 0.5f;
    private static final NoAnimator NO_ANIMATOR = new NoAnimator();

    private final @Px int mUnclampedInitialWidth;
    private final boolean mShowMaximizeButton;
    private final int mRoundedCornersPosition;

    private boolean mIsMaximized;
    private int mDecorationType;
    private boolean mSlideDownAnimation; // Slide down to bottom when closing the sheet.
    private boolean mSheetOnRight;

    public PartialCustomTabSideSheetStrategy(Activity activity, @Px int initialWidth,
            CustomTabHeightStrategy.OnResizedCallback onResizedCallback,
            CustomTabHeightStrategy.OnActivityLayoutCallback onActivityLayoutCallback,
            FullscreenManager fullscreenManager, boolean isTablet, boolean interactWithBackground,
            boolean showMaximizeButton, boolean startMaximized, int position, int slideInBehavior,
            PartialCustomTabHandleStrategyFactory handleStrategyFactory, int decorationType,
            int roundedCornersPosition) {
        super(activity, onResizedCallback, onActivityLayoutCallback, fullscreenManager, isTablet,
                interactWithBackground, handleStrategyFactory);

        mUnclampedInitialWidth = initialWidth;
        mShowMaximizeButton = showMaximizeButton;
        mPositionUpdater = this::updatePosition;
        mDecorationType = decorationType;
        mRoundedCornersPosition = roundedCornersPosition;
        mIsMaximized = startMaximized;
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
    @PartialCustomTabType
    public int getStrategyType() {
        return PartialCustomTabType.SIDE_SHEET;
    }

    @Override
    public void onShowSoftInput(Runnable softKeyboardRunnable) {
        softKeyboardRunnable.run();
    }

    @Override
    public boolean handleCloseAnimation(Runnable finishRunnable) {
        if (!super.handleCloseAnimation(finishRunnable)) return false;

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
        return true;
    }

    @Override
    public void onToolbarInitialized(
            View coordinatorView, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        super.onToolbarInitialized(coordinatorView, toolbar, toolbarCornerRadius);

        CustomTabToolbar.HandleStrategy handleStrategy =
                mHandleStrategyFactory.create(getStrategyType(), mActivity, this::isFullHeight,
                        () -> 0, null, this::handleCloseAnimation);
        if (mShowMaximizeButton) {
            toolbar.initSideSheetMaximizeButton(mIsMaximized, () -> toggleMaximize(true));
        }
        toolbar.setHandleStrategy(handleStrategy);
        updateDragBarVisibility(/*dragHandlebarVisibility*/ View.GONE);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    boolean toggleMaximize(boolean animate) {
        @ResizeType
        int resizeType =
                mIsMaximized ? ResizeType.MANUAL_MINIMIZATION : ResizeType.MANUAL_EXPANSION;
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.SideSheetResizeType", resizeType, ResizeType.COUNT);

        mIsMaximized = !mIsMaximized;
        if (mIsMaximized) {
            if (shouldDrawDividerLine()) resetCoordinatorLayoutInsets();
            setTopMargins(0, 0);
        } else {
            if (shouldDrawDividerLine()) drawDividerLine();
            updateShadowOffset();
        }

        AnimatorUpdateListener updateListener;
        WindowManager.LayoutParams windowLayout = mActivity.getWindow().getAttributes();
        int displayWidth = mVersionCompat.getDisplayWidth();
        int clampedInitialWidth = calculateWidth(mUnclampedInitialWidth);
        int start;
        int end;
        if (mSheetOnRight) {
            configureLayoutBeyondScreen(true);
            setWindowWidth(displayWidth);
            int xOffset = mVersionCompat.getXOffset();
            start = windowLayout.x;
            end = (mIsMaximized ? 0 : displayWidth - clampedInitialWidth) + xOffset;
            updateListener = (anim) -> setWindowX((int) anim.getAnimatedValue());
        } else {
            start = windowLayout.width;
            end = mIsMaximized ? displayWidth : clampedInitialWidth;
            updateListener = (anim) -> setWindowWidth((int) anim.getAnimatedValue());
        }
        // Keep the WebContents invisible during the animation to hide the jerky visual artifacts
        // of the contents due to resizing.
        setContentVisible(false);
        startAnimation(start, end, updateListener, this::onMaximizeEnd, animate);
        return mIsMaximized;
    }

    private void setContentVisible(boolean visible) {
        View content = (ViewGroup) mActivity.findViewById(R.id.compositor_view_holder);
        if (visible) {
            // Set a slight delay in restoring the view to hide the visual glitch caused by
            // the resized web contents.
            new Handler().postDelayed(() -> content.setVisibility(View.VISIBLE), 20);
        } else {
            content.setVisibility(View.INVISIBLE);
        }
    }

    private void onMaximizeEnd() {
        if (isMaximized()) {
            if (mSheetOnRight) {
                configureLayoutBeyondScreen(false);
                maybeResetTalkbackFocus();
            }
            maybeInvokeResizeCallback();
            setContentVisible(true);
        } else {
            // System UI dimensions are not settled yet. Post the task.
            new Handler().post(() -> {
                if (mSheetOnRight) {
                    configureLayoutBeyondScreen(false);
                    maybeResetTalkbackFocus();
                }
                initializeSize();
                maybeInvokeResizeCallback();
            });
        }
    }

    private void maybeResetTalkbackFocus() {
        var am = (AccessibilityManager) mActivity.getSystemService(Context.ACCESSIBILITY_SERVICE);
        if (am != null && am.isTouchExplorationEnabled()) {
            // After resizing the view, notify the window state change to let the talkback
            // focus navigation work as before.
            mToolbarView.sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
            new Handler().postDelayed(() -> {
                // Move the talkback focus from the leftmost button back to maximize button. This
                // happens when double-tapping on the button causes the sheet to be resized to full
                // width in talkback mode. Some delay is required for this to work as expected.
                var maximizeButton = mToolbarView.findViewById(R.id.custom_tabs_sidepanel_maximize);
                maximizeButton.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
            }, 200);
        }
    }

    @Override
    protected boolean isMaximized() {
        return mIsMaximized;
    }

    @Override
    protected int getHandleHeight() {
        return mRoundedCornersPosition == ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_NONE
                ? 0
                : mToolbarCornerRadius;
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
        maybeInvokeResizeCallback();
    }

    @Override
    protected void setTopMargins(int shadowOffset, int handleOffset) {
        int leftMargin = mSheetOnRight ? shadowOffset : 0;
        int rightMargin = !mSheetOnRight ? shadowOffset : 0;
        float elevation = calculateElevation();
        ViewGroup coordinatorLayout = (ViewGroup) mActivity.findViewById(R.id.coordinator);
        coordinatorLayout.setElevation(elevation);
        View handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);
        if (handleView != null) {
            handleView.setElevation(elevation);
        }

        if (handleView != null) {
            ViewGroup.MarginLayoutParams lp =
                    (ViewGroup.MarginLayoutParams) handleView.getLayoutParams();
            lp.setMargins(leftMargin, 0, rightMargin, 0);
        }

        // Make enough room for the handle View.
        int topOffset = Math.max(handleOffset - shadowOffset, 0);
        ViewGroup.MarginLayoutParams mlp =
                (ViewGroup.MarginLayoutParams) mToolbarCoordinator.getLayoutParams();
        mlp.setMargins(leftMargin, topOffset, rightMargin, 0);
    }

    @Override
    protected boolean shouldHaveNoShadowOffset() {
        // We remove shadow in maximized mode.
        return isMaximized()
                || calculateWidth(mUnclampedInitialWidth) == mVersionCompat.getDisplayWidth()
                || mDecorationType == ACTIVITY_SIDE_SHEET_DECORATION_TYPE_NONE
                || mDecorationType == ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER;
    }

    @Override
    protected void adjustCornerRadius(GradientDrawable d, int radius) {
        if (mRoundedCornersPosition == ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_NONE) {
            radius = 0;
        }

        int topLeftCornerRadius = mSheetOnRight ? radius : 0;
        int topRightCornerRadius = !mSheetOnRight ? radius : 0;

        View handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);
        View dragBar = handleView.findViewById(R.id.drag_bar);
        ViewGroup.LayoutParams dragBarLayoutParams = dragBar.getLayoutParams();
        dragBarLayoutParams.height = radius;
        dragBar.setLayoutParams(dragBarLayoutParams);

        d.mutate();
        // Inner top rounded corner (depends on side sheet positioning)
        d.setCornerRadii(new float[] {topLeftCornerRadius, topLeftCornerRadius,
                topRightCornerRadius, topRightCornerRadius, 0, 0, 0, 0});
    }

    @Override
    protected void cleanupImeStateCallback() {
        mVersionCompat.setImeStateCallback(null);
    }

    @Override
    @ActivityLayoutState
    protected int getActivityLayoutState() {
        if (isFullscreen()) {
            return ACTIVITY_LAYOUT_STATE_FULL_SCREEN;
        } else if (isMaximized()) {
            return ACTIVITY_LAYOUT_STATE_SIDE_SHEET_MAXIMIZED;
        } else {
            return ACTIVITY_LAYOUT_STATE_SIDE_SHEET;
        }
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

        positionOnWindow();
        setCoordinatorLayoutHeight(MATCH_PARENT);

        updateDragBarVisibility(/*dragHandlebarVisibility*/ View.GONE);

        if (mIsMaximized) {
            mIsMaximized = false;
            toggleMaximize(/*animate=*/false);
        }
        setContentVisible(true);
    }

    private void positionOnWindow() {
        WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
        attrs.height = mDisplayHeight - mStatusbarHeight - mNavbarHeight;
        attrs.width = calculateWidth(mUnclampedInitialWidth);

        attrs.y = mStatusbarHeight;
        attrs.x = (mSheetOnRight ? mVersionCompat.getDisplayWidth() - attrs.width : 0)
                + mVersionCompat.getXOffset();
        attrs.gravity = Gravity.TOP | Gravity.START;
        mActivity.getWindow().setAttributes(attrs);
    }

    private int calculateWidth(int unclampedWidth) {
        int displayWidth = mVersionCompat.getDisplayWidth();
        int displayWidthDp = mVersionCompat.getDisplayWidthDp();
        float minWidthRatio = displayWidthDp < WINDOW_WIDTH_EXPANDED_CUTOFF_DP
                ? MINIMAL_WIDTH_RATIO_MEDIUM
                : MINIMAL_WIDTH_RATIO_EXPANDED;
        return MathUtils.clamp(unclampedWidth, displayWidth, (int) (displayWidth * minWidthRatio));
    }

    private float calculateElevation() {
        int width = calculateWidth(mUnclampedInitialWidth);
        int displayWidth = mVersionCompat.getDisplayWidth();

        // Shadows grow depending on size of activity, which is undesirable for this purpose
        // To keep the shadow size consistent, we stratify the elevation according to the width.
        if (width >= (displayWidth * 3 / 4)) {
            // Side Sheet > 75% of screen
            return 5;
        } else if (width >= displayWidth / 2) {
            // Side Sheet between 75% and 50% of screen
            return 7;
        } else if (width > displayWidth / 3) {
            // Side Sheet between 33% and 50% of screen
            return 9;
        } else {
            // 33% min-width Side Sheet
            return 11;
        }
    }

    @Override
    protected void drawDividerLine() {
        int width =
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_outline_width);
        int leftDividerInset = mSheetOnRight ? width : 0;
        int rightDividerInset = !mSheetOnRight ? width : 0;

        drawDividerLineBase(leftDividerInset, 0, rightDividerInset);
    }

    @Override
    protected boolean shouldDrawDividerLine() {
        boolean notMaxWidthSideSheet =
                calculateWidth(mUnclampedInitialWidth) != mVersionCompat.getDisplayWidth();
        // Elevation shadows are only rendered properly on devices >= Android Q
        return notMaxWidthSideSheet
                && (SysUtils.isLowEndDevice()
                        || mDecorationType == ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER
                        || (mDecorationType == ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW
                                && Build.VERSION.SDK_INT < Build.VERSION_CODES.Q));
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
