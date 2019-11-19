// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

/**
 * A factory for creating an {@link AppMenuCoordinator}.
 */
public class AppMenuCoordinatorFactory {
    private AppMenuCoordinatorFactory() {}

    /**
     * Create a new AppMenuCoordinator.
     * @param context The activity context.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the containing
     *         activity.
     * @param buttonDelegate The {@link MenuButtonDelegate} for the containing activity.
     * @param appMenuDelegate The {@link AppMenuDelegate} for the containing activity.
     * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the containing
     *         activity.
     * @param hardwareButtonAnchorView The {@link View} used as an anchor for the menu when it is
     *            displayed using a hardware butt
     */
    public static AppMenuCoordinator createAppMenuCoordinator(Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MenuButtonDelegate buttonDelegate, AppMenuDelegate appMenuDelegate, View decorView,
            View hardwareButtonAnchorView) {
        return new AppMenuCoordinatorImpl(context, activityLifecycleDispatcher, buttonDelegate,
                appMenuDelegate, decorView, hardwareButtonAnchorView);
    }
}
