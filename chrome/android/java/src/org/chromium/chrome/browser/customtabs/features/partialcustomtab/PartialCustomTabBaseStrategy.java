// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_FULL_SCREEN;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
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
import androidx.annotation.StringRes;
import androidx.browser.customtabs.CustomTabsCallback;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.ViewUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.BooleanSupplier;

/** Base class for PCCT size strategies implementations. */
public abstract class PartialCustomTabBaseStrategy extends CustomTabHeightStrategy
        implements FullscreenManager.Observer {
    private static boolean sDeviceSpecLogged;

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
    @Nullable protected Runnable mFinishRunnable;

    protected @Px int mNavbarHeight;
    protected @Px int mStatusbarHeight;

    // The current height/width used to trigger onResizedCallback when it is resized.
    protected int mHeight;
    protected int mWidth;

    protected CustomTabToolbar mToolbarView;
    protected View mToolbarCoordinator;
    protected int mToolbarColor;
    protected int mToolbarCornerRadius;
    protected PartialCustomTabHandleStrategyFactory mHandleStrategyFactory;

    protected int mShadowOffset;

    // Note: Do not use anywhere except in |onConfigurationChanged| as it might not be up-to-date.
    protected boolean mIsInMultiWindowMode;
    protected int mOrientation;

    private final Callback<Integer> mVisibilityChangeObserver =
            this::onToolbarContainerVisibilityChange;

    private ValueAnimator mAnimator;
    private Runnable mPostAnimationRunnable;

    private BooleanSupplier mIsFullscreenForTesting;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // This should be kept in sync with the definition |CustomTabsPartialCustomTabType|
    // in tools/metrics/histograms/enums.xml.
    @IntDef({
        PartialCustomTabType.NONE,
        PartialCustomTabType.BOTTOM_SHEET,
        PartialCustomTabType.SIDE_SHEET,
        PartialCustomTabType.FULL_SIZE,
        PartialCustomTabType.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PartialCustomTabType {
        int NONE = 0;
        int BOTTOM_SHEET = 1;
        int SIDE_SHEET = 2;
        int FULL_SIZE = 3;

        // Number of elements in the enum
        int COUNT = 4;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // This should be kept in sync with the definition |PcctDeviceSpec|
    // in tools/metrics/histograms/enums.xml.
    @IntDef({
        DeviceSpec.LOWEND_NOPIP,
        DeviceSpec.LOWEND_PIP,
        DeviceSpec.HIGHEND_NOPIP,
        DeviceSpec.HIGHEND_PIP
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface DeviceSpec {
        int LOWEND_NOPIP = 0;
        int LOWEND_PIP = 1;
        int HIGHEND_NOPIP = 2;
        int HIGHEND_PIP = 3;

        // Number of elements in the enum
        int COUNT = 4;
    }

    public PartialCustomTabBaseStrategy(
            Activity activity,
            BrowserServicesIntentDataProvider intentData,
            OnResizedCallback onResizedCallback,
            OnActivityLayoutCallback onActivityLayoutCallback,
            FullscreenManager fullscreenManager,
            boolean isTablet,
            PartialCustomTabHandleStrategyFactory handleStrategyFactory) {
        mActivity = activity;
        mOnResizedCallback = onResizedCallback;
        mOnActivityLayoutCallback = onActivityLayoutCallback;
        mIsTablet = isTablet;
        mInteractWithBackground = intentData.canInteractWithBackground();

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

        // Initialize size info used for resize callback to skip the very first one that settles
        // down to the initial height/width.
        mHeight = MATCH_PARENT;
        mWidth = MATCH_PARENT;

        if (!sDeviceSpecLogged) {
            logDeviceSpecForPcct(activity);
            sDeviceSpecLogged = true;
        }
    }

    static void logDeviceSpecForPcct(Context context) {
        var pm = context.getPackageManager();
        var am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        boolean pip = pm.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE);
        boolean lowEnd = am.isLowRamDevice();
        @DeviceSpec int spec;
        if (lowEnd && !pip) {
            spec = DeviceSpec.LOWEND_NOPIP;
        } else if (lowEnd && pip) {
            spec = DeviceSpec.LOWEND_PIP;
        } else if (!lowEnd && !pip) {
            spec = DeviceSpec.HIGHEND_NOPIP;
        } else {
            spec = DeviceSpec.HIGHEND_PIP;
        }
        RecordHistogram.recordEnumeratedHistogram("CustomTabs.DeviceSpec", spec, DeviceSpec.COUNT);
    }

    @Override
    public void onPostInflationStartup() {
        // Elevate the main web contents area as high as the handle bar to have the shadow
        // effect look right.
        View coordinatorLayout = getCoordinatorLayout();
        coordinatorLayout.setElevation(getCustomTabsElevation());

        mPositionUpdater.run();

        // Set the window title so the type announcement is made, only when CCT is first launched.
        if (!coordinatorLayout.isAttachedToWindow()) setWindowTitleForTouchExploration();
    }

    private void setWindowTitleForTouchExploration() {
        View coordinatorLayout = getCoordinatorLayout();
        var attachStateListener =
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View v) {
                        Window window = mActivity.getWindow();
                        window.setTitle(mActivity.getResources().getString(getTypeStringId()));
                        coordinatorLayout.removeOnAttachStateChangeListener(this);
                    }

                    @Override
                    public void onViewDetachedFromWindow(View v) {}
                };
        coordinatorLayout.addOnAttachStateChangeListener(attachStateListener);
    }

    @Override
    public void destroy() {
        mFullscreenManager.removeObserver(this);
        cleanupImeStateCallback();
        if (mToolbarView != null) {
            mToolbarView.removeContainerVisibilityChangeObserver(mVisibilityChangeObserver);
        }
    }

    @Override
    public void onToolbarInitialized(
            View coordinatorView, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        // The radius should not be bigger than the handle view default height of 16dp.
        mToolbarCornerRadius = Math.min(toolbarCornerRadius, mCachedHandleHeight);
        setToolbar(coordinatorView, toolbar);
        roundCorners(toolbar, mToolbarCornerRadius);
    }

    public void setToolbar(View toolbarCoordinator, CustomTabToolbar toolbar) {
        if (mToolbarView != null) {
            mToolbarView.removeContainerVisibilityChangeObserver(mVisibilityChangeObserver);
        }

        mToolbarCoordinator = toolbarCoordinator;
        mToolbarView = toolbar;
        mToolbarColor = toolbar.getBackground().getColor();

        mToolbarView.addContainerVisibilityChangeObserver(mVisibilityChangeObserver);
    }

    public void onShowSoftInput(Runnable softKeyboardRunnable) {
        softKeyboardRunnable.run();
    }

    public void onConfigurationChanged(int orientation) {
        boolean isInMultiWindow = MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);
        int displayHeight = mVersionCompat.getDisplayHeight();
        int displayWidth = mVersionCompat.getDisplayWidth();

        if (isInMultiWindow != mIsInMultiWindowMode
                || orientation != mOrientation
                || displayHeight != mDisplayHeight
                || displayWidth != mDisplayWidth) {
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
        if (shouldDrawDividerLine()) resetCoordinatorLayoutInsets();
        setTopMargins(0, 0);
        maybeInvokeResizeCallback();
    }

    @Override
    public void onExitFullscreen(Tab tab) {
        // |mNavbarHeight| is zero now. Post the task instead.
        new Handler()
                .post(
                        () -> {
                            initializeSize();
                            if (shouldDrawDividerLine() && !isMaximized()) drawDividerLine();
                            if (!isMaximized()) updateShadowOffset();
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

        // onActivityLayout should be called before onResized and only when the PCCT is created
        // or its size has changed.
        if (mHeight != attrs.height || mWidth != attrs.width) {
            invokeActivityLayoutCallback();
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
        @CustomTabsCallback.ActivityLayoutState int activityLayoutState = getActivityLayoutState();

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

    public abstract @PartialCustomTabType int getStrategyType();

    public abstract @StringRes int getTypeStringId();

    protected abstract @CustomTabsCallback.ActivityLayoutState int getActivityLayoutState();

    protected abstract void updatePosition();

    protected abstract int getHandleHeight();

    protected abstract boolean isFullHeight();

    protected abstract void cleanupImeStateCallback();

    protected abstract void adjustCornerRadius(GradientDrawable d, int radius);

    protected abstract void setTopMargins(int shadowOffset, int handleOffset);

    protected abstract boolean shouldHaveNoShadowOffset();

    protected abstract boolean isMaximized();

    protected abstract void drawDividerLine();

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

        View dragHandlebar = mActivity.findViewById(R.id.drag_handle);
        if (dragHandlebar != null) {
            dragHandlebar.setVisibility(dragHandlebarVisibility);
        }
    }

    protected void updateShadowOffset() {
        if (isFullHeight()
                || isFullscreen()
                || shouldHaveNoShadowOffset()
                || shouldDrawDividerLine()) {
            mShadowOffset = 0;
        } else {
            mShadowOffset =
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.custom_tabs_shadow_offset);
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

        getCoordinatorLayout().setElevation(getCustomTabsElevation());
        View handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);
        handleView.setElevation(getCustomTabsElevation());
        updateShadowOffset();
        GradientDrawable cctBackground = (GradientDrawable) handleView.getBackground();
        adjustCornerRadius(cctBackground, toolbarCornerRadius);
        handleView.setBackground(cctBackground);

        // Inner frame |R.id.drag_bar| is used for setting background color to match that of
        // the toolbar. Outer frame |R.id.custom_tabs_handle_view| is not suitable since it
        // covers the entire client area for rendering outline shadow around the CCT.
        View dragBar = handleView.findViewById(R.id.drag_bar);
        GradientDrawable dragBarBackground = getDragBarBackground();
        adjustCornerRadius(dragBarBackground, toolbarCornerRadius);
        if (dragBar.getBackground() instanceof InsetDrawable) resetCoordinatorLayoutInsets();

        if (shouldDrawDividerLine()) {
            drawDividerLine();
        } else {
            dragBar.setBackground(dragBarBackground);
        }

        // Pass the drag bar portion to CustomTabToolbar for background color management.
        toolbar.setHandleBackground(dragBarBackground);

        // Having the transparent background is necessary for the shadow effect.
        mActivity.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
    }

    protected void drawDividerLineBase(int leftInset, int topInset, int rightInset) {
        View handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);
        View dragBar = handleView.findViewById(R.id.drag_bar);
        GradientDrawable cctBackground = (GradientDrawable) handleView.getBackground();
        GradientDrawable dragBarBackground = getDragBarBackground();
        int width =
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_outline_width);

        cctBackground.setStroke(width, SemanticColorUtils.getDividerLineBgColor(mActivity));

        // We need an inset to make the outline shadow visible.
        dragBar.setBackground(
                new InsetDrawable(dragBarBackground, leftInset, topInset, rightInset, 0));
        getCoordinatorLayout()
                .setBackground(new InsetDrawable(cctBackground, leftInset, 0, rightInset, 0));
    }

    protected GradientDrawable getDragBarBackground() {
        View dragBar = mActivity.findViewById(R.id.drag_bar);
        // Check if the current dragBar background is the InsetDrawable used in conjunction with
        // the divider line
        if (dragBar.getBackground() instanceof InsetDrawable insetDrawable) {
            return (GradientDrawable) insetDrawable.getDrawable();
        } else {
            return (GradientDrawable) dragBar.getBackground();
        }
    }

    protected void resetCoordinatorLayoutInsets() {
        ViewGroup coordinatorLayout = getCoordinatorLayout();
        Drawable backgroundDrawable = coordinatorLayout.getBackground();
        if (backgroundDrawable == null) return;

        // Get the insets of the CoordinatorLayout
        int insetLeft = coordinatorLayout.getPaddingLeft();
        int insetTop = coordinatorLayout.getPaddingTop();
        int insetRight = coordinatorLayout.getPaddingRight();
        int insetBottom = coordinatorLayout.getPaddingBottom();

        // Set the CoordinatorLayout to a new InsetDrawable with insets all offset back to 0.
        InsetDrawable newDrawable =
                new InsetDrawable(
                        backgroundDrawable, -insetLeft, -insetTop, -insetRight, -insetBottom);
        coordinatorLayout.setBackground(newDrawable);
    }

    protected boolean isFullscreen() {
        return mIsFullscreenForTesting != null
                ? mIsFullscreenForTesting.getAsBoolean()
                : mFullscreenManager.getPersistentFullscreenMode();
    }

    protected void setupAnimator() {
        mAnimator = new ValueAnimator();
        mAnimator.addListener(
                new AnimatorListenerAdapter() {
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

    protected void startAnimation(
            int start,
            int end,
            ValueAnimator.AnimatorUpdateListener updateListener,
            Runnable endRunnable) {
        mAnimator.removeAllUpdateListeners();
        mAnimator.addUpdateListener(updateListener);
        mPostAnimationRunnable = endRunnable;
        mAnimator.setIntValues(start, end);
        mAnimator.start();
    }

    @Override
    public boolean handleCloseAnimation(Runnable finishRunnable) {
        // Can be entered twice - first from CustomTabToolbar (with a tap on close button)/
        // HandleStrategy (swiping down), once again from RootUiCoordinator. Just run the passed
        // runnable and return for the second invocation.
        if (mFinishRunnable != null) {
            if (finishRunnable != null) finishRunnable.run();
            return false;
        }
        mFinishRunnable = finishRunnable;
        return true;
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

    protected int getCustomTabsElevation() {
        return mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_elevation);
    }

    private void onToolbarContainerVisibilityChange(int visibility) {
        // See https://crbug.com/1430948 for more context. The issue is that sometimes when
        // exiting fullscreen, if we don't get a new layout, SurfaceFlinger doesn't recalculate
        // transparent regions and this View (and children) are never shown. Theoretically this
        // should also only ever need to be done the first time becoming visible after exiting
        // fullscreen, but PCCTs do not currently allow scrolling off the toolbar, so it doesn't
        // matter.
        if (visibility == View.VISIBLE) {
            ViewUtils.requestLayout(
                    mToolbarView,
                    "PartialCustomTabBaseStrategy.onToolbarContainerVisibilityChange");
        }
    }

    void setMockViewForTesting(
            ViewGroup coordinatorLayout, CustomTabToolbar toolbar, View toolbarCoordinator) {
        mPositionUpdater = this::updatePosition;
        mToolbarView = toolbar;
        mToolbarCoordinator = toolbarCoordinator;

        onPostInflationStartup();
    }

    void setFullscreenSupplierForTesting(BooleanSupplier fullscreen) {
        mIsFullscreenForTesting = fullscreen;
    }

    int getTopMarginForTesting() {
        var mlp = (ViewGroup.MarginLayoutParams) mToolbarCoordinator.getLayoutParams();
        return mlp.topMargin;
    }

    int getShadowOffsetForTesting() {
        return mShadowOffset;
    }

    static void resetDeviceSpecLoggedForTesting() {
        sDeviceSpecLogged = false;
    }
}
