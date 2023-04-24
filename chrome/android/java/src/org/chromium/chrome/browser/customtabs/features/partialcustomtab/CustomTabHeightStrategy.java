// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.app.Activity;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ActivityLayoutState;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.findinpage.FindToolbarObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

/**
 * The default strategy for setting the height of the custom tab.
 */
public class CustomTabHeightStrategy implements FindToolbarObserver {
    /** A callback to be called once the Custom Tab has been resized. */
    interface OnResizedCallback {
        /** The Custom Tab has been resized. */
        void onResized(int height, int width);
    }

    /** A callback to be called once the Custom Tab's layout has changed. */
    interface OnActivityLayoutCallback {
        /** The Custom Tab's layout has changed. */
        void onActivityLayout(
                int left, int top, int right, int bottom, @ActivityLayoutState int state);
    }

    public static CustomTabHeightStrategy createStrategy(Activity activity, @Px int initialHeight,
            @Px int initialWidth, int breakPointDp, boolean isPartialCustomTabFixedHeight,
            CustomTabsConnection connection, @Nullable CustomTabsSessionToken session,
            ActivityLifecycleDispatcher lifecycleDispatcher, FullscreenManager fullscreenManager,
            boolean isTablet, boolean interactWithBackground, boolean showMaximizeButton,
            int decorationType, int sideSheetPosition, int sideSheetAnimation,
            int roundedCornersPosition) {
        if (initialHeight <= 0
                && (!ChromeFeatureList.sCctResizableSideSheet.isEnabled() || initialWidth <= 0)) {
            return new CustomTabHeightStrategy();
        }

        if (ChromeFeatureList.sCctResizableSideSheet.isEnabled()) {
            return new PartialCustomTabDisplayManager(activity, initialHeight, initialWidth,
                    breakPointDp, isPartialCustomTabFixedHeight,
                    (height, width)
                            -> connection.onResized(session, height, width),
                    (left, top, right, bottom, state)
                            -> connection.onActivityLayout(
                                    session, left, top, right, bottom, state),
                    lifecycleDispatcher, fullscreenManager, isTablet, interactWithBackground,
                    showMaximizeButton, decorationType, sideSheetPosition, sideSheetAnimation,
                    roundedCornersPosition);
        } else {
            return new PartialCustomTabBottomSheetStrategy(activity, initialHeight,
                    isPartialCustomTabFixedHeight,
                    (height, width)
                            -> connection.onResized(session, height, width),
                    (left, top, right, bottom, state)
                            -> connection.onActivityLayout(
                                    session, left, top, right, bottom, state),
                    lifecycleDispatcher, fullscreenManager, isTablet, interactWithBackground, false,
                    new PartialCustomTabHandleStrategyFactory());
        }
    }

    /**
     * @see {@link org.chromium.chrome.browser.lifecycle.InflationObserver#onPostInflationStartup()}
     */
    public void onPostInflationStartup() {}

    /**
     * Returns false if we didn't change the Window background color, true otherwise.
     */
    public boolean changeBackgroundColorForResizing() {
        return false;
    }

    /**
     * Provide this class with the required views and values so it can set up the strategy.
     *
     * @param coordinatorView Coordinator view to insert the UI handle for the users to resize the
     *                        custom tab.
     * @param toolbar The {@link CustomTabToolbar} to set up the strategy.
     * @param toolbarCornerRadius The custom tab corner radius in pixels.
     */
    public void onToolbarInitialized(
            View coordinatorView, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {}

    /**
     * @see {@link BaseCustomTabRootUiCoordinator#handleCloseAnimation()}
     * @return {@code true} if the animation will be performed.
     */
    public boolean handleCloseAnimation(Runnable finishRunnable) {
        throw new IllegalStateException(
                "Custom close animation should be performed only on partial CCT.");
    }

    /**
     * Set the scrim value to apply to partial CCT UI.
     * @param scrimFraction Scrim fraction.
     */
    public void setScrimFraction(float scrimFraction) {}

    // FindToolbarObserver implementation.

    @Override
    public void onFindToolbarShown() {}

    @Override
    public void onFindToolbarHidden() {}

    /**
     * Destroy the height strategy object.
     */
    public void destroy() {}
}
