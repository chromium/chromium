// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.os.Build;
import android.os.Handler;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.animation.AccelerateInterpolator;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsAnimationControlListenerCompat;
import androidx.core.view.WindowInsetsAnimationControllerCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.swiperefreshlayout.widget.CircularProgressDrawable;

import org.chromium.base.MathUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * CustomTabHeightStrategy for Partial Custom Tab. An instance of this class should be
 * owned by the CustomTabActivity.
 */
public class PartialCustomTabHeightStrategy extends CustomTabHeightStrategy
        implements ConfigurationChangedObserver, ValueAnimator.AnimatorUpdateListener,
                   PartialCustomTabHandleStrategy.DragEventCallback, FullscreenManager.Observer {
    @VisibleForTesting
    static final long SPINNER_TIMEOUT_MS = 500;
    /**
     * Minimal height the bottom sheet CCT should show is half of the display height.
     */
    private static final float MINIMAL_HEIGHT_RATIO = 0.5f;
    /**
     * The maximum height we can snap to is under experiment, we have two branches, 90% of the
     * display height and 100% of the display height. This ratio is used to calculate the 90% of the
     * display height.
     */
    private static final float EXTRA_HEIGHT_RATIO = 0.1f;
    private static final int SCROLL_DURATION_MS = 200;
    private static final int NAVBAR_FADE_DURATION_MS = 16;
    private static final int SPINNER_FADEIN_DURATION_MS = 100;
    private static final int SPINNER_FADEOUT_DURATION_MS = 400;

    @IntDef({HeightStatus.TOP, HeightStatus.INITIAL_HEIGHT, HeightStatus.TRANSITION})
    @Retention(RetentionPolicy.SOURCE)
    @interface HeightStatus {
        int TOP = 0;
        int INITIAL_HEIGHT = 1;
        int TRANSITION = 2;
    }

    private final Activity mActivity;
    private final Integer mNavigationBarColor;
    private final Integer mNavigationBarDividerColor;
    private final OnResizedCallback mOnResizedCallback;
    private final AnimatorListener mSpinnerFadeoutAnimatorListener;
    private final int mCachedHandleHeight;
    private final boolean mIsFixedHeight;
    private final @Px int mUnclampedInitialHeight;
    private final FullscreenManager mFullscreenManager;

    private @Px int mDisplayHeight;
    private @Px int mFullyExpandedAdjustmentHeight;
    private boolean mWindowAboveNavbar;
    private ValueAnimator mAnimator;
    private int mShadowOffset;
    private boolean mDrawOutlineShadow;

    // ContentFrame + CoordinatorLayout - CompositorViewHolder
    //              + NavigationBar
    //              + Spinner
    // When CCT_RESIZABLE_WINDOW_ABOVE_NAVBAR is disabled, We resize inner contents view.
    // Not just CompositorViewHolder but also CoordinatorLayout is resized because many UI
    // components such as BottomSheet, InfoBar, Snackbar are child views of CoordinatorLayout,
    // which makes them appear correctly at the bottom.
    private ViewGroup mContentFrame;
    private ViewGroup mCoordinatorLayout;

    private @HeightStatus int mStatus = HeightStatus.INITIAL_HEIGHT;
    private @HeightStatus int mTargetStatus;

    // Bottom navigation bar height. Set to zero when the bar is positioned on the right side
    // in landcape mode.
    private @Px int mNavbarHeight;
    private int mOrientation;

    // Note: Do not use anywhere except in |onConfigurationChanged| as it might not be up-to-date.
    private boolean mIsInMultiWindowMode;

    private ImageView mSpinnerView;
    private LinearLayout mNavbar;
    private CircularProgressDrawable mSpinner;
    private View mToolbarView;
    private View mToolbarCoordinator;
    private int mToolbarColor;
    private Runnable mPositionUpdater;
    private boolean mStopShowingSpinner;

    // Window attributes backed up for HTML fullscreen mode.
    private WindowManager.LayoutParams mPreFullscreenAttrs;

    // Runnable finishing the activity after the exit animation. Non-null when PCCT is closing.
    @Nullable
    private Runnable mFinishRunnable;

    // Y offset when a dragging gesture starts.
    private int mDraggingStartY;
    private float mOffsetY;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // This should be kept in sync with the definition |CustomTabsResizeType|
    // in tools/metrics/histograms/enums.xml.
    @IntDef({ResizeType.EXPANSION, ResizeType.MINIMIZATION, ResizeType.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    @interface ResizeType {
        int EXPANSION = 0;
        int MINIMIZATION = 1;
        int COUNT = 2;
    }

    /** A callback to be called once the Custom Tab has been resized. */
    interface OnResizedCallback {
        /** The Custom Tab has been resized. */
        void onResized(int size);
    }

    // The current height used to trigger onResizedCallback when it is resized.
    // Used in 'window-above-navbar' version only.
    private int mHeight;

    // Class used to control show / hide nav bar.
    private NavBarTransitionController mNavbarTransitionController =
            new NavBarTransitionController();

    public PartialCustomTabHeightStrategy(Activity activity, @Px int initialHeight,
            Integer navigationBarColor, Integer navigationBarDividerColor, boolean isFixedHeight,
            OnResizedCallback onResizedCallback, ActivityLifecycleDispatcher lifecycleDispatcher,
            FullscreenManager fullscreenManager) {
        mWindowAboveNavbar = ChromeFeatureList.sCctResizableWindowAboveNavbar.isEnabled();
        mActivity = activity;
        mDisplayHeight = getDisplayHeight();
        mUnclampedInitialHeight = initialHeight;
        mIsFixedHeight = isFixedHeight;
        mOnResizedCallback = onResizedCallback;
        mFullscreenManager = fullscreenManager;

        mAnimator = new ValueAnimator();
        mAnimator.setDuration(SCROLL_DURATION_MS);
        mAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mStatus = HeightStatus.TRANSITION;
            }
            @Override
            public void onAnimationEnd(Animator animation) {
                mStatus = mTargetStatus;
                onMoveEnd();
            }
        });
        mAnimator.addUpdateListener(this);

        lifecycleDispatcher.register(this);

        mOrientation = mActivity.getResources().getConfiguration().orientation;
        mIsInMultiWindowMode = MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);
        mNavigationBarColor = navigationBarColor;
        mNavigationBarDividerColor = navigationBarDividerColor;
        mDrawOutlineShadow = SysUtils.isLowEndDevice();
        mCachedHandleHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_handle_height);
        mSpinnerFadeoutAnimatorListener = new AnimatorListener() {
            @Override
            public void onAnimationStart(Animator animator) {}
            @Override
            public void onAnimationRepeat(Animator animator) {}
            @Override
            public void onAnimationEnd(Animator animator) {
                mSpinner.stop();
                mSpinnerView.setVisibility(View.GONE);
            }
            @Override
            public void onAnimationCancel(Animator animator) {}
        };

        // On pre-R devices, We wait till the layout is complete and get the content
        // |android.R.id.content| view height. See |getAppUsableScreenHeightFromContent|.
        mPositionUpdater =
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.R ? this::updatePosition : () -> {
            // Maybe invoked before layout inflation? Simply return here - postion update will be
            // executed by |onPostInflationStartUp| anyway.
            if (mContentFrame == null) return;

            mContentFrame.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int left, int top, int right, int bottom,
                        int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    mContentFrame.removeOnLayoutChangeListener(this);
                    updatePosition();
                }
            });
        };
        fullscreenManager.addObserver(this);
    }

    @Override
    public void onPostInflationStartup() {
        mContentFrame = (ViewGroup) mActivity.findViewById(android.R.id.content);
        mCoordinatorLayout = (ViewGroup) mActivity.findViewById(R.id.coordinator);
        // Elevate the main web contents area as high as the handle bar to have the shadow
        // effect look right.
        int ev = mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_elevation);
        mCoordinatorLayout.setElevation(ev);

        mPositionUpdater.run();
    }

    public void onShowSoftInput() {
        if (isFullHeight() || mStatus != HeightStatus.INITIAL_HEIGHT) return;

        // Expands to full height.
        int start = mActivity.getWindow().getAttributes().y;
        int end = getFullyExpandedYWithAdjustment();
        mAnimator.setIntValues(start, end);
        mTargetStatus = HeightStatus.TOP;
        mAnimator.start();
    }

    private void updatePosition() {
        if (mContentFrame == null) return;

        initializeHeight();
        updateShadowOffset();
        if (mWindowAboveNavbar) {
            maybeInvokeResizeCallback();
        } else {
            setContentsHeight();
            updateNavbarVisibility(true);
        }
    }

    private int initialY() {
        return mDisplayHeight - initialHeightInPortraitMode();
    }

    private int initialHeightInPortraitMode() {
        assert !isFullHeight() : "initialHeightInPortraitMode() is used in portrait mode only";
        return MathUtils.clamp(mUnclampedInitialHeight, mDisplayHeight - getStatusBarHeight(),
                (int) (mDisplayHeight * MINIMAL_HEIGHT_RATIO));
    }

    private @Px int getNavbarHeight() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return mActivity.getWindowManager()
                    .getCurrentWindowMetrics()
                    .getWindowInsets()
                    .getInsets(WindowInsets.Type.navigationBars())
                    .bottom;
        }
        // Pre-R OS offers no official way to get the navigation bar height. A common way was
        // to get it from a resource definition('navigation_bar_height') but it fails on some
        // vendor-customized devices.
        // A workaround here is to subtract the app-usable height from the whole display height.
        // There are a couple of ways to get the app-usable height:
        // 1) content frame + status bar height
        // 2) |display.getSize|
        // On some devices, only one returns the right height, the other returning a height
        // bigger that the actual value. Heuristically we choose the smaller of the two.
        return mDisplayHeight
                - Math.max(getAppUsableScreenHeightFromContent(),
                        getAppUsableScreenHeightFromDisplay());
    }

    private int getAppUsableScreenHeightFromContent() {
        // A correct way to get the client area height would be to use the parent of |content|
        // to make sure to include the top action bar dimension. But CCT (or Chrome for that
        // matter) doesn't have the top action bar. So getting the height of |content| is enough.
        return mContentFrame.getHeight() + getStatusBarHeight();
    }

    private int getAppUsableScreenHeightFromDisplay() {
        Display display = mActivity.getWindowManager().getDefaultDisplay();
        Point size = new Point();
        display.getSize(size);
        return size.y;
    }

    @Override
    public void onToolbarInitialized(
            View coordinatorView, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        mToolbarCoordinator = coordinatorView;
        mToolbarView = toolbar;
        mToolbarColor = toolbar.getBackground().getColor();
        roundCorners(coordinatorView, toolbar, toolbarCornerRadius);
        toolbar.setHandleStrategy(new PartialCustomTabHandleStrategy(
                mActivity, this::isFullHeight, () -> mStatus, this));
        updateDragBarVisibility();
    }

    // ConfigurationChangedObserver implementation.

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        boolean isInMultiWindow = MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);
        int orientation = newConfig.orientation;
        int displayHeight = getDisplayHeight();

        if (isInMultiWindow != mIsInMultiWindowMode || orientation != mOrientation
                || displayHeight != mDisplayHeight) {
            mIsInMultiWindowMode = isInMultiWindow;
            mOrientation = orientation;
            mDisplayHeight = displayHeight;
            if (isFullHeight()) {
                // We should update CCT position before Window#FLAG_LAYOUT_NO_LIMITS is set,
                // otherwise it is not possible to get the correct content height.
                mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
            }
            mPositionUpdater.run();
        }
    }

    // ValueAnimator.AnimatorUpdateListener implementation.
    @Override
    public void onAnimationUpdate(ValueAnimator valueAnimator) {
        int value = (int) valueAnimator.getAnimatedValue();
        updateWindowPos(value);
    }

    private void roundCorners(
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

    private static void adjustCornerRadius(GradientDrawable d, int radius) {
        d.mutate();
        d.setCornerRadii(new float[] {radius, radius, radius, radius, 0, 0, 0, 0});
    }

    @Override
    public void setScrimFraction(float scrimFraction) {
        int scrimColor = mActivity.getResources().getColor(R.color.default_scrim_color);
        float scrimColorAlpha = (scrimColor >>> 24) / 255f;
        int scrimColorOpaque = scrimColor & 0xFF000000;
        int color = ColorUtils.getColorWithOverlay(
                mToolbarColor, scrimColorOpaque, scrimFraction * scrimColorAlpha, false);

        // Drag handle view is not part of CoordinatorLayout. As the root UI scrim changes,
        // the handle view color needs updating to match it. This is a better way than running
        // PCCT's own scrim coordinator since it can apply shape-aware scrim to the handle view
        // that has the rounded corner.
        View dragBar = mActivity.findViewById(R.id.drag_bar);
        GradientDrawable drawable = (GradientDrawable) dragBar.getBackground();
        drawable.setColor(color);
    }

    private void initializeHeight() {
        Window window = mActivity.getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        mNavbarHeight = getNavbarHeight();

        // When the flag is enabled, we make the max snap point 10% shorter, so it will only occupy
        // 90% of the height.
        mFullyExpandedAdjustmentHeight = ChromeFeatureList.sCctResizable90MaximumHeight.isEnabled()
                ? (int) ((mDisplayHeight - getFullyExpandedY()) * EXTRA_HEIGHT_RATIO)
                : 0;

        int maxExpandedY = getFullyExpandedY();
        @Px
        int height = 0;

        if (isFullHeight()) {
            // Resizing by user dragging is not supported in landscape mode; no need to set
            // the status here.
            if (!mWindowAboveNavbar) {
                height = mDisplayHeight;
                mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
            }
        } else {
            height = initialHeightInPortraitMode();
            mStatus = HeightStatus.INITIAL_HEIGHT;
            if (!mWindowAboveNavbar) {
                mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
            }
        }

        WindowManager.LayoutParams attrs = window.getAttributes();
        if (attrs.height == height) return;

        if (mWindowAboveNavbar) {
            // To avoid the bottom navigation bar area flickering when starting dragging, position
            // web contents area right above the navigation bar so the two won't overlap. The
            // navigation area now just shows whatever is underneath: 1) loading view/web contents
            // while dragging 2) host app's navigation bar when at rest.
            positionAtHeight(height);
            mHeight = attrs.height;
        } else {
            // We do not resize Window but just translate its vertical offset, and resize
            // CoordinatorLayoutForPointer instead. This helps us work around the round-corner bug
            // in Android S. See b/223536648.
            attrs.y = Math.max(maxExpandedY, mDisplayHeight - height);
            window.setAttributes(attrs);
        }
        updateDragBarVisibility();
    }

    private void positionAtHeight(int height) {
        WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
        if (isFullHeight()) {
            attrs.height = MATCH_PARENT;
            attrs.y = 0;
            attrs.gravity = Gravity.NO_GRAVITY;
        } else {
            attrs.height = height - mNavbarHeight;
            attrs.y = mNavbarHeight;
            attrs.gravity = Gravity.BOTTOM;
        }
        mActivity.getWindow().setAttributes(attrs);
    }

    private void updateDragBarVisibility() {
        View dragBar = mActivity.findViewById(R.id.drag_bar);
        if (dragBar != null) dragBar.setVisibility(isFullHeight() ? View.GONE : View.VISIBLE);

        View dragHandlebar = mActivity.findViewById(R.id.drag_handlebar);
        if (dragHandlebar != null) {
            dragHandlebar.setVisibility(isFixedHeight() ? View.GONE : View.VISIBLE);
        }
    }

    private void updateShadowOffset() {
        // TODO(jinsukkim): Remove the shadow when in full-height so there won't be a gap
        //                  beneath the status bar.
        if (isFullHeight() || mDrawOutlineShadow) {
            mShadowOffset = 0;
        } else {
            mShadowOffset = mActivity.getResources().getDimensionPixelSize(
                    R.dimen.custom_tabs_shadow_offset);
        }
        setTopMargins(mShadowOffset, getHandleHeight() + mShadowOffset);
        mToolbarCoordinator.requestLayout();
    }

    private void setTopMargins(int shadowOffset, int handleOffset) {
        View handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) handleView.getLayoutParams();
        lp.setMargins(0, shadowOffset, 0, 0);

        // Make enough room for the handle View.
        ViewGroup.MarginLayoutParams mlp =
                (ViewGroup.MarginLayoutParams) mToolbarCoordinator.getLayoutParams();
        mlp.setMargins(0, handleOffset, 0, 0);
    }

    private int getHandleHeight() {
        return isFullHeight() ? 0 : mCachedHandleHeight;
    }

    private boolean isFullHeight() {
        return isLandscape() || MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);
    }

    private boolean isLandscape() {
        return mOrientation == Configuration.ORIENTATION_LANDSCAPE;
    }

    private boolean isFixedHeight() {
        return mIsFixedHeight;
    }

    private void updateWindowPos(@Px int y) {
        // Do not allow the Window to go above the minimum threshold capped by the status
        // bar and (optionally) the 90%-height adjustment.
        int topY = getFullyExpandedYWithAdjustment();
        y = MathUtils.clamp(y, topY, mDisplayHeight);
        Window window = mActivity.getWindow();
        WindowManager.LayoutParams attrs = window.getAttributes();
        if (attrs.y == y) return;

        // If the tab is not resizable then dragging it higher than the initial height will not be
        // allowed. The tab can still be dragged down in order to be closed.
        if (isFixedHeight() && y < initialY()) return;

        attrs.y = y;
        window.setAttributes(attrs);
        if (mFinishRunnable != null) return;

        // Starting dragging from INITIAL_HEIGHT state, we can hide the spinner if the tab:
        // 1) reaches full height
        // 2) is dragged below the initial height
        if (mStatus == HeightStatus.INITIAL_HEIGHT && (y <= topY || y > initialY())
                && isSpinnerVisible()) {
            hideSpinnerView();
            if (y <= topY) {
                // Once reaching full-height, tab can hide the spinner permanently till
                // the finger is lifted. Keep it hidden.
                mStopShowingSpinner = true;
                return;
            }
        }
        // Show the spinner lazily, only when the tab is dragged _up_, which requires showing
        // more area than initial state.
        if (!mStopShowingSpinner && mStatus != HeightStatus.TRANSITION && !isSpinnerVisible()
                && y < mDraggingStartY) {
            showSpinnerView();
            if (mWindowAboveNavbar) {
                // We do not have to keep the spinner till the end of dragging action in
                // 'window-above-navbar' version since it doesn't have the flickering issue at
                // the end. Keeping it visible up to 500ms is sufficient to hide the initial
                // glitch that can briefly expose the host app screen at the beginning.
                new Handler().postDelayed(() -> {
                    hideSpinnerView();
                    mStopShowingSpinner = true;
                }, SPINNER_TIMEOUT_MS);
            }
        }
        if (isSpinnerVisible()) {
            centerSpinnerVertically((ViewGroup.LayoutParams) mSpinnerView.getLayoutParams());
        }
    }

    private boolean isSpinnerVisible() {
        return mSpinnerView != null && mSpinnerView.getVisibility() == View.VISIBLE;
    }

    private void onMoveStart() {
        if (mWindowAboveNavbar) {
            Window window = mActivity.getWindow();
            window.addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
            WindowManager.LayoutParams attrs = window.getAttributes();
            attrs.y = mDisplayHeight - attrs.height - mNavbarHeight;
            attrs.height = mDisplayHeight;
            attrs.gravity = Gravity.NO_GRAVITY;
            window.setAttributes(attrs);
            showNavbarButtons(false);
        } else {
            updateNavbarVisibility(false);
        }
    }

    private void onMoveEnd() {
        if (mFinishRunnable != null) {
            mFinishRunnable.run();
            return;
        }

        int draggingEndY = mActivity.getWindow().getAttributes().y;
        if (mDraggingStartY >= 0 && mDraggingStartY != draggingEndY) {
            RecordHistogram.recordEnumeratedHistogram("CustomTabs.ResizeType",
                    mDraggingStartY < draggingEndY ? ResizeType.MINIMIZATION : ResizeType.EXPANSION,
                    ResizeType.COUNT);
        }

        hideSpinnerView();
        if (mWindowAboveNavbar) {
            Window window = mActivity.getWindow();
            window.clearFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
            positionAtHeight(mDisplayHeight - window.getAttributes().y);
            showNavbarButtons(true);
            maybeInvokeResizeCallback();
        } else {
            updateNavbarVisibility(true);
        }
    }

    private void hideSpinnerView() {
        if (!mWindowAboveNavbar) setContentsHeight();

        // TODO(crbug.com/1328555): Look into observing a view resize event to ensure the fade
        // animation can always cover the transition artifact.
        if (isSpinnerVisible()) {
            mSpinnerView.animate()
                    .alpha(0f)
                    .setDuration(SPINNER_FADEOUT_DURATION_MS)
                    .setListener(mSpinnerFadeoutAnimatorListener);
        }
    }

    private void showSpinnerView() {
        if (mSpinnerView != null) {
            centerSpinnerVertically((ViewGroup.LayoutParams) mSpinnerView.getLayoutParams());
        } else {
            mSpinnerView = new ImageView(mActivity);
            mSpinnerView.setElevation(
                    mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_elevation));
            mSpinnerView.setBackgroundColor(mActivity.getColor(R.color.window_background_color));

            // Toolbar should not be hidden by spinner screen.
            ViewGroup.MarginLayoutParams lp = new ViewGroup.MarginLayoutParams(MATCH_PARENT, 0);
            int topMargin = mToolbarView.getHeight();
            // See the comment below for why we add handle height.
            if (!mWindowAboveNavbar) topMargin += getHandleHeight();
            lp.setMargins(0, topMargin, 0, 0);

            mSpinner = new CircularProgressDrawable(mActivity);
            mSpinner.setStyle(CircularProgressDrawable.LARGE);
            mSpinnerView.setImageDrawable(mSpinner);
            mSpinnerView.setScaleType(ImageView.ScaleType.CENTER);
            int[] colorList = new int[1];
            colorList[0] = mActivity.getColor(R.color.default_bg_color_blue);
            mSpinner.setColorSchemeColors(colorList);
            centerSpinnerVertically(lp);
        }
        // Spinner view is added to ContentFrameLayout to hide both WebContents and navigation bar.
        // For window-above-navbar, it should be added to CoordinatorLayoutForPointer to obscure
        // the flickering at the beginning of dragging action. Their top positions differ by
        // |getHandleHeight()| which is a top margin of CoordinatorLayoutForPointer.
        if (mSpinnerView.getParent() == null) {
            ViewGroup parent = mWindowAboveNavbar ? mCoordinatorLayout : mContentFrame;
            parent.addView(mSpinnerView);
        }
        mSpinnerView.clearAnimation();
        mSpinnerView.setAlpha(0.f);
        mSpinnerView.setVisibility(View.VISIBLE);
        mSpinnerView.animate().alpha(1.f).setDuration(SPINNER_FADEIN_DURATION_MS).setListener(null);
        mSpinner.start();
    }

    private void centerSpinnerVertically(ViewGroup.LayoutParams lp) {
        int toolbarHeight = mToolbarView.getHeight();
        int cctHeight = mDisplayHeight - mActivity.getWindow().getAttributes().y - toolbarHeight;
        lp.height = cctHeight;
        mSpinnerView.setLayoutParams(lp);
    }

    private void setContentsHeight() {
        // Return early if the parent view is not available. CCT may be on its way to destruction.
        if (mCoordinatorLayout == null) return;

        ViewGroup.LayoutParams lp = mCoordinatorLayout.getLayoutParams();
        int oldHeight = lp.height;

        // We resize CoordinatorLayout to occupy the size we want for CCT. This excludes
        // the bottom navigation bar height and the top margin of CVH set aside for
        // the handle bar portion of the CCT toolbar header.
        // TODO(jinsukkim):
        //   - Remove the shadow when in full-height so there won't be a gap beneath the status bar.
        int windowPos = mActivity.getWindow().getAttributes().y;
        lp.height = mDisplayHeight - windowPos - getHandleHeight() - mShadowOffset - mNavbarHeight;
        mCoordinatorLayout.setLayoutParams(lp);
        if (oldHeight >= 0 && lp.height != oldHeight) mOnResizedCallback.onResized(lp.height);
    }

    // Show or hide our own navigation bar.
    private void updateNavbarVisibility(boolean show) {
        if (show) {
            if (shouldShowSystemNavbar()) {
                setNavigationBarAndDividerColor();
                if (mNavbar != null) mNavbar.setVisibility(View.GONE);
                return;
            }
            if (mNavbar == null) {
                mNavbar = (LinearLayout) mActivity.getLayoutInflater().inflate(
                        R.layout.custom_tabs_navigation_bar, null);
                mNavbar.setLayoutParams(new ViewGroup.LayoutParams(MATCH_PARENT, mNavbarHeight));
                setNavbarOffset();
                setNavigationBarAndDividerColor();

                assert mContentFrame != null;
                mContentFrame.addView(mNavbar);
            } else {
                setNavbarOffset();
                mNavbar.setAlpha(0f);
                mNavbar.setVisibility(View.VISIBLE);
                mNavbar.animate().alpha(1.f).setDuration(NAVBAR_FADE_DURATION_MS);
            }
        } else {
            if (mNavbar != null) mNavbar.animate().alpha(0.f).setDuration(NAVBAR_FADE_DURATION_MS);
        }
        showNavbarButtons(show);
    }

    /**
     * Return whether we need to show not its own custom navigation bar but the system-provided
     * one that can handle the API |setNavigationBarColor()|.
     */
    private boolean shouldShowSystemNavbar() {
        return mNavbarHeight == 0 || isFullHeight();
    }

    // Position our own navbar where the system navigation bar which is obscured by WebContents
    // rendered over it due to Window#FLAGS_LAYOUT_NO_LIMITS would be shown.
    private void setNavbarOffset() {
        if (mCoordinatorLayout == null) return;
        int offset =
                mCoordinatorLayout.getLayoutParams().height + getHandleHeight() + mShadowOffset;
        mNavbar.setTranslationY(offset);
    }

    private void setNavigationBarAndDividerColor() {
        // Set the default color based on the system theme, if not specified.
        int color = mNavigationBarColor != null ? mNavigationBarColor
                                                : mActivity.getColor(R.color.navigation_bar_color);

        // Since we cannot alter the button color, darken the bar color instead to address
        // the bad contrast against buttons when they are both white.
        boolean needsDarkButtons = !ColorUtils.shouldUseLightForegroundOnBackground(color);
        if (needsDarkButtons) color = ColorUtils.getDarkenedColorForStatusBar(color);
        if (shouldShowSystemNavbar()) {
            mActivity.getWindow().setNavigationBarColor(color);
        } else {
            // Use our own navbar where the system navigation bar which is obscured by WebContents
            // rendered over it due to Window#FLAGS_LAYOUT_NO_LIMITS would be shown.
            View bar = mNavbar.findViewById(R.id.bar);
            bar.setBackgroundColor(color);
        }

        // navigationBarDividerColor can only be set in Android P+
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;
        Integer dividerColor = CustomTabNavigationBarController.getDividerColor(
                mActivity, mNavigationBarColor, mNavigationBarDividerColor, needsDarkButtons);
        if (shouldShowSystemNavbar()) {
            if (dividerColor != null) {
                mActivity.getWindow().setNavigationBarDividerColor(dividerColor);
            }
        } else {
            View divider = mNavbar.findViewById(R.id.divider);
            if (dividerColor != null) {
                divider.setBackgroundColor(dividerColor);
            } else {
                divider.setVisibility(View.GONE);
            }
        }
    }

    private void maybeInvokeResizeCallback() {
        WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
        if (mHeight != attrs.height && attrs.height > 0) {
            mOnResizedCallback.onResized(attrs.height);
            mHeight = attrs.height;
        }
    }

    private void showNavbarButtons(boolean show) {
        View decorView = mActivity.getWindow().getDecorView();
        WindowInsetsControllerCompat controller =
                WindowCompat.getInsetsController(mActivity.getWindow(), decorView);
        if (show) {
            controller.show(WindowInsetsCompat.Type.navigationBars());
        } else {
            // Can we remove the slow fade-out animation?
            controller.hide(WindowInsetsCompat.Type.navigationBars());
        }

        // Take over the control of insets animation after the #show / #hide. This call needs to
        // happen after the #show / #hide call to work correctly.
        mNavbarTransitionController.setShow(show);
        controller.controlWindowInsetsAnimation(WindowInsetsCompat.Type.navigationBars(),
                /*durationMillis*/ 1, null, null, mNavbarTransitionController);
    }

    // TODO(jinsukkim): Explore the way to use androidx.window.WindowManager or
    // androidx.window.java.WindowInfoRepoJavaAdapter once the androidx API get finalized and is
    // available in Chromium to use #getCurrentWindowMetrics()/#currentWindowMetrics() to get the
    // height of the display our Window currently in.
    //
    // The #getRealMetrics() method will give the physical size of the screen, which is generally
    // fine when the app is not in multi-window mode and #getMetrics() will give the height excludes
    // the decor views, so not suitable for our case. But in multi-window mode, we have no much
    // choice, the closest way is to use #getMetrics() method, because we need to handle rotation.
    private @Px int getDisplayHeight() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return mActivity.getWindowManager().getCurrentWindowMetrics().getBounds().height();
        }

        DisplayMetrics displayMetrics = new DisplayMetrics();
        if (MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity)) {
            mActivity.getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
        } else {
            mActivity.getWindowManager().getDefaultDisplay().getRealMetrics(displayMetrics);
        }
        return displayMetrics.heightPixels;
    }

    // status_bar_height is not a public framework resource, so we have to getIdentifier()
    @SuppressWarnings("DiscouragedApi")
    private @Px int getStatusBarHeight() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return mActivity.getWindowManager()
                    .getCurrentWindowMetrics()
                    .getWindowInsets()
                    .getInsets(WindowInsets.Type.statusBars())
                    .top;
        }
        int statusBarHeight = 0;
        final int statusBarHeightResourceId =
                mActivity.getResources().getIdentifier("status_bar_height", "dimen", "android");
        if (statusBarHeightResourceId > 0) {
            statusBarHeight =
                    mActivity.getResources().getDimensionPixelSize(statusBarHeightResourceId);
        }
        return statusBarHeight;
    }

    private @Px int getFullyExpandedY() {
        return getStatusBarHeight();
    }

    @VisibleForTesting
    @Px
    int getFullyExpandedYWithAdjustment() {
        // Adding |mFullyExpandedAdjustmentHeight| to the y coordinate because the
        // coordinates system's origin is at the top left and y is growing in downward, larger y
        // means smaller height of the bottom sheet CCT.
        return getFullyExpandedY() + mFullyExpandedAdjustmentHeight;
    }

    // CustomTabHeightStrategy implementation

    @Override
    public boolean changeBackgroundColorForResizing() {
        // Need to return true to keep the transparent background we set in the init step.
        return true;
    }

    @Override
    public void handleCloseAnimation(Runnable finishRunnable) {
        if (mFinishRunnable != null) return;

        mFinishRunnable = finishRunnable;
        Window window = mActivity.getWindow();
        WindowManager.LayoutParams attrs = window.getAttributes();
        if (attrs.gravity == Gravity.BOTTOM || isFullHeight()) {
            window.addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
            attrs.y = isFullHeight() ? getFullyExpandedY()
                                     : mDisplayHeight - attrs.height - mNavbarHeight;
            attrs.gravity = Gravity.TOP; // NO_GRAVITY doesn't work here.
            window.setAttributes(attrs);
        }
        mAnimator.setIntValues(attrs.y, mDisplayHeight - mNavbarHeight);
        mAnimator.setDuration(
                mActivity.getResources().getInteger(android.R.integer.config_mediumAnimTime));
        mAnimator.setInterpolator(new AccelerateInterpolator());
        mAnimator.start();
    }

    @Override
    public boolean canDrawOutsideScreen() {
        return !mWindowAboveNavbar && !isFullHeight();
    }

    // DragEventCallback implementation

    @Override
    public void onDragStart(int y) {
        onMoveStart();
        Window window = mActivity.getWindow();
        mDraggingStartY = window.getAttributes().y;
        mOffsetY = mDraggingStartY - y;
        mStopShowingSpinner = false;
    }

    @Override
    public void onDragMove(int y) {
        updateWindowPos((int) (y + mOffsetY));
    }

    @Override
    public boolean onDragEnd(int flingDistance) {
        int currentY = mActivity.getWindow().getAttributes().y;
        int finalY = currentY + flingDistance;
        int topY = getFullyExpandedYWithAdjustment();
        int initialY = initialY();
        int bottomY = mDisplayHeight - mNavbarHeight;
        int animateEndY = -1;

        if (finalY < initialY) { // Move up
            if (Math.abs(topY - finalY) < Math.abs(finalY - initialY)) {
                mTargetStatus = HeightStatus.TOP;
                animateEndY = topY;
            } else {
                mTargetStatus = HeightStatus.INITIAL_HEIGHT;
                animateEndY = initialY;
            }
        } else { // Move down
            // Prevents skipping initial state when swiping from the top.
            if (mStatus == HeightStatus.TOP) finalY = Math.min(initialY, finalY);

            if (Math.abs(initialY - finalY) < Math.abs(finalY - bottomY)) {
                mTargetStatus = HeightStatus.INITIAL_HEIGHT;
                animateEndY = initialY;
            }
        }

        if (animateEndY < 0) return false;

        mAnimator.setIntValues(currentY, animateEndY);
        mAnimator.start();
        return true;
    }

    // FullscreenManager.Observer implementation

    @Override
    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
        // TODO(jinsukkim): Handle fullscreen in non-'window-above-navbar' version as well.
        if (mPreFullscreenAttrs != null || !mWindowAboveNavbar) return;
        mPreFullscreenAttrs = mActivity.getWindow().getAttributes();
        WindowManager.LayoutParams attrs = new WindowManager.LayoutParams();
        attrs.copyFrom(mPreFullscreenAttrs);
        attrs.x = 0;
        attrs.y = 0;
        attrs.height = MATCH_PARENT;
        attrs.width = MATCH_PARENT;
        mActivity.getWindow().setAttributes(attrs);
        setTopMargins(0, 0);
    }

    @Override
    public void onExitFullscreen(Tab tab) {
        if (mPreFullscreenAttrs == null || !mWindowAboveNavbar) return;
        mActivity.getWindow().setAttributes(mPreFullscreenAttrs);
        mPreFullscreenAttrs = null;
        setTopMargins(mShadowOffset, getHandleHeight() + mShadowOffset);
    }

    @Override
    public void destroy() {
        mFullscreenManager.removeObserver(this);
    }

    @VisibleForTesting
    void setMockViewForTesting(LinearLayout navbar, ImageView spinnerView,
            CircularProgressDrawable spinner, View toolbar, View toolbarCoordinator) {
        mNavbar = navbar;
        mSpinnerView = spinnerView;
        mSpinner = spinner;
        mToolbarView = toolbar;
        mToolbarCoordinator = toolbarCoordinator;

        mPositionUpdater = this::updatePosition;
        onPostInflationStartup();
    }

    @VisibleForTesting
    int getNavbarHeightForTesting() {
        return mNavbarHeight;
    }

    @VisibleForTesting
    void setWindowAboveNavbarForTesting(boolean windowAboveNavbar) {
        mWindowAboveNavbar = windowAboveNavbar;
    }

    @VisibleForTesting
    PartialCustomTabHandleStrategy createHandleStrategyForTesting() {
        // Pass null for context because we don't depend on the GestureDetector inside as we invoke
        // MotionEvents directly in the tests.
        return new PartialCustomTabHandleStrategy(null, this::isFullHeight, () -> mStatus, this);
    }

    // Reusable class used to control nav bar transitioning, to make the transition instant.
    private static class NavBarTransitionController
            implements WindowInsetsAnimationControlListenerCompat {
        private boolean mShown;

        void setShow(boolean show) {
            mShown = show;
        }

        @Override
        public void onReady(@NonNull WindowInsetsAnimationControllerCompat controller, int types) {
            controller.finish(mShown);
        }

        @Override
        public void onFinished(WindowInsetsAnimationControllerCompat controller) {}

        @Override
        public void onCancelled(WindowInsetsAnimationControllerCompat controller) {}
    }
}
