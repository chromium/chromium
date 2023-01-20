// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.Px;

import org.chromium.base.SysUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.ui.base.ViewUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Base class for PCCT size strategies implementations.
 */
public abstract class PartialCustomTabBaseStrategy
        extends CustomTabHeightStrategy implements FullscreenManager.Observer {
    protected final Activity mActivity;
    protected final @Px int mUnclampedInitialHeight;
    protected final boolean mIsFixedHeight;
    protected final OnResizedCallback mOnResizedCallback;
    protected final FullscreenManager mFullscreenManager;
    protected final boolean mIsTablet;
    protected final boolean mInteractWithBackground;
    protected final int mCachedHandleHeight;
    protected final PartialCustomTabVersionCompat mVersionCompat;

    protected @Px int mDisplayHeight;
    protected @Px int mDisplayWidth;

    // ContentFrame + CoordinatorLayout - CompositorViewHolder
    //              + NavigationBar
    //              + Spinner
    // Not just CompositorViewHolder but also CoordinatorLayout is resized because many UI
    // components such as BottomSheet, InfoBar, Snackbar are child views of CoordinatorLayout,
    // which makes them appear correctly at the bottom.
    protected ViewGroup mCoordinatorLayout;
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

    protected int mShadowOffset;
    protected boolean mDrawOutlineShadow;

    @IntDef({PartialCustomTabType.NONE, PartialCustomTabType.BOTTOM_SHEET,
            PartialCustomTabType.SIDE_SHEET, PartialCustomTabType.FULL_SIZE})
    @Retention(RetentionPolicy.SOURCE)
    @interface PartialCustomTabType {
        int NONE = 0;
        int BOTTOM_SHEET = 1;
        int SIDE_SHEET = 2;
        int FULL_SIZE = 3;
    }

    public PartialCustomTabBaseStrategy(Activity activity, @Px int initialHeight,
            boolean isFixedHeight, OnResizedCallback onResizedCallback,
            FullscreenManager fullscreenManager, boolean isTablet, boolean interactWithBackground) {
        mActivity = activity;
        mUnclampedInitialHeight = initialHeight;
        mIsFixedHeight = isFixedHeight;
        mOnResizedCallback = onResizedCallback;
        mIsTablet = isTablet;
        mInteractWithBackground = interactWithBackground;

        mVersionCompat = PartialCustomTabVersionCompat.create(mActivity, this::updatePosition);
        mDisplayHeight = mVersionCompat.getDisplayHeight();
        mDisplayWidth = mVersionCompat.getDisplayWidth();

        mFullscreenManager = fullscreenManager;
        mFullscreenManager.addObserver(this);

        mDrawOutlineShadow = SysUtils.isLowEndDevice();
        mCachedHandleHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_handle_height);
    }

    @Override
    public void onPostInflationStartup() {
        mCoordinatorLayout = (ViewGroup) mActivity.findViewById(R.id.coordinator);
        // Elevate the main web contents area as high as the handle bar to have the shadow
        // effect look right.
        int ev = mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_elevation);
        mCoordinatorLayout.setElevation(ev);

        mPositionUpdater.run();
    }

    @Override
    public void destroy() {
        mFullscreenManager.removeObserver(this);
    }

    @Override
    public void onToolbarInitialized(
            View coordinatorView, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        mToolbarCoordinator = coordinatorView;
        mToolbarView = toolbar;
        mToolbarColor = toolbar.getBackground().getColor();

        roundCorners(coordinatorView, toolbar, toolbarCornerRadius);
    }

    @PartialCustomTabType
    public abstract int getStrategyType();

    public abstract void onShowSoftInput(Runnable softKeyboardRunnable);

    protected abstract void updatePosition();

    protected abstract int getHandleHeight();

    protected abstract boolean isFullHeight();

    protected abstract void adjustCornerRadius(GradientDrawable d, int radius);

    protected abstract void setTopMargins(int shadowOffset, int handleOffset);

    protected abstract boolean shouldHaveNoShadowOffset();

    protected boolean canInteractWithBackground() {
        return mInteractWithBackground;
    }

    protected void setCoordinatorLayoutHeight(int height) {
        ViewGroup.LayoutParams p = mCoordinatorLayout.getLayoutParams();
        p.height = height;
        mCoordinatorLayout.setLayoutParams(p);
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

    protected void updateDragBarVisibility(int dragHandlebarVisibility) {
        View dragBar = mActivity.findViewById(R.id.drag_bar);
        if (dragBar != null) dragBar.setVisibility(isFullHeight() ? View.GONE : View.VISIBLE);

        View dragHandlebar = mActivity.findViewById(R.id.drag_handlebar);
        if (dragHandlebar != null) {
            dragHandlebar.setVisibility(dragHandlebarVisibility);
        }
    }

    protected void updateShadowOffset() {
        if (isFullHeight() || mDrawOutlineShadow || shouldHaveNoShadowOffset()) {
            mShadowOffset = 0;
        } else {
            mShadowOffset = mActivity.getResources().getDimensionPixelSize(
                    R.dimen.custom_tabs_shadow_offset);
        }
        setTopMargins(mShadowOffset, getHandleHeight() + mShadowOffset);
        ViewUtils.requestLayout(
                mToolbarCoordinator, "PartialCustomTabBaseStrategy.updateShadowOffset");
    }

    protected void roundCorners(
            View coordinator, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        // Inflate the handle View.
        ViewStub handleViewStub = mActivity.findViewById(R.id.custom_tabs_handle_view_stub);
        handleViewStub.inflate();
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
        if (mDrawOutlineShadow) {
            int width = mActivity.getResources().getDimensionPixelSize(
                    R.dimen.custom_tabs_outline_width);
            cctBackground.setStroke(width, toolbar.getToolbarHairlineColor(mToolbarColor));

            // We need an inset to make the outline shadow visible.
            dragBar.setBackground(new InsetDrawable(dragBarBackground, width, width, width, 0));
        } else {
            dragBar.setBackground(dragBarBackground);
        }

        // Pass the drag bar portion to CustomTabToolbar for background color management.
        toolbar.setHandleBackground(dragBarBackground);

        // Having the transparent background is necessary for the shadow effect.
        mActivity.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
    }
}
