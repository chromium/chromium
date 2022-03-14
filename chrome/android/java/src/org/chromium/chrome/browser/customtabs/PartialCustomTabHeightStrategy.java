// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.GestureDetector;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.widget.ImageView;

import androidx.annotation.IntDef;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.MotionEventCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;

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
    private static final int DURATION_MS = 200;

    @IntDef({HeightStatus.TOP, HeightStatus.INITIAL_HEIGHT, HeightStatus.TRANSITION})
    @Retention(RetentionPolicy.SOURCE)
    private @interface HeightStatus {
        int TOP = 0;
        int INITIAL_HEIGHT = 1;
        int TRANSITION = 2;
    }

    private Activity mActivity;
    private @Px int mInitialHeight;
    private final @Px int mMaxHeight;
    private final @Px int mFullyExpandedAdjustmentHeight;
    private ValueAnimator mAnimator;

    private @HeightStatus int mStatus = HeightStatus.INITIAL_HEIGHT;
    private @HeightStatus int mTargetStatus;

    private int mOrientation;
    private boolean mIsInMultiWindowMode;

    private final OnResizedCallback mOnResizedCallback;

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
                        updateWindowHeight((int) (mInitialY - mLastDownPosY + y));
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

    public PartialCustomTabHeightStrategy(Activity activity, @Px int initialHeight,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            OnResizedCallback onResizedCallback, ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mMaxHeight = getMaximumPossibleHeight();
        mInitialHeight = MathUtils.clamp(
                initialHeight, mMaxHeight, (int) (mMaxHeight * MINIMAL_HEIGHT_RATIO));
        mOnResizedCallback = onResizedCallback;
        // When the flag is enabled, we make the max snap point 10% shorter, so it will only occupy
        // 90% of the height.
        mFullyExpandedAdjustmentHeight =
                CachedFeatureFlags.isEnabled(ChromeFeatureList.CCT_RESIZABLE_90_MAXIMUM_HEIGHT)
                ? (int) ((mMaxHeight - getFullyExpandedYCoordinate()) * EXTRA_HEIGHT_RATIO)
                : 0;

        mAnimator = new ValueAnimator();
        mAnimator.setDuration(DURATION_MS);
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

        initializeHeight();
    }

    @Override
    public boolean changeBackgroundColorForResizing() {
        GradientDrawable background =
                (GradientDrawable) mActivity.getWindow().getDecorView().getBackground();
        if (background == null) {
            return false;
        }

        final int color = mActivity.getColor(R.color.resizing_background_color);
        ((GradientDrawable) background.mutate()).setColor(color);
        return true;
    }

    @Override
    public void onToolbarInitialized(View coordinatorView, CustomTabToolbar toolbar) {
        roundCorners(coordinatorView, toolbar);

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
        }
    }

    // ValueAnimator.AnimatorUpdateListener implementation.
    @Override
    public void onAnimationUpdate(ValueAnimator valueAnimator) {
        int value = (int) valueAnimator.getAnimatedValue();
        updateWindowHeight(value);
    }

    private void roundCorners(View coordinator, CustomTabToolbar toolbar) {
        final float radius = mActivity.getResources().getDimensionPixelSize(
                R.dimen.custom_tabs_top_corner_round_radius);

        // Inflate the handle View.
        ViewStub handleViewStub = mActivity.findViewById(R.id.custom_tabs_handle_view_stub);
        handleViewStub.inflate();
        ImageView handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);

        // Pass the handle View to CustomTabToolbar for background color management.
        toolbar.setHandleView(handleView);

        // Make enough room for the handle View.
        ViewGroup.MarginLayoutParams mlp =
                (ViewGroup.MarginLayoutParams) coordinator.getLayoutParams();
        mlp.setMargins(0, Math.round(radius), 0, 0);
        coordinator.requestLayout();

        mActivity.getWindow().setBackgroundDrawableResource(
                R.drawable.custom_tabs_handle_view_shape);
    }

    private void initializeHeight() {
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        final @Px int heightInPhysicalPixels;
        if (mOrientation == Configuration.ORIENTATION_LANDSCAPE) {
            heightInPhysicalPixels = getDisplayHeight() - getFullyExpandedYCoordinate();
        } else {
            heightInPhysicalPixels = mInitialHeight;
            mStatus = HeightStatus.INITIAL_HEIGHT;
        }

        WindowManager.LayoutParams attributes = mActivity.getWindow().getAttributes();
        // TODO(ctzsm): Consider to handle rotation and resizing when entering/exiting multi-window
        // mode.
        if (attributes.height == heightInPhysicalPixels) return;

        attributes.height = heightInPhysicalPixels;
        attributes.gravity = Gravity.BOTTOM;
        mActivity.getWindow().setAttributes(attributes);

        View decorView = mActivity.getWindow().getDecorView();
        // Hide the navigation bar to reduce flickering at the bottom. Because FLAG_LAYOUT_NO_LIMITS
        // flag we used in onMoveStart() will make the navigation bar background color to be
        // transparent, so there is a transient stage.
        WindowCompat.setDecorFitsSystemWindows(mActivity.getWindow(), true);
        WindowInsetsControllerCompat controller =
                WindowCompat.getInsetsController(mActivity.getWindow(), decorView);
        controller.hide(WindowInsetsCompat.Type.navigationBars());
        controller.setSystemBarsBehavior(
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
    }

    private void updateWindowHeight(@Px int y) {
        y = MathUtils.clamp(
                y, getFullyExpandedYCoordinateWithAdjustment(), mMaxHeight - mInitialHeight);
        WindowManager.LayoutParams attributes = mActivity.getWindow().getAttributes();
        if (attributes.y == y) {
            return;
        }
        attributes.y = y;
        mActivity.getWindow().setAttributes(attributes);
    }

    private void onMoveStart() {
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);

        WindowManager.LayoutParams attributes = mActivity.getWindow().getAttributes();

        attributes.y = mMaxHeight - attributes.height;
        attributes.height = mMaxHeight;
        attributes.gravity = Gravity.NO_GRAVITY;
        mActivity.getWindow().setAttributes(attributes);
    }

    private void onMoveEnd() {
        mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);

        WindowManager.LayoutParams attributes = mActivity.getWindow().getAttributes();

        attributes.height = mMaxHeight - attributes.y;
        attributes.y = 0;
        attributes.gravity = Gravity.BOTTOM;
        mActivity.getWindow().setAttributes(attributes);

        mOnResizedCallback.onResized(attributes.height);
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
}
