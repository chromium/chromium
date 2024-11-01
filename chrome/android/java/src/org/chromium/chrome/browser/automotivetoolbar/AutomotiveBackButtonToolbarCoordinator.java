// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotivetoolbar;

import android.content.Context;
import android.os.Handler;
import android.view.View;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.Toolbar;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.TouchEventProvider;

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
                        mOnSwipeAutomotiveToolbar.setVisibility(View.GONE);
                        if (mEdgeSwipeGestureDetector != null) {
                            mEdgeSwipeGestureDetector.setIsActive(false);
                        }
                    }
                }
            };

    private final Handler mHandler = new Handler();
    private final View mBackButtonToolbarForAutomotive;
    private final FullscreenManager mFullscreenManager;
    private final TouchEventProvider mTouchEventProvider;
    private final BackPressManager mBackPressedManager;

    private Toolbar mOnSwipeAutomotiveToolbar;
    private EdgeSwipeGestureDetector mEdgeSwipeGestureDetector;
    private boolean mIsFullscreen;

    interface OnSwipeCallback {
        /** Handles actions required after a swipe occurs. */
        void handleSwipe();
    }

    private FullscreenManager.Observer mFullscreenObserver =
            new FullscreenManager.Observer() {
                @Override
                public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                    // TODO(https://crbug.com/376737727): Evaluate if lazy inflation is needed.
                    mTouchEventProvider.addTouchEventObserver(mEdgeSwipeGestureDetector);
                    mBackButtonToolbarForAutomotive.setVisibility(View.GONE);
                    mIsFullscreen = true;
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
        mFullscreenManager = fullscreenManager;
        mTouchEventProvider = touchEventProvider;
        mBackPressedManager = backPressManager;
        mEdgeSwipeGestureDetector = new EdgeSwipeGestureDetector(context, this::handleSwipe);
        mFullscreenManager.addObserver(mFullscreenObserver);
        mBackButtonToolbarForAutomotive =
                automotiveBaseFrameLayout.findViewById(R.id.back_button_toolbar);
        setOnSwipeBackButtonToolbar(
                automotiveBaseFrameLayout.findViewById(
                        R.id.automotive_on_swipe_back_button_toolbar_stub));
    }

    /** Handles back button toolbar visibility on a swipe. */
    @VisibleForTesting
    void handleSwipe() {
        if (mIsFullscreen) {
            mOnSwipeAutomotiveToolbar.setVisibility(View.VISIBLE);
            mHandler.postDelayed(mHideToolbar, SHOW_TOOLBAR_ON_SWIPE_DURATION_MS);
        }
    }

    private void setOnSwipeBackButtonToolbar(ViewStub onSwipeAutomotiveToolbarStub) {
        // TODO(https://crbug.com/376737727): Revisit when toolbar improvements is fully launched.
        mOnSwipeAutomotiveToolbar = (Toolbar) onSwipeAutomotiveToolbarStub.inflate();
        assert mOnSwipeAutomotiveToolbar != null;
        mOnSwipeAutomotiveToolbar.setNavigationOnClickListener(
                view -> {
                    mBackPressedManager.getCallback().handleOnBackPressed();
                });
        // TODO(https://crbug.com/376740682): Configure back press behavior for Automotive Toolbar
        // here.
    }

    /** Destroy the Automotive Back Button Toolbar coordinator and its components. */
    public void destroy() {
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
}
