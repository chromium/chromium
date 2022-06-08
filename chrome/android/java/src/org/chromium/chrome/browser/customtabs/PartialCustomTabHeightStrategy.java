// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.IntDef;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.MotionEventCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.swiperefreshlayout.widget.CircularProgressDrawable;

import org.chromium.base.MathUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * CustomTabHeightStrategy for Partial Custom Tab. An instance of this class should be
 * owned by the CustomTabActivity.
 */
public class PartialCustomTabHeightStrategy extends CustomTabHeightStrategy
        implements ConfigurationChangedObserver, ValueAnimator.AnimatorUpdateListener,
                   MultiWindowModeStateDispatcher.MultiWindowModeObserver {
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
    private static final int SPINNER_FADE_DURATION_MS = 400;

    @IntDef({HeightStatus.TOP, HeightStatus.INITIAL_HEIGHT, HeightStatus.TRANSITION})
    @Retention(RetentionPolicy.SOURCE)
    private @interface HeightStatus {
        int TOP = 0;
        int INITIAL_HEIGHT = 1;
        int TRANSITION = 2;
    }

    private final Activity mActivity;
    private final @Px int mInitialHeight;
    private final @Px int mMaxHeight;

    private final @Px int mFullyExpandedAdjustmentHeight;
    private final Integer mNavigationBarColor;
    private final Integer mNavigationBarDividerColor;
    private final OnResizedCallback mOnResizedCallback;
    private final AnimatorListener mSpinnerFadeoutAnimatorListener;
    private final int mHandleHeight;
    private ValueAnimator mAnimator;
    private int mShadowOffset;
    private boolean mDrawOutlineShadow;

    // ContentFrame + CoordinatorLayout - CompositorViewHolder
    //              + NavigationBar
    //              + Spinner
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
    private boolean mIsInMultiWindowMode;

    private ImageView mSpinnerView;
    private LinearLayout mNavbar;
    private CircularProgressDrawable mSpinner;
    private View mToolbarView;
    private View mToolbarCoordinator;

    /** A callback to be called once the Custom Tab has been resized. */
    interface OnResizedCallback {
        /** The Custom Tab has been resized. */
        void onResized(int size);
    }

    /**
     * Handling touch events for resizing the Window.
     */
    @VisibleForTesting
    /* package */ class PartialCustomTabHandleStrategy
            extends GestureDetector.SimpleOnGestureListener
            implements CustomTabToolbar.HandleStrategy {
        private static final int CLOSE_DISTANCE = 300;
        private GestureDetector mGestureDetector;
        private float mLastPosY;
        private float mLastDownPosY;
        private float mMostRecentYDistance;
        private float mInitialY;
        private boolean mSeenFirstMoveOrDown;
        private Runnable mCloseHandler;

        public PartialCustomTabHandleStrategy(Context context) {
            mGestureDetector = new GestureDetector(context, this, ThreadUtils.getUiThreadHandler());
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent event) {
            if (mOrientation == Configuration.ORIENTATION_LANDSCAPE) {
                return false;
            }
            if (mIsInMultiWindowMode) {
                return false;
            }
            return mGestureDetector.onTouchEvent(event);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (!CachedFeatureFlags.isEnabled(
                        ChromeFeatureList.CCT_RESIZABLE_ALLOW_RESIZE_BY_USER_GESTURE)) {
                return false;
            }

            if (mStatus == HeightStatus.TRANSITION) {
                return true;
            }
            // We will get events directly even when onInterceptTouchEvent() didn't return true,
            // because the sub View tree might not want this event, so check orientation and
            // multi-window flags here again.
            if (mOrientation == Configuration.ORIENTATION_LANDSCAPE) {
                return true;
            }
            if (mIsInMultiWindowMode) {
                return true;
            }

            float y = event.getRawY();
            switch (MotionEventCompat.getActionMasked(event)) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_MOVE:
                    if (!mSeenFirstMoveOrDown) {
                        mSeenFirstMoveOrDown = true;
                        onMoveStart();
                        mLastDownPosY = y;
                        mInitialY = mActivity.getWindow().getAttributes().y;
                    } else {
                        updateWindowPos((int) (mInitialY - mLastDownPosY + y));
                        if (y - mLastPosY != 0) {
                            mMostRecentYDistance = y - mLastPosY;
                        }
                        if (mStatus == HeightStatus.INITIAL_HEIGHT
                                && y - mInitialY > CLOSE_DISTANCE) {
                            mCloseHandler.run();
                        }
                    }
                    mLastPosY = y;
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    if (mSeenFirstMoveOrDown) {
                        if (!handleAnimation(mMostRecentYDistance)) {
                            onMoveEnd();
                        }
                    }
                    mMostRecentYDistance = 0;
                    mSeenFirstMoveOrDown = false;
                    return true;
                default:
                    return true;
            }
        }

        @Override
        public void setCloseClickHandler(Runnable handler) {
            mCloseHandler = handler;
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            // Always intercept scroll events.
            return true;
        }

        private boolean handleAnimation(float distanceY) {
            boolean playAnimation = false;

            int start = 0;
            int end = 0;

            if (distanceY < 0) {
                start = mActivity.getWindow().getAttributes().y;
                end = getFullyExpandedYCoordinateWithAdjustment();
                if (start > end) {
                    playAnimation = true;
                }
                mTargetStatus = HeightStatus.TOP;
            } else if (distanceY > 0) {
                start = mActivity.getWindow().getAttributes().y;
                end = mMaxHeight - mInitialHeight;
                if (start < end) {
                    playAnimation = true;
                }
                mTargetStatus = HeightStatus.INITIAL_HEIGHT;
            }
            if (playAnimation) {
                mAnimator.setIntValues(start, end);
                mStatus = HeightStatus.TRANSITION;
                mAnimator.start();
            }
            return playAnimation;
        }
    }

    public PartialCustomTabHeightStrategy(Activity activity,
            ObservableSupplier<? extends FrameLayout> parentViewSupplier, @Px int initialHeight,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            Integer navigationBarColor, Integer navigationBarDividerColor,
            OnResizedCallback onResizedCallback, ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mMaxHeight = getMaximumPossibleHeight();
        mInitialHeight = MathUtils.clamp(
                initialHeight, mMaxHeight, (int) (mMaxHeight * MINIMAL_HEIGHT_RATIO));

        // Invoked twice - when populated/destroyed(null)
        // TODO(jinsukkim): Obtain the CoordinatorLayout and ContentFrame directly,
        //                  not through CompositorViewHolder which is not in use.
        parentViewSupplier.addObserver(parentView -> {
            if (parentView == null) {
                mCoordinatorLayout = null;
                mContentFrame = null;
                return;
            }

            mCoordinatorLayout = (ViewGroup) parentView.getParent();
            mContentFrame = (ViewGroup) mCoordinatorLayout.getParent();

            // Elevate the main web contents area as high as the handle bar to have the shadow
            // effect look right.
            int ev = mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_elevation);
            mCoordinatorLayout.setElevation(ev);

            // When the navigation bar on the right side (not at the bottom), no need to set
            // contents height since it is fixed to the max height.
            if (mNavbarHeight != 0) setContentsHeight();
            updateNavbarVisibility(true);
        });

        mOnResizedCallback = onResizedCallback;
        // When the flag is enabled, we make the max snap point 10% shorter, so it will only occupy
        // 90% of the height.
        mFullyExpandedAdjustmentHeight =
                CachedFeatureFlags.isEnabled(ChromeFeatureList.CCT_RESIZABLE_90_MAXIMUM_HEIGHT)
                ? (int) ((mMaxHeight - getFullyExpandedYCoordinate()) * EXTRA_HEIGHT_RATIO)
                : 0;

        mAnimator = new ValueAnimator();
        mAnimator.setDuration(SCROLL_DURATION_MS);
        mAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mStatus = mTargetStatus;
                onMoveEnd();
            }
        });
        mAnimator.addUpdateListener(this);

        lifecycleDispatcher.register(this);

        multiWindowModeStateDispatcher.addObserver(this);

        mOrientation = mActivity.getResources().getConfiguration().orientation;
        mIsInMultiWindowMode = multiWindowModeStateDispatcher.isInMultiWindowMode();
        mNavigationBarColor = navigationBarColor;
        mNavigationBarDividerColor = navigationBarDividerColor;
        mDrawOutlineShadow = SysUtils.isLowEndDevice();
        mHandleHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_handle_height);
        mSpinnerFadeoutAnimatorListener = new AnimatorListener() {
            @Override
            public void onAnimationStart(Animator animator) {}

            @Override
            public void onAnimationRepeat(Animator animator) {}

            @Override
            public void onAnimationEnd(Animator animator) {
                mSpinner.stop();
            }
            @Override
            public void onAnimationCancel(Animator animator) {}
        };
        initializeHeight();
    }

    private @Px int getNavbarHeight() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return mActivity.getWindowManager()
                    .getCurrentWindowMetrics()
                    .getWindowInsets()
                    .getInsets(WindowInsets.Type.navigationBars())
                    .bottom;
        }
        return getDisplayHeight() - getAppUsableScreenHeight();
    }

    private int getAppUsableScreenHeight() {
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
        roundCorners(coordinatorView, toolbar, toolbarCornerRadius);
        toolbar.setHandleStrategy(new PartialCustomTabHandleStrategy(mActivity));
    }

    // MultiWindowMOdeObserver implementation
    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        mIsInMultiWindowMode = isInMultiWindowMode;
    }

    // ConfigurationChangedObserver implementation.
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (newConfig.orientation != mOrientation) {
            mOrientation = newConfig.orientation;
            initializeHeight();
            updateShadowOffset();
            setContentsHeight();
            updateNavbarVisibility(true);
        }
    }

    // ValueAnimator.AnimatorUpdateListener implementation.
    @Override
    public void onAnimationUpdate(ValueAnimator valueAnimator) {
        int ypos = (int) valueAnimator.getAnimatedValue();
        updateWindowPos(ypos);
    }

    private void roundCorners(
            View coordinator, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        // Inflate the handle View.
        ViewStub handleViewStub = mActivity.findViewById(R.id.custom_tabs_handle_view_stub);
        handleViewStub.inflate();
        View handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);
        handleView.setElevation(
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_elevation));
        View handleBar = handleView.findViewById(R.id.handle_bar);
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) handleBar.getLayoutParams();
        int dragBarTopMargin =
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_handle_height)
                - mActivity.getResources().getDimensionPixelSize(
                        R.dimen.custom_tabs_drag_bar_height);
        lp.setMargins(0, dragBarTopMargin, 0, 0);

        GradientDrawable background = (GradientDrawable) handleView.getBackground();
        background.mutate();
        background.setCornerRadii(new float[] {toolbarCornerRadius, toolbarCornerRadius,
                toolbarCornerRadius, toolbarCornerRadius, 0, 0, 0, 0});
        updateShadowOffset();
        if (mDrawOutlineShadow) {
            int width = mActivity.getResources().getDimensionPixelSize(
                    R.dimen.custom_tabs_outline_width);
            int color = toolbar.getBackground().getColor();
            background.setStroke(width, toolbar.getToolbarHairlineColor(color));
        }

        handleView.setBackground(background);

        // Pass the handle View to CustomTabToolbar for background color management.
        toolbar.setHandleView(handleView);

        // Having the transparent background is necessary for the shadow effect.
        mActivity.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
    }

    private void initializeHeight() {
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        mNavbarHeight = getNavbarHeight();
        int maxHeight = getDisplayHeight();
        int maxExpandedY = getFullyExpandedYCoordinate();
        final @Px int height;

        if (mOrientation == Configuration.ORIENTATION_LANDSCAPE) {
            // Resizing by user dragging is not supported in landscape mode; no need to set
            // the status here.
            height = maxHeight - maxExpandedY;
            mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        } else {
            height = mInitialHeight;
            mStatus = HeightStatus.INITIAL_HEIGHT;
            mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        }

        WindowManager.LayoutParams attributes = mActivity.getWindow().getAttributes();
        // TODO(jinsukkim): Handle multi-window mode.
        if (attributes.height == height) return;

        // We do not resize Window but just translate its vertical offset, and resize the parent
        // view of WebContents (CompositorViewHolder) instead. This helps us work around the round-
        // corner bug in Android S. See b/223536648.
        attributes.y = Math.max(maxExpandedY, maxHeight - height - mNavbarHeight);
        mActivity.getWindow().setAttributes(attributes);
    }

    private void updateShadowOffset() {
        if (mOrientation == Configuration.ORIENTATION_LANDSCAPE || mDrawOutlineShadow) {
            // Shadow is not necessary as CCT will be always of full-height in landscape mode.
            mShadowOffset = 0;
        } else {
            mShadowOffset = mActivity.getResources().getDimensionPixelSize(
                    R.dimen.custom_tabs_shadow_offset);
        }
        View handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) handleView.getLayoutParams();
        lp.setMargins(0, mShadowOffset, 0, 0);

        // Make enough room for the handle View.
        ViewGroup.MarginLayoutParams mlp =
                (ViewGroup.MarginLayoutParams) mToolbarCoordinator.getLayoutParams();
        mlp.setMargins(0, mHandleHeight + mShadowOffset, 0, 0);
        mToolbarCoordinator.requestLayout();
    }

    private void updateWindowPos(@Px int y) {
        // Do not allow the Window to go down below the initial position or above the minimum
        // threshold capped by the status bar and (optionally) the 90%-height adjustment.
        y = MathUtils.clamp(y, getFullyExpandedYCoordinateWithAdjustment(),
                mMaxHeight - mInitialHeight - mNavbarHeight);
        WindowManager.LayoutParams attributes = mActivity.getWindow().getAttributes();
        if (attributes.y == y) return;
        attributes.y = y;
        mActivity.getWindow().setAttributes(attributes);
        assert mSpinnerView != null;
        centerSpinnerVertically((ViewGroup.LayoutParams) mSpinnerView.getLayoutParams());
    }

    private void onMoveStart() {
        showSpinnerView();
        updateNavbarVisibility(false);
    }

    private void onMoveEnd() {
        setContentsHeight();

        // TODO(crbug.com/1328555): Look into observing a view resize event to ensure the fade
        // animation can always cover the transition artifact.
        mSpinnerView.animate()
                .alpha(0f)
                .setDuration(SPINNER_FADE_DURATION_MS)
                .setListener(mSpinnerFadeoutAnimatorListener);
        updateNavbarVisibility(true);
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
            lp.setMargins(0, mToolbarView.getHeight() + mHandleHeight + mShadowOffset, 0, 0);

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
        if (mSpinnerView.getParent() == null) mContentFrame.addView(mSpinnerView);
        mSpinnerView.clearAnimation();
        mSpinnerView.setAlpha(0.f);
        mSpinnerView.setVisibility(View.VISIBLE);
        mSpinnerView.animate().alpha(1.f).setDuration(SPINNER_FADE_DURATION_MS).setListener(null);
        mSpinner.start();
    }

    private void centerSpinnerVertically(ViewGroup.LayoutParams lp) {
        int toolbarHeight = mToolbarView.getHeight();
        int cctHeight = mMaxHeight - mActivity.getWindow().getAttributes().y - toolbarHeight;
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
        lp.height = getDisplayHeight() - windowPos - mHandleHeight - mShadowOffset - mNavbarHeight;
        mCoordinatorLayout.setLayoutParams(lp);
        if (oldHeight >= 0 && lp.height != oldHeight) mOnResizedCallback.onResized(lp.height);
    }

    // Show or hide our own navigation bar.
    private void updateNavbarVisibility(boolean show) {
        if (show) {
            // No need draw its own navigation bar when it is located on the right side since
            // the system navigation bar is visible and can handle API #setNavigationBarColor.
            if (mNavbarHeight == 0) {
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
            mNavbar.animate().alpha(0.f).setDuration(NAVBAR_FADE_DURATION_MS);
        }
        showNavbarButtons(show);
    }

    // Position our own navbar where the system navigation bar which is obscured by WebContents
    // rendered over it due to Window#FLAGS_LAYOUT_NO_LIMITS would be shown.
    private void setNavbarOffset() {
        if (mCoordinatorLayout == null) return;
        int offset = mCoordinatorLayout.getLayoutParams().height + mHandleHeight + mShadowOffset;
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
        if (mNavbarHeight == 0) {
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
        if (mNavbarHeight == 0) {
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
    }

    private @Px int getMaximumPossibleHeight() {
        @Px
        int res = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Rect rect = mActivity.getWindowManager().getMaximumWindowMetrics().getBounds();
            res = Math.max(rect.width(), rect.height());
        } else {
            DisplayMetrics displayMetrics = new DisplayMetrics();
            mActivity.getWindowManager().getDefaultDisplay().getRealMetrics(displayMetrics);
            res = Math.max(displayMetrics.widthPixels, displayMetrics.heightPixels);
        }
        return res;
    }

    // TODO(ctzsm): Explore the way to use androidx.window.WindowManager or
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
        if (mIsInMultiWindowMode) {
            mActivity.getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
        } else {
            mActivity.getWindowManager().getDefaultDisplay().getRealMetrics(displayMetrics);
        }
        return displayMetrics.heightPixels;
    }

    private @Px int getFullyExpandedYCoordinate() {
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

    private @Px int getFullyExpandedYCoordinateWithAdjustment() {
        // Adding |mFullyExpandedAdjustmentHeight| to the y coordinate because the
        // coordinates system's origin is at the top left and y is growing in downward, larger y
        // means smaller height of the bottom sheet CCT.
        return getFullyExpandedYCoordinate() + mFullyExpandedAdjustmentHeight;
    }

    @Override
    public boolean changeBackgroundColorForResizing() {
        // Need to return true to keep the transparent background we set in the init step.
        return true;
    }

    @VisibleForTesting
    void setMockViewForTesting(LinearLayout navbar, ImageView spinnerView,
            CircularProgressDrawable spinner, View toolbar, View toolbarCoordinator,
            ViewGroup coordinatorLayout) {
        mNavbar = navbar;
        mSpinnerView = spinnerView;
        mSpinner = spinner;
        mToolbarView = toolbar;
        mToolbarCoordinator = toolbarCoordinator;
        mCoordinatorLayout = coordinatorLayout;
    }

    @VisibleForTesting
    int getNavbarHeightForTesting() {
        return mNavbarHeight;
    }
}
