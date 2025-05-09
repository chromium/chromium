// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotivetoolbar;

import android.content.Context;
import android.os.Handler;
import android.view.View;
import android.view.ViewStub;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.FrameLayout;

import androidx.annotation.AnimRes;
import androidx.appcompat.widget.Toolbar;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.util.AutomotiveUtils;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.ui.animation.EmptyAnimationListener;

/**
 * The automotive back button toolbar allows users to navigate backwards. This coordinator supports
 * the back button toolbar disappearing on fullscreen, appearing on swipe.
 */
public class AutomotiveBackButtonToolbarCoordinator {
    /** Duration automotive back button toolbar is visible after a valid swipe */
    private static final long SHOW_TOOLBAR_ON_SWIPE_DURATION_MS = 10000;

    private final Runnable mHideToolbar =
            new Runnable() {
                @Override
                public void run() {
                    if (mIsFullscreen) {
                        assert (mEdgeSwipeGestureDetector != null);
                        mEdgeSwipeGestureDetector.setIsReadyForNewScroll(false);
                        mIsAnimationActive = true;
                        mOnSwipeAutomotiveToolbar.startAnimation(mHideOnSwipeToolbarAnimation);
                    }
                }
            };

    private final Handler mHandler = new Handler();
    private final Context mContext;
    private final View mBackButtonToolbarForAutomotive;
    private final FullscreenManager mFullscreenManager;
    private final TouchEventProvider mTouchEventProvider;
    private final BackPressManager mBackPressedManager;

    private Toolbar mOnSwipeAutomotiveToolbar;
    private EdgeSwipeGestureDetector mEdgeSwipeGestureDetector;
    private Animation mShowOnSwipeToolbarAnimation;
    private Animation mHideOnSwipeToolbarAnimation;
    private boolean mIsFullscreen;
    private final boolean mIsVerticalToolbar;
    private boolean mIsAnimationActive;

    interface OnSwipeCallback {
        /** Handles actions required after a swipe occurs. */
        void handleSwipe();

        /** Handles actions required after a back swipe occurs. */
        void handleBackSwipe();
    }

    private final OnSwipeCallback mOnSwipeCallback =
            new OnSwipeCallback() {
                @Override
                public void handleSwipe() {
                    if (mIsFullscreen
                            && !mIsAnimationActive
                            && mOnSwipeAutomotiveToolbar.getVisibility() == View.GONE) {
                        mIsAnimationActive = true;
                        mOnSwipeAutomotiveToolbar.setVisibility(View.VISIBLE);
                        mOnSwipeAutomotiveToolbar.startAnimation(mShowOnSwipeToolbarAnimation);
                        mHandler.postDelayed(mHideToolbar, SHOW_TOOLBAR_ON_SWIPE_DURATION_MS);
                    }
                }

                @Override
                public void handleBackSwipe() {
                    if (mIsFullscreen
                            && !mIsAnimationActive
                            && mOnSwipeAutomotiveToolbar.getVisibility() == View.VISIBLE) {
                        mIsAnimationActive = true;
                        mOnSwipeAutomotiveToolbar.startAnimation(mHideOnSwipeToolbarAnimation);
                        mHandler.removeCallbacks(mHideToolbar);
                    }
                }
            };

    private FullscreenManager.Observer mFullscreenObserver =
            new FullscreenManager.Observer() {
                @Override
                public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                    // TODO(https://crbug.com/376737727): Evaluate if lazy inflation is needed.
                    mTouchEventProvider.addTouchEventObserver(mEdgeSwipeGestureDetector);
                    mBackButtonToolbarForAutomotive.setVisibility(View.GONE);
                    mIsFullscreen = true;
                    mEdgeSwipeGestureDetector.setIsReadyForNewScroll(true);
                }

