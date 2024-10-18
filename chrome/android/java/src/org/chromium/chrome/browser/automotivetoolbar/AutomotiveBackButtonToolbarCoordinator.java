// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotivetoolbar;

import android.content.Context;
import android.os.Handler;
import android.view.View;

import androidx.annotation.VisibleForTesting;

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
                        mBackButtonToolbarForAutomotive.setVisibility(View.GONE);
                        if (mEdgeSwipeGestureDetector != null) {
                            mEdgeSwipeGestureDetector.setIsActive(false);
                        }
                    }
                }
            };

    private final Handler mHandler = new Handler();
    private final View mBackButtonToolbarForAutomotive;
    private final FullscreenManager mFullscreenManager;

    private TouchEventProvider mTouchEventProvider;
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
                    mTouchEventProvider.addTouchEventObserver(mEdgeSwipeGestureDetector);
                    mBackButtonToolbarForAutomotive.setVisibility(View.GONE);
                    mIsFullscreen = true;
                }

                @Override
                public void onExitFullscreen(Tab tab) {
                    mHandler.removeCallbacks(mHideToolbar);
                    mTouchEventProvider.removeTouchEventObserver(mEdgeSwipeGestureDetector);
                    mBackButtonToolbarForAutomotive.setVisibility(View.VISIBLE);
                    mIsFullscreen = false;
                }
            };

    /**
     * Create the Coordinator of automotive back button toolbar that owns the view.
     *
     * @param context Context activity.
     * @param backButtonToolbarForAutomotive View for the Automotive back button toolbar.
     * @param fullscreenManager Used to determine if fullscreen.
     * @param touchEventProvider Used to attach touchEventObserver to view.
     */
    public AutomotiveBackButtonToolbarCoordinator(
            Context context,
            View backButtonToolbarForAutomotive,
            FullscreenManager fullscreenManager,
            TouchEventProvider touchEventProvider) {
        mBackButtonToolbarForAutomotive = backButtonToolbarForAutomotive;
        mFullscreenManager = fullscreenManager;
        mTouchEventProvider = touchEventProvider;
        mEdgeSwipeGestureDetector = new EdgeSwipeGestureDetector(context, this::handleSwipe);
        mFullscreenManager.addObserver(mFullscreenObserver);
    }

    /** Handles back button toolbar visibility on a swipe. */
    @VisibleForTesting
    void handleSwipe() {
        if (mIsFullscreen) {
            mBackButtonToolbarForAutomotive.setVisibility(View.VISIBLE);
            mHandler.postDelayed(mHideToolbar, SHOW_TOOLBAR_ON_SWIPE_DURATION_MS);
        }
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
