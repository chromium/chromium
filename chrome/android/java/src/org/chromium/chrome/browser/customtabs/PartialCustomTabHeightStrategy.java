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
import android.graphics.drawable.InsetDrawable;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.animation.AccelerateInterpolator;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * CustomTabHeightStrategy for Partial Custom Tab. An instance of this class should be
 * owned by the CustomTabActivity.
 */
public class PartialCustomTabHeightStrategy extends CustomTabHeightStrategy
        implements ConfigurationChangedObserver, ValueAnimator.AnimatorUpdateListener {
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

    private static final int FLING_VELOCITY_PIXELS_PER_MS = 1000;

    @IntDef({HeightStatus.TOP, HeightStatus.INITIAL_HEIGHT, HeightStatus.TRANSITION})
    @Retention(RetentionPolicy.SOURCE)
    private @interface HeightStatus {
        int TOP = 0;
        int INITIAL_HEIGHT = 1;
        int TRANSITION = 2;
    }

    private final Activity mActivity;
    private final @Px int mMaxHeight;

    private final @Px int mFullyExpandedAdjustmentHeight;
    private final Integer mNavigationBarColor;
    private final Integer mNavigationBarDividerColor;
    private final OnResizedCallback mOnResizedCallback;
    private final AnimatorListener mSpinnerFadeoutAnimatorListener;
    private final int mCachedHandleHeight;
    private @Px int mInitialHeight;
    private ValueAnimator mAnimator;
    private int mShadowOffset;
    private boolean mDrawOutlineShadow;
    private @Px int mDisplayHeight;

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
    private int mToolbarColor;
    private Runnable mPositionUpdater;

    // Runnable finishing the activity after the exit animation. Non-null when PCCT is closing.
    @Nullable
    private Runnable mFinishRunnable;

    // Y offset when a dragging gesture starts.
    private int mDraggingStartY;

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
        /**
         * The base duration of the settling animation of the sheet. 218 ms is a spec for material
         * design (this is the minimum time a user is guaranteed to pay attention to something).
         */
        private static final long BASE_ANIMATION_DURATION_MS = 218;

        private static final int FLING_THRESHOLD_PX = 100;

        private GestureDetector mGestureDetector;
        private float mLastPosY;
        private float mOffsetY;
        private float mDeltaY;
        private boolean mSeenFirstMoveOrDown;
        private VelocityTracker mVelocityTracker;
        private Runnable mCloseHandler;

        public PartialCustomTabHandleStrategy(Context context) {
            mGestureDetector = new GestureDetector(context, this, ThreadUtils.getUiThreadHandler());
            mVelocityTracker = VelocityTracker.obtain();
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent event) {
            return isFullHeight() ? false : mGestureDetector.onTouchEvent(event);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (!ChromeFeatureList.sCctResizableAllowResizeByUserGesture.isEnabled()) {
                return false;
            }

            if (mStatus == HeightStatus.TRANSITION) {
                return true;
            }
            // We will get events directly even when onInterceptTouchEvent() didn't return true,
            // because the sub View tree might not want this event, so check orientation and
            // multi-window flags here again.
            if (isFullHeight()) {
                return true;
            }

            float y = event.getRawY();
            switch (MotionEventCompat.getActionMasked(event)) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_MOVE:
                    if (!mSeenFirstMoveOrDown) {
                        mSeenFirstMoveOrDown = true;
                        mVelocityTracker.clear();
                        onMoveStart();
                        mOffsetY = mActivity.getWindow().getAttributes().y - y;
                        mLastPosY = y;
                    } else {
                        mVelocityTracker.addMovement(event);
                        updateWindowPos((int) (y + mOffsetY));
                    }
                    mDeltaY = y - mLastPosY;
                    mLastPosY = y;
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    if (mSeenFirstMoveOrDown) {
                        mVelocityTracker.computeCurrentVelocity(FLING_VELOCITY_PIXELS_PER_MS);
                        float v = Math.abs(mVelocityTracker.getYVelocity());
                        int flingDist = Math.abs(v) < FLING_THRESHOLD_PX ? 0 : getFlingDistance(v);
                        int direction = (int) Math.signum(mDeltaY);
                        if (!handleAnimation(flingDist * direction)) mCloseHandler.run();
                        mSeenFirstMoveOrDown = false;
                    }
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

        /**
         * Gets the distance of a fling based on the velocity and the base animation time. This
         * formula assumes the deceleration curve is quadratic (t^2), hence the displacement formula
         * should be: displacement = initialVelocity * duration / 2.
         * @param velocity The velocity of the fling.
         * @return The distance the fling would cover.
         */
        private int getFlingDistance(float velocity) {
            // This includes conversion from seconds to ms.
            return (int) (velocity * BASE_ANIMATION_DURATION_MS / 2000f);
        }

        private boolean handleAnimation(int flingDistance) {
            int currentY = mActivity.getWindow().getAttributes().y;
            int finalY = currentY + flingDistance;
            int topY = getFullyExpandedYCoordinateWithAdjustment();
            int initialY = mDisplayHeight - mInitialHeight;
            int bottomY = mDisplayHeight - mNavbarHeight;

            int start = 0;
            int end = 0;
            boolean playAnimation = true;

            if (finalY == initialY) return false;

            if (finalY < initialY) { // Move up
                if (Math.abs(topY - finalY) < Math.abs(finalY - initialY)) {
                    start = currentY;
                    end = topY;
                    mTargetStatus = HeightStatus.TOP;
                } else {
                    start = currentY;
                    end = initialY;
                    mTargetStatus = HeightStatus.INITIAL_HEIGHT;
                }
            } else { // Move down
                // Prevents skipping intial state when swiping from the top.
                if (mStatus == HeightStatus.TOP) finalY = Math.min(initialY, finalY);

                if (Math.abs(initialY - finalY) < Math.abs(finalY - bottomY)) {
                    start = currentY;
                    end = initialY;
                    mTargetStatus = HeightStatus.INITIAL_HEIGHT;
                } else {
                    playAnimation = false;
                }
            }
            if (playAnimation) {
                mAnimator.setIntValues(start, end);
                mStatus = HeightStatus.TRANSITION;
                mAnimator.start();
            }
            return playAnimation;
        }
    }

    public PartialCustomTabHeightStrategy(Activity activity, @Px int initialHeight,
            Integer navigationBarColor, Integer navigationBarDividerColor,
            OnResizedCallback onResizedCallback, ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mMaxHeight = getMaximumPossibleHeight();
        mInitialHeight = MathUtils.clamp(
                initialHeight, mMaxHeight, (int) (mMaxHeight * MINIMAL_HEIGHT_RATIO));
        mDisplayHeight = getDisplayHeight();
        mOnResizedCallback = onResizedCallback;
        // When the flag is enabled, we make the max snap point 10% shorter, so it will only occupy
        // 90% of the height.
        mFullyExpandedAdjustmentHeight = ChromeFeatureList.sCctResizable90MaximumHeight.isEnabled()
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
        int end = getFullyExpandedYCoordinateWithAdjustment();
        mAnimator.setIntValues(start, end);
        mStatus = HeightStatus.TRANSITION;
        mTargetStatus = HeightStatus.TOP;
        mAnimator.start();
    }

    private void updatePosition() {
        if (mContentFrame == null) return;

        initializeHeight();
        updateShadowOffset();

        setContentsHeight();
        updateNavbarVisibility(true);
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
        toolbar.setHandleStrategy(new PartialCustomTabHandleStrategy(mActivity));
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
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        mNavbarHeight = getNavbarHeight();
        int maxExpandedY = getFullyExpandedYCoordinate();
        final @Px int height;

        if (isFullHeight()) {
            // Resizing by user dragging is not supported in landscape mode; no need to set
            // the status here.
            height = mDisplayHeight - maxExpandedY;
            mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        } else {
            height = mInitialHeight;
            mStatus = HeightStatus.INITIAL_HEIGHT;
            mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        }

        WindowManager.LayoutParams attributes = mActivity.getWindow().getAttributes();
        // TODO(jinsukkim): Handle multi-window mode.
        if (attributes.height == height) return;

        // We do not resize Window but just translate its vertical offset, and resize Coordinator-
        // LayoutForPointer instead. This helps us work around the round-corner bug in Android S.
        // See b/223536648.
        attributes.y = Math.max(maxExpandedY, mDisplayHeight - height);
        mActivity.getWindow().setAttributes(attributes);

        updateDragBarVisibility();
    }

    private void updateDragBarVisibility() {
        View dragBar = mActivity.findViewById(R.id.drag_bar);
        if (dragBar != null) dragBar.setVisibility(isFullHeight() ? View.GONE : View.VISIBLE);
    }

    private void updateShadowOffset() {
        if (isFullHeight() || mDrawOutlineShadow) {
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
        mlp.setMargins(0, getHandleHeight() + mShadowOffset, 0, 0);
        mToolbarCoordinator.requestLayout();
    }

    private int getHandleHeight() {
        return isFullHeight() ? 0 : mCachedHandleHeight;
    }

    private boolean isFullHeight() {
        return mOrientation == Configuration.ORIENTATION_LANDSCAPE || mIsInMultiWindowMode;
    }

    private void updateWindowPos(@Px int y) {
        // Do not allow the Window to go above the minimum threshold capped by the status
        // bar and (optionally) the 90%-height adjustment.
        y = MathUtils.clamp(y, getFullyExpandedYCoordinateWithAdjustment(), mMaxHeight);
        WindowManager.LayoutParams attributes = mActivity.getWindow().getAttributes();
        if (attributes.y == y) return;

        attributes.y = y;
        mActivity.getWindow().setAttributes(attributes);
        if (mFinishRunnable != null) return;

        // Show the spinner lazily, only when the tab is dragged _up_, which requires showing
        // more area than initial state.
        if (mStatus != HeightStatus.TRANSITION
                && (mSpinnerView == null || mSpinnerView.getVisibility() != View.VISIBLE)
                && y < mDraggingStartY) {
            showSpinnerView();
        }
        if (mSpinnerView != null) {
            centerSpinnerVertically((ViewGroup.LayoutParams) mSpinnerView.getLayoutParams());
        }
    }

    private void onMoveStart() {
        mDraggingStartY = mActivity.getWindow().getAttributes().y;
        updateNavbarVisibility(false);
    }

    private void onMoveEnd() {
        if (mFinishRunnable != null) {
            mFinishRunnable.run();
            return;
        }
        setContentsHeight();

        // TODO(crbug.com/1328555): Look into observing a view resize event to ensure the fade
        // animation can always cover the transition artifact.
        if (mSpinnerView != null && mSpinnerView.getVisibility() == View.VISIBLE) {
            mSpinnerView.animate()
                    .alpha(0f)
                    .setDuration(SPINNER_FADEOUT_DURATION_MS)
                    .setListener(mSpinnerFadeoutAnimatorListener);
        }
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
            lp.setMargins(0, mToolbarView.getHeight() + getHandleHeight() + mShadowOffset, 0, 0);
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
        mSpinnerView.animate().alpha(1.f).setDuration(SPINNER_FADEIN_DURATION_MS).setListener(null);
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

    private @Px int getFullyExpandedYCoordinate() {
        return getStatusBarHeight();
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

    @Override
    public void handleCloseAnimation(Runnable finishRunnable) {
        if (mFinishRunnable != null) return;

        mFinishRunnable = finishRunnable;

        int start = mActivity.getWindow().getAttributes().y;
        int end = mDisplayHeight - mNavbarHeight;

        if (isFullHeight()) {
            mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        }
        mAnimator.setDuration(
                mActivity.getResources().getInteger(android.R.integer.config_mediumAnimTime));
        mAnimator.setIntValues(start, end);
        mAnimator.setInterpolator(new AccelerateInterpolator());
        mAnimator.start();
    }

    @Override
    public boolean canDrawOutsideScreen() {
        return !isFullHeight();
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
}