                @Override
                public void onExitFullscreen(Tab tab) {
                    mOnSwipeAutomotiveToolbar.setVisibility(View.GONE);
                    mHandler.removeCallbacks(mHideToolbar);
                    mTouchEventProvider.removeTouchEventObserver(mEdgeSwipeGestureDetector);
                    mBackButtonToolbarForAutomotive.setVisibility(View.VISIBLE);
                    mIsFullscreen = false;
                }
            };

    /**
     * Create the Coordinator of automotive back button toolbar that inflates and owns the view.
     *
     * @param context Context activity
     * @param automotiveBaseFrameLayout FrameLayout for the Automotive base.
     * @param fullscreenManager Used to determine if fullscreen.
     * @param touchEventProvider Used to attach touchEventObserver to view.
     * @param backPressManager Used to handle back press navigation
     */
    public AutomotiveBackButtonToolbarCoordinator(
            Context context,
            FrameLayout automotiveBaseFrameLayout,
            FullscreenManager fullscreenManager,
            TouchEventProvider touchEventProvider,
            BackPressManager backPressManager) {
        mContext = context;
        mFullscreenManager = fullscreenManager;
        mTouchEventProvider = touchEventProvider;
        mBackPressedManager = backPressManager;
        mEdgeSwipeGestureDetector = new EdgeSwipeGestureDetector(mContext, mOnSwipeCallback);
        mFullscreenManager.addObserver(mFullscreenObserver);
        mBackButtonToolbarForAutomotive =
                automotiveBaseFrameLayout.findViewById(R.id.back_button_toolbar);
        // Check if back button toolbar is vertical
        mIsVerticalToolbar = AutomotiveUtils.useVerticalAutomotiveBackButtonToolbar(context);
        setOnSwipeBackButtonToolbar(
                automotiveBaseFrameLayout.findViewById(
                        R.id.automotive_on_swipe_back_button_toolbar_stub));
    }

    private void setOnSwipeBackButtonToolbar(ViewStub onSwipeAutomotiveToolbarStub) {
        // TODO(https://crbug.com/376737727): Revisit when toolbar improvements is fully launched.
        mOnSwipeAutomotiveToolbar = (Toolbar) onSwipeAutomotiveToolbarStub.inflate();
        assert mOnSwipeAutomotiveToolbar != null;
        mOnSwipeAutomotiveToolbar.setNavigationOnClickListener(
                view -> {
                    mBackPressedManager.getCallback().handleOnBackPressed();
                });

        @AnimRes
        int showOnSwipeTransition =
                mIsVerticalToolbar ? R.anim.slide_in_left : R.anim.slide_in_down;
        mShowOnSwipeToolbarAnimation =
                AnimationUtils.loadAnimation(mContext, showOnSwipeTransition);
        mShowOnSwipeToolbarAnimation.setAnimationListener(
                new EmptyAnimationListener() {
                    @Override
                    public void onAnimationEnd(Animation animation) {
                        assert (mEdgeSwipeGestureDetector != null);
                        mEdgeSwipeGestureDetector.setIsReadyForNewScroll(true);
                        mIsAnimationActive = false;
                    }
                });

        @AnimRes
        int hideOnSwipeTransition =
                mIsVerticalToolbar ? R.anim.slide_out_left : R.anim.slide_out_down;
        mHideOnSwipeToolbarAnimation =
                AnimationUtils.loadAnimation(mContext, hideOnSwipeTransition);
        mHideOnSwipeToolbarAnimation.setAnimationListener(
                new EmptyAnimationListener() {
                    @Override
                    public void onAnimationEnd(Animation animation) {
                        mOnSwipeAutomotiveToolbar.setVisibility(View.GONE);
                        assert (mEdgeSwipeGestureDetector != null);
                        mEdgeSwipeGestureDetector.setIsReadyForNewScroll(true);
                        mIsAnimationActive = false;
                    }
                });
        // TODO(https://crbug.com/376740682): Configure back press behavior for Automotive Toolbar
        // here.
    }

    /** Destroy the Automotive Back Button Toolbar coordinator and its components. */
    public void destroy() {
        mHideOnSwipeToolbarAnimation.cancel();
        mShowOnSwipeToolbarAnimation.cancel();
        mHandler.removeCallbacks(mHideToolbar);
        mFullscreenManager.removeObserver(mFullscreenObserver);
        mTouchEventProvider.removeTouchEventObserver(mEdgeSwipeGestureDetector);
        mFullscreenObserver = null;
        mEdgeSwipeGestureDetector = null;
    }

    FullscreenManager.Observer getFullscreenObserverForTesting() {
        return mFullscreenObserver;
    }

    EdgeSwipeGestureDetector getEdgeSwipeGestureDetectorForTesting() {
        return mEdgeSwipeGestureDetector;
    }

    OnSwipeCallback getOnSwipeCallbackForTesting() {
        return mOnSwipeCallback;
    }
}
