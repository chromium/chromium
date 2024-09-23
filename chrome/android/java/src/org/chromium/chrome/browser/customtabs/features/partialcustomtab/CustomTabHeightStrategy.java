// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.app.Activity;
import android.view.View;

import androidx.annotation.Px;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.findinpage.FindToolbarObserver;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.TouchEventProvider;

/** The default strategy for setting the height of the custom tab. */
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
                int left,
                int top,
                int right,
                int bottom,
                @CustomTabsCallback.ActivityLayoutState int state);
    }

    public static CustomTabHeightStrategy createStrategy(
            Activity activity,
            BrowserServicesIntentDataProvider intentData,
            Supplier<TouchEventProvider> touchEventProvider,
            Supplier<Tab> tab,
            CustomTabsConnection connection,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            FullscreenManager fullscreenManager,
            boolean isTablet) {
        if (!intentData.isPartialCustomTab()) {
            return new CustomTabHeightStrategy();
        }

        CustomTabsSessionToken session = intentData.getSession();
        OnResizedCallback resizeCallback =
                (height, width) -> connection.onResized(session, height, width);
        OnActivityLayoutCallback layoutCallback =
                (left, top, right, bottom, state) ->
                        connection.onActivityLayout(session, left, top, right, bottom, state);
        return new PartialCustomTabDisplayManager(
                activity,
                intentData,
                touchEventProvider,
                tab,
                resizeCallback,
                layoutCallback,
                lifecycleDispatcher,
                fullscreenManager,
                isTablet);
    }

    /**
     * @see {@link org.chromium.chrome.browser.lifecycle.InflationObserver#onPostInflationStartup()}
     */
    public void onPostInflationStartup() {}

    /** Returns false if we didn't change the Window background color, true otherwise. */
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

    /** Destroy the height strategy object. */
    public void destroy() {}
}
