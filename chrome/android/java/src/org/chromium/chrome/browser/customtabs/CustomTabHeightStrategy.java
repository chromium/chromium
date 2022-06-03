// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;

/**
 * The default strategy for setting the height of the custom tab.
 */
public class CustomTabHeightStrategy {
    public static CustomTabHeightStrategy createStrategy(Activity activity, @Px int initialHeight,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            CustomTabsConnection connection, @Nullable CustomTabsSessionToken session,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        if (initialHeight <= 0) {
            return new CustomTabHeightStrategy();
        }

        return new PartialCustomTabHeightStrategy(activity, initialHeight,
                multiWindowModeStateDispatcher,
                size -> connection.onResized(session, size), lifecycleDispatcher);
    }

    /**
     * Returns false if we didn't change the Window background color, true otherwise.
     */
    public boolean changeBackgroundColorForResizing() {
        return false;
    }

    /**
     * Provide this class with the {@link CustomTabToolbar} so it can set up the strategy, and the
     * coordinator view to insert the UI "handle" for users to interact with to resize the Custom
     * Tab.
     */
    public void onToolbarInitialized(View coordinatorView, CustomTabToolbar toolbar) {}
}
