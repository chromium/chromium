// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_LAYOUT_STATE_FULL_SCREEN;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.os.Handler;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.Window;
import android.view.WindowManager;
import android.view.animation.AccelerateInterpolator;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ActivityLayoutState;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.ViewUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.BooleanSupplier;

/**
 * Base class for PCCT size strategies implementations.
 */
public abstract class PartialCustomTabBaseStrategy
        extends CustomTabHeightStrategy implements FullscreenManager.Observer {
    protected final Activity mActivity;
    protected final OnResizedCallback mOnResizedCallback;
    protected final OnActivityLayoutCallback mOnActivityLayoutCallback;
    protected final FullscreenManager mFullscreenManager;
    protected final boolean mIsTablet;
    protected final boolean mInteractWithBackground;
    protected final int mCachedHandleHeight;
    protected final PartialCustomTabVersionCompat mVersionCompat;

    protected @Px int mDisplayHeight;
    protected @Px int mDisplayWidth;

    protected Runnable mPositionUpdater;

    // Runnable finishing the activity after the exit animation. Non-null when PCCT is closing.
    @Nullable
    protected Runnable mFinishRunnable;

    protected @Px int mNavbarHeight;
    protected @Px int mStatusbarHeight;

    // The current height/width used to trigger onResizedCallback when it is resized.
    protected int mHeight;
    protected int mWidth;

    protected View mToolbarView;
    protected View mToolbarCoordinator;
    protected int mToolbarColor;
    protected PartialCustomTabHandleStrategyFactory mHandleStrategyFactory;

    protected int mShadowOffset;

    // Note: Do not use anywhere except in |onConfigurationChanged| as it might not be up-to-date.
    protected boolean mIsInMultiWindowMode;
    protected int mOrientation;

    private ValueAnimator mAnimator;
    private Runnable mPostAnimationRunnable;

    private BooleanSupplier mIsFullscreen;

    @IntDef({PartialCustomTabType.NONE, PartialCustomTabType.BOTTOM_SHEET,
            PartialCustomTabType.SIDE_SHEET, PartialCustomTabType.FULL_SIZE})
    @Retention(RetentionPolicy.SOURCE)
    @interface PartialCustomTabType {
        int NONE = 0;
        int BOTTOM_SHEET = 1;
        int SIDE_SHEET = 2;
        int FULL_SIZE = 3;
    }

    public PartialCustomTabBaseStrategy(Activity activity, OnResizedCallback onResizedCallback,
            OnActivityLayoutCallback onActivityLayoutCallback, FullscreenManager fullscreenManager,
            boolean isTablet, boolean interactWithBackground,
            PartialCustomTabHandleStrategyFactory handleStrategyFactory) {
        mActivity = activity;
        mOnResizedCallback = onResizedCallback;
        mOnActivityLayoutCallback = onActivityLayoutCallback;
        mIsTablet = isTablet;
        mInteractWithBackground = interactWithBackground;

        mVersionCompat = PartialCustomTabVersionCompat.create(mActivity, this::updatePosition);
        mDisplayHeight = mVersionCompat.getDisplayHeight();
        mDisplayWidth = mVersionCompat.getDisplayWidth();

        mFullscreenManager = fullscreenManager;
        mFullscreenManager.addObserver(this);

        mCachedHandleHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_handle_height);

        mOrientation = mActivity.getResources().getConfiguration().orientation;
        mIsInMultiWindowMode = MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);

        mHandleStrategyFactory = handleStrategyFactory;
        mIsFullscreen = fullscreenManager::getPersistentFullscreenMode;

        // Initialize size info used for resize callback to skip the very first one that settles
        // down to the initial height/width.
        mHeight = MATCH_PARENT;
        mWidth = MATCH_PARENT;
    }

    @Override
    public void onPostInflationStartup() {
        // Elevate the main web contents area as high as the handle bar to have the shadow
        // effect look right.
        int ev = mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_elevation);
        getCoordinatorLayout().setElevation(ev);

        mPositionUpdater.run();
    }

    @Override
    public void destroy() {
        mFullscreenManager.removeObserver(this);
    }

    @Override
    public void onToolbarInitialized(
            View coordinatorView, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        setToolbar(coordinatorView, toolbar);
        roundCorners(toolbar, toolbarCornerRadius);
    }

    public void setToolbar(View toolbarCoordinator, CustomTabToolbar toolbar) {
        mToolbarCoordinator = toolbarCoordinator;
        mToolbarView = toolbar;
        mToolbarColor = toolbar.getBackground().getColor();
    }

    public void onShowSoftInput(Runnable softKeyboardRunnable) {
        softKeyboardRunnable.run();
    }

    public void onConfigurationChanged(int orientation) {
        boolean isInMultiWindow = MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);
        int displayHeight = mVersionCompat.getDisplayHeight();
        int displayWidth = mVersionCompat.getDisplayWidth();

        if (isInMultiWindow != mIsInMultiWindowMode || orientation != mOrientation
                || displayHeight != mDisplayHeight || displayWidth != mDisplayWidth) {
            mIsInMultiWindowMode = isInMultiWindow;
            mOrientation = orientation;
            mDisplayHeight = displayHeight;
            mDisplayWidth = displayWidth;
            if (isFullHeight()) {
                // We should update CCT position before Window#FLAG_LAYOUT_NO_LIMITS is set,
                // otherwise it is not possible to get the correct content height.
                configureLayoutBeyondScreen(false);

                // Clean up the state initiated by IME so the height can be restored when
                // rotating back to non-full-height mode later.
                cleanupImeStateCallback();
            }
            mPositionUpdater.run();
        }
    }

    // FullscreenManager.Observer implementation

    @Override
    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
        WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
        attrs.height = MATCH_PARENT;
        attrs.width = MATCH_PARENT;
        attrs.y = 0;
        attrs.x = 0;
        mActivity.getWindow().setAttributes(attrs);
        updateShadowOffset();
        maybeInvokeResizeCallback();
    }

    @Override
    public void onExitFullscreen(Tab tab) {
        // |mNavbarHeight| is zero now. Post the task instead.
        new Handler().post(() -> {
            initializeSize();
            updateShadowOffset();
            maybeInvokeResizeCallback();
        });
    }

    protected ViewGroup getCoordinatorLayout() {
        // ContentFrame + CoordinatorLayout - CompositorViewHolder
        //              + NavigationBar
        //              + Spinner
        // Not just CompositorViewHolder but also CoordinatorLayout is resized because many UI
        // components such as BottomSheet, InfoBar, Snackbar are child views of CoordinatorLayout,
        // which makes them appear correctly at the bottom.
        return mActivity.findViewById(R.id.coordinator);
    }

    protected void maybeInvokeResizeCallback() {
        WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();

        if (ChromeFeatureList.sCctResizableSideSheet.isEnabled()) {
            // onActivityLayout should be called before onResized and only when the PCCT is created
            // or its size has changed.
            if (mHeight != attrs.height || mWidth != attrs.width) {
                invokeActivityLayoutCallback();
            }
        }

        if (isFullHeight() || isFullscreen()) {
            mOnResizedCallback.onResized(mDisplayHeight, mDisplayWidth);
            mHeight = mDisplayHeight;
            mWidth = mDisplayWidth;
        } else {
            if ((mHeight != attrs.height && mHeight > 0) || (mWidth != attrs.width && mWidth > 0)) {
                mOnResizedCallback.onResized(attrs.height, attrs.width);
            }
            mHeight = attrs.height;
            mWidth = attrs.width;
        }
    }

    protected void invokeActivityLayoutCallback() {
        @ActivityLayoutState
        int activityLayoutState = getActivityLayoutState();

        // If we are in full screen then we manually need to set the values as we are using
        // MATCH_PARENT which has the value -1.
        int left = 0;
        int top = 0;
        int right = mDisplayWidth;
        int bottom = mDisplayHeight;
        if (activityLayoutState != ACTIVITY_LAYOUT_STATE_FULL_SCREEN) {
            WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
            left = attrs.x;
            top = attrs.y;
            right = left + attrs.width;
            bottom = top + attrs.height;
        }

        mOnActivityLayoutCallback.onActivityLayout(left, top, right, bottom, activityLayoutState);
    }

    @PartialCustomTabType
    public abstract int getStrategyType();

    @ActivityLayoutState
    protected abstract int getActivityLayoutState();

    protected abstract void updatePosition();

    protected abstract int getHandleHeight();

    protected abstract boolean isFullHeight();

    protected abstract void cleanupImeStateCallback();

    protected abstract void adjustCornerRadius(GradientDrawable d, int radius);

    protected abstract void setTopMargins(int shadowOffset, int handleOffset);

    protected abstract boolean shouldHaveNoShadowOffset();

    protected abstract boolean isMaximized();

    protected abstract void drawDividerLine(CustomTabToolbar toolbar);

    protected abstract boolean shouldDrawDividerLine();

    protected boolean canInteractWithBackground() {
        return mInteractWithBackground;
    }

    protected void setCoordinatorLayoutHeight(int height) {
        ViewGroup coordinator = getCoordinatorLayout();
        ViewGroup.LayoutParams p = coordinator.getLayoutParams();
        p.height = height;
        coordinator.setLayoutParams(p);
    }

    protected void initializeHeight() {
        Window window = mActivity.getWindow();
        if (canInteractWithBackground()) {
            window.addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
            window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        } else {
            window.clearFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
            window.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            window.setDimAmount(0.6f);
        }

        mNavbarHeight = mVersionCompat.getNavbarHeight();
        mStatusbarHeight = mVersionCompat.getStatusbarHeight();
    }

    protected void initializeSize() {}

    protected void updateDragBarVisibility(int dragHandlebarVisibility) {
        View dragBar = mActivity.findViewById(R.id.drag_bar);
        if (dragBar != null) dragBar.setVisibility(isFullHeight() ? View.GONE : View.VISIBLE);

        View dragHandlebar = mActivity.findViewById(R.id.drag_handlebar);
        if (dragHandlebar != null) {
            dragHandlebar.setVisibility(dragHandlebarVisibility);
        }
    }

    protected void updateShadowOffset() {
        if (isFullHeight() || isFullscreen() || shouldHaveNoShadowOffset()
                || shouldDrawDividerLine()) {
            mShadowOffset = 0;
        } else {
            mShadowOffset = mActivity.getResources().getDimensionPixelSize(
                    R.dimen.custom_tabs_shadow_offset);
        }
        setTopMargins(mShadowOffset, getHandleHeight() + mShadowOffset);
        ViewUtils.requestLayout(
                mToolbarCoordinator, "PartialCustomTabBaseStrategy.updateShadowOffset");
    }

    protected void roundCorners(CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        // Inflate the handle View.
        ViewStub handleViewStub = mActivity.findViewById(R.id.custom_tabs_handle_view_stub);
        // If the handle view has already been inflated then the stub will be null. This can happen,
        // for example, when we are transitioning from side-sheet to bottom-sheet and we need to
        // apply the round corners logic again.
        if (handleViewStub != null) {
            handleViewStub.inflate();
        }

        getCoordinatorLayout().setElevation(
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_elevation));
        View handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);
        handleView.setElevation(
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_elevation));
        updateShadowOffset();

        GradientDrawable cctBackground = (GradientDrawable) handleView.getBackground();
        adjustCornerRadius(cctBackground, toolbarCornerRadius);
        handleView.setBackground(cctBackground);

        // Inner frame |R.id.drag_bar| is used for setting background color to match that of
        // the toolbar. Outer frame |R.id.custom_tabs_handle_view| is not suitable since it
        // covers the entire client area for rendering outline shadow around the CCT.
        View dragBar = handleView.findViewById(R.id.drag_bar);
        GradientDrawable dragBarBackground = (GradientDrawable) dragBar.getBackground();
        adjustCornerRadius(dragBarBackground, toolbarCornerRadius);

        if (shouldDrawDividerLine()) {
            drawDividerLine(toolbar);
        } else {
            dragBar.setBackground(dragBarBackground);
        }

        // Pass the drag bar portion to CustomTabToolbar for background color management.
        toolbar.setHandleBackground(dragBarBackground);

        // Having the transparent background is necessary for the shadow effect.
        mActivity.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
    }

    protected void drawDividerLine(
            int leftInset, int topInset, int rightInset, CustomTabToolbar toolbar) {
        View handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);
        View dragBar = handleView.findViewById(R.id.drag_bar);
        GradientDrawable cctBackground = (GradientDrawable) handleView.getBackground();
        GradientDrawable dragBarBackground = (GradientDrawable) dragBar.getBackground();
        int width =
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_outline_width);

        cctBackground.setStroke(width, toolbar.getToolbarHairlineColor(mToolbarColor));

        // We need an inset to make the outline shadow visible.
        dragBar.setBackground(new InsetDrawable(dragBarBackground, width, width, width, 0));
        getCoordinatorLayout().setBackground(
                new InsetDrawable(cctBackground, leftInset, topInset, rightInset, 0));
    }

    protected boolean isFullscreen() {
        return mIsFullscreen.getAsBoolean();
    }

    protected void setupAnimator() {
        mAnimator = new ValueAnimator();
        mAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {}
            @Override
            public void onAnimationEnd(Animator animation) {
                mPostAnimationRunnable.run();
            }
        });

        int animTime = mActivity.getResources().getInteger(android.R.integer.config_mediumAnimTime);
        mAnimator.setDuration(animTime);
        mAnimator.setInterpolator(new AccelerateInterpolator());
    }

    protected void startAnimation(int start, int end,
            ValueAnimator.AnimatorUpdateListener updateListener, Runnable endRunnable) {
        mAnimator.removeAllUpdateListeners();
        mAnimator.addUpdateListener(updateListener);
        mPostAnimationRunnable = endRunnable;
        mAnimator.setIntValues(start, end);
        mAnimator.start();
    }

    @Override
    public void handleCloseAnimation(Runnable finishRunnable) {
        if (mFinishRunnable != null) return;

        mFinishRunnable = finishRunnable;
        configureLayoutBeyondScreen(true);
        AnimatorUpdateListener updater = animator -> setWindowY((int) animator.getAnimatedValue());
        int start = mActivity.getWindow().getAttributes().y;
        startAnimation(start, mHeight, updater, this::onCloseAnimationEnd);
    }

    protected void configureLayoutBeyondScreen(boolean enable) {
        Window window = mActivity.getWindow();
        if (enable) {
            window.addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        } else {
            window.clearFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        }
    }

    protected void setWindowX(int x) {
        var attrs = mActivity.getWindow().getAttributes();
        attrs.x = x;
        mActivity.getWindow().setAttributes(attrs);
    }

    protected void setWindowY(int y) {
        var attrs = mActivity.getWindow().getAttributes();
        attrs.y = y;
        mActivity.getWindow().setAttributes(attrs);
    }

    protected void setWindowWidth(int width) {
        var attrs = mActivity.getWindow().getAttributes();
        attrs.width = width;
        mActivity.getWindow().setAttributes(attrs);
    }

    protected void onCloseAnimationEnd() {
        assert mFinishRunnable != null;

        mFinishRunnable.run();
        mFinishRunnable = null;
    }

    @VisibleForTesting
    void setMockViewForTesting(ViewGroup coordinatorLayout, View toolbar, View toolbarCoordinator) {
        mPositionUpdater = this::updatePosition;
        mToolbarView = toolbar;
        mToolbarCoordinator = toolbarCoordinator;

        onPostInflationStartup();
    }

    @VisibleForTesting
    void setFullscreenSupplierForTesting(BooleanSupplier fullscreen) {
        mIsFullscreen = fullscreen;
    }

    @VisibleForTesting
    int getTopMarginForTesting() {
        var mlp = (ViewGroup.MarginLayoutParams) mToolbarCoordinator.getLayoutParams();
        return mlp.topMargin;
    }

    @VisibleForTesting
    int getShadowOffsetForTesting() {
        return mShadowOffset;
    }
}
