// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotivetoolbar;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.TouchEventProvider;

/**
 * The automotive back button toolbar allows users to navigate backwards. This coordinator supports
 * the back button toolbar disappearing on fullscreen, appearing on swipe.
 */
public class AutomotiveBackButtonToolbarCoordinator {
    private final View mBackButtonToolbarForAutomotive;
    private final FullscreenManager mFullscreenManager;

    private TouchEventProvider mTouchEventProvider;
    private EdgeSwipeGestureDetector mEdgeSwipeGestureDetector;

    interface OnSwipeCallback {
        /** Handles actions required after a swipe occurs. */
        void handleSwipe();
    }

    private FullscreenManager.Observer mFullscreenObserver =
            new FullscreenManager.Observer() {
                // TODO(jtanaristy): Implement fullscreen observer
                @Override
                public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                }

                @Override
                public void onExitFullscreen(Tab tab) {
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
        mTouchEventProvider.addTouchEventObserver(mEdgeSwipeGestureDetector);
    }

    /** Handles back button toolbar visibility on a swipe. */
    private void handleSwipe() {
        // TODO(jtanaristy): implement handling a swipe
    }

    /** Destroy the Automotive Back Button Toolbar coordinator and its components. */
    public void destroy() {
        mFullscreenManager.removeObserver(mFullscreenObserver);
        mTouchEventProvider.removeTouchEventObserver(mEdgeSwipeGestureDetector);
        mFullscreenObserver = null;
        mEdgeSwipeGestureDetector = null;
    }
}
